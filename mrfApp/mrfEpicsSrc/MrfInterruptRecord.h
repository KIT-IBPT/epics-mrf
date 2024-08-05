/*
 * Copyright 2016-2024 aquenos GmbH.
 * Copyright 2016-2024 Karlsruhe Institute of Technology.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * This software has been developed by aquenos GmbH on behalf of the
 * Karlsruhe Institute of Technology's Institute for Beam Physics and
 * Technology.
 *
 * This software contains code originally developed by aquenos GmbH for
 * the s7nodave EPICS device support. aquenos GmbH has relicensed the
 * affected poritions of code from the s7nodave EPICS device support
 * (originally licensed under the terms of the GNU GPL) under the terms
 * of the GNU LGPL version 3 or newer.
 */

#ifndef ANKA_MRF_EPICS_INTERRUPT_RECORD_H
#define ANKA_MRF_EPICS_INTERRUPT_RECORD_H

#include <cstdint>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>

extern "C" {
#include <dbCommon.h>
#include <dbScan.h>
}

#include <MrfMemoryAccess.h>

#include "MrfDeviceRegistry.h"
#include "MrfInterruptRecordAddress.h"
#include "mrfEpicsError.h"

namespace anka {
namespace mrf {
namespace epics {

/**
 * Class template that serves as a base for all record types that deal with
 * interrupts generated by MRF devices.
 */
template<typename RecordTypeParam>
class MrfInterruptRecord {

public:

  /**
   * Type of data structure used by the supported record.
   */
  using RecordType = RecordTypeParam;

  /**
   * Processes a request to enable or disable the I/O Intr mode.
   */
  void getInterruptInfo(int command, IOSCANPVT *iopvt);

  /**
   * Called each time the record is processed. Usually, a record of this type is
   * put into "I/O Intr" mode. In this case, processing of the record is
   * triggered when an interrupt occurs. The interrupt event is written to an
   * internal queue and fetched from this queue when the record is processed.
   *
   * When the record does not operate in "I/O Intr" mode, the record is updated
   * with the latest value that was received as part of an interrupt event. When
   * no interrupt event has happened since the last time the record was
   * processed, the record is updated with a value of zero.
   *
   * In either case, this method calls {@link writeRecordValue} so that the
   * record-type specific implementation can actually update the record's value.
   */
  virtual void processRecord();

protected:

  /**
   * Constructor. The record is stored in an attribute of this class and
   * is used by all methods which want to access record fields.
   */
  MrfInterruptRecord(RecordType *record);

  /**
   * Destructor.
   */
  virtual ~MrfInterruptRecord();

  /**
   * Returns the device represented by this record. The name of the device is
   * extracted from the input address field of the record.
   */
  inline std::shared_ptr<MrfConsistentMemoryAccess> getDevice() const {
    return device;
  }

  /**
   * Returns a pointer to the structure that holds the actual EPICS record.
   * Always returns a valid pointer.
   */
  inline RecordType *getRecord() const {
    return record;
  }

  /**
   * Returns the mask that should be applied to the interrupt flags. This mask
   * is extracted from the address field of the record. A mask can be used to
   * make the record sensitive to certain interrupt sources only. Interrupts
   * that do not have a flag present that is also set in the mask do not trigger
   * processing of the record. Those flags are not visible in the record's value
   * either (they are always set to zero).
   */
  inline std::uint32_t getInterruptFlagsMask() const {
    return interruptFlagsMask;
  }

  /**
   * Updates the record's value with the specified value.
   */
  virtual void writeRecordValue(std::uint32_t value) = 0;

private:

  /**
   * Interrupt listener used by the {@link MrfRecordAddress}.
   */
  class InterruptListenerImpl: public MrfMemoryAccess::InterruptListener {

  public:

    InterruptListenerImpl(MrfInterruptRecord &recordDeviceSupport) :
        recordDeviceSupport(recordDeviceSupport) {
    }

    void operator()(std::uint32_t interruptFlags) {
      bool doScanIoRequest;
      // We have to hold the mutex while accessing the interruptFlagsQueue and
      // the interruptModeEnabled flag.
      {
        std::lock_guard<std::mutex> lock(recordDeviceSupport.mutex);
        // We mask the interrupt flags with the user-configurable mask. If none
        // of the bits that is set in the mask is also set in the interrupt
        // flags, we want to ignore this interrupt.
        interruptFlags &= recordDeviceSupport.interruptFlagsMask;
        if (interruptFlags == 0) {
          return;
        }
        // We only want to call scanIoRequest(...) when the queue is empty
        // before we add an event and we are in the I/O Intr mode.
        doScanIoRequest = recordDeviceSupport.interruptModeEnabled
            && recordDeviceSupport.interruptFlagsQueue.empty();
        // We want to queue a maximum of 1024 interrupts. This allows all
        // interrupts to be processed when record processing is paused for a
        // short moment, but it also ensures that the queue does not grow
        // without bounds when the record processing simply cannot keep up with
        // the fast pace of the interrupts. When not operating in "I/O Intr"
        // mode, we only keep one notification and discard all other ones. This
        // makes sense because a record in scan mode is usually expected to
        // reflect the current state at the time of processing.
        if (recordDeviceSupport.interruptModeEnabled) {
          while (recordDeviceSupport.interruptFlagsQueue.size() >= 1024) {
            recordDeviceSupport.interruptFlagsQueue.pop();
            errorExtendedPrintf(
                "%s Interrupt queue overflow. Interrupt events have been lost. Typically, this happens when interrupts occur at a rate that is so high that the record cannot be processed at the same rate.",
                recordDeviceSupport.record->name);
          }
        } else {
          while (!recordDeviceSupport.interruptFlagsQueue.empty()) {
            recordDeviceSupport.interruptFlagsQueue.pop();
          }
        }
        recordDeviceSupport.interruptFlagsQueue.push(interruptFlags);
      }
      // We call scanIoRequest() after releasing the mutex. It should be safe to
      // call it while holding the mutex because it looks like the record
      // processing is actually not triggered while holding the same mutex (and
      // thus there cannot be a dead lock). On the other hand, there could be a
      // dead lock when this implementation changes. More importantly, calling
      // scanIoRequest(...) while holding the mutex would only have the
      // advantage that we would never call it when the interrupt mode has been
      // disabled. However, as the actual processing is done asynchronously, it
      // could still happen that interrupt mode is disabled between calling it
      // and the processing being triggered. For this reason, there is no
      // downside to releasing the mutex first.
      if (doScanIoRequest) {
        scanIoRequest(recordDeviceSupport.ioScanPvt);
      }
    }

  private:

    // Storing a reference to the record device-support looks unsafe and in
    // fact it would if the record device-support was ever going to be
    // destroyed. However, EPICS never destroys the device support, so we should
    // be safe.
    MrfInterruptRecord &recordDeviceSupport;

  };

  // We do not want to allow copy or move construction or assignment.
  MrfInterruptRecord(const MrfInterruptRecord &) = delete;
  MrfInterruptRecord(MrfInterruptRecord &&) = delete;
  MrfInterruptRecord &operator=(const MrfInterruptRecord &) = delete;
  MrfInterruptRecord &operator=(MrfInterruptRecord &&) = delete;

  /**
   * Pointer to the underlying device.
   */
  std::shared_ptr<MrfConsistentMemoryAccess> device;

  /**
   * Bit mask applied to the interrupt flags. Only those interrupt flags that
   * are also set in the mask are considered relevant.
   */
  std::uint32_t interruptFlagsMask;

  /**
   * Mutex protecting the {@link interruptFlagsQueue} and the
   * {@link interruptModeEnabled} flag.
   */
  std::mutex mutex;

  /**
   * Queue storing the interrupt flags that arrived with interrupt events. The
   * interrupt listener adds interrupts to the queue and triggers processing of
   * the record. When the record is processed, the oldest element is taken from
   * the queue and put into the record.
   */
  std::queue<std::uint32_t> interruptFlagsQueue;

  /**
   * Interrupt listener that is called by the device each time an interrupt
   * occurs.
   */
  std::shared_ptr<InterruptListenerImpl> interruptListener;

  /**
   * Flag indicating whether the record is operating in the "I/O Intr" mode.
   */
  bool interruptModeEnabled;

  /**
   * Record this device support has been instantiated for.
   */
  RecordType *record;

  /**
   * Data structure used by in order to schedule processing because of an
   * interrupt event.
   */
  ::IOSCANPVT ioScanPvt;

};

template<typename RecordType>
void MrfInterruptRecord<RecordType>::getInterruptInfo(int command,
    IOSCANPVT *iopvt) {
  if (!device->supportsInterrupts()) {
    // By setting iopvt to null, we can signal that I/O Intr mode is not
    // supported for this record.
    *iopvt = NULL;
    return;
  }
  // We have to hold the mutex while accessing the interruptFlagsQueue and while
  // modifying the interruptModeEnabled flag. However, we do not want to hold
  // it while scheduling a callback. We do not have to either, because the
  // interrupt mode cannot change while we are in this method (EPICS holds the
  // record mutex while calling this method).
  bool triggerProcessing = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (command == 0) {
      interruptModeEnabled = true;
      // If there are interrupt notifications in the queue, we have to schedule
      // a processing of the record. When the queue is not empty, the interrupt
      // handler will not trigger a processing, so the record would never be
      // processed.
      triggerProcessing = !this->interruptFlagsQueue.empty();
    } else {
      interruptModeEnabled = false;
    }
  }
  *iopvt = this->ioScanPvt;
  if (triggerProcessing) {
    scanIoRequest(this->ioScanPvt);
  }
}

template<typename RecordType>
void MrfInterruptRecord<RecordType>::processRecord() {
  std::uint32_t interruptFlags;
  bool doScanIoRequest;
  {
    // Access to the interruptFlagsQueue is protected by the mutex.
    std::lock_guard<std::mutex> lock(mutex);
    if (interruptFlagsQueue.empty()) {
      // Processing the record while the queue is empty can only happen when
      // PINI is set or the record operates in scan mode. We set the record's
      // value to zero in order to indicate that no interrupts are pending.
      interruptFlags = 0;
      doScanIoRequest = false;
    } else {
      interruptFlags = interruptFlagsQueue.front();
      interruptFlagsQueue.pop();
      // If there are more pending interrupts, we have to process the record
      // again.
      doScanIoRequest = !interruptFlagsQueue.empty() && interruptModeEnabled;
    }
  }
  writeRecordValue(interruptFlags);
  // If the record is in I/O Intr mode and more interrupt notifications are
  // pending, we have to schedule another processing because the interrupt
  // listener does not schedule a processing when the queue is not empty before
  // it is called.
  // We do this without holding the mutex in order to avoid an (unlikely) dead
  // lock (depending on how EPICS deals with locks). This should still be safe
  // because this method should only be called while holding the record lock and
  // this means that getInterruptInfo() (which might modify the
  // interruptModeEnabled flag) should not be called concurrently.
  if (doScanIoRequest) {
    scanIoRequest(ioScanPvt);
  }
}

template<typename RecordType>
MrfInterruptRecord<RecordType>::MrfInterruptRecord(RecordType *record) :
    interruptModeEnabled(false), record(record) {
  if (this->record->inp.type != INST_IO) {
    throw std::runtime_error(
        "Invalid device address. Maybe mixed up INP/OUT or forgot '@'?");
  }
  MrfInterruptRecordAddress recordAddress(
      this->record->inp.value.instio.string == nullptr ?
          "" : record->inp.value.instio.string);
  this->device = MrfDeviceRegistry::getInstance().getDevice(
      recordAddress.getDeviceId());
  if (!this->device) {
    throw std::runtime_error(
        std::string("Could not find device ") + recordAddress.getDeviceId()
            + ".");
  }
  if (!this->device->supportsInterrupts()) {
    throw std::runtime_error(
        std::string("The device ") + recordAddress.getDeviceId()
            + " does not support interrupts.");
  }
  this->interruptFlagsMask = recordAddress.getInterruptFlagsMask();
  ::scanIoInit(&ioScanPvt);
  // The interrupt listener stores a reference to this object. For this reason
  // we create it after we can be sure that this constructor will not throw an
  // exception and thus this object will stay available.
  this->interruptListener = std::make_shared<InterruptListenerImpl>(*this);
  this->device->addInterruptListener(interruptListener);
}

template<typename RecordType>
MrfInterruptRecord<RecordType>::~MrfInterruptRecord() {
  // This is not sufficient because the interrupt listener might be called
  // asynchronously and it will then use the invalid reference to this object,
  // but it is better than doing nothing. Usually, this destructor should not be
  // called anyway because the record device support is never destroyed after
  // having been created.
  if (this->interruptListener) {
    this->device->removeInterruptListener(interruptListener);
  }
}

} // namespace epics
} // namespace mrf
} // namespace anka

#endif // ANKA_MRF_EPICS_INTERRUPT_RECORD_H
