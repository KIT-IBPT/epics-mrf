/*
 * Copyright 2015-2022 aquenos GmbH.
 * Copyright 2015-2022 Karlsruhe Institute of Technology.
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

#ifndef ANKA_MRF_EPICS_OUTPUT_RECORD_H
#define ANKA_MRF_EPICS_OUTPUT_RECORD_H

#include <alarm.h>
#include <recGbl.h>

#include "MrfRecord.h"
#include "mrfEpicsError.h"

namespace anka {
namespace mrf {
namespace epics {

/**
 * Base class for most device support classes belonging to EPICS input records.
 */
template<typename RecordType>
class MrfOutputRecord: public MrfRecord<RecordType> {

protected:

  /**
   * Creates an instance of the device support class for the specified record
   * instance.
   */
  MrfOutputRecord(RecordType *record);

  /**
   * Destructor.
   */
  virtual ~MrfOutputRecord() {
  }

  /**
   * Initializes the records value with the current value read from the device.
   * If the record address specifies that no initialization is desired, the
   * initialization is skipped. This is not part of the constructor because
   * the virtual {@link #writeRecordValue(std::uint32_t)} method has to be
   * called which is not possible from the base constructor.
   */
  void initializeValue();

  /**
   * Reads and returns the record's current value.
   */
  virtual std::uint32_t readRecordValue() = 0;

  virtual void processPrepare();

  virtual void processComplete();

private:

  template<typename T>
  struct CallbackImpl: MrfMemoryAccess::Callback<T> {
    CallbackImpl(MrfOutputRecord &record);
    void success(std::uint32_t address, T value);
    void failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
        const std::string &details);

    // In EPICS, records are never destroyed. Therefore, we can safely keep a
    // reference to the device support object.
    MrfOutputRecord &record;
  };

  // We do not want to allow copy or move construction or assignment.
  MrfOutputRecord(const MrfOutputRecord &) = delete;
  MrfOutputRecord(MrfOutputRecord &&) = delete;
  MrfOutputRecord &operator=(const MrfOutputRecord &) = delete;
  MrfOutputRecord &operator=(MrfOutputRecord &&) = delete;

  bool writeSuccessful;
  std::uint32_t writeRequestValue;
  std::uint32_t writeReplyValue;
  std::string writeErrorMessage;

};

template<typename RecordType>
MrfOutputRecord<RecordType>::MrfOutputRecord(RecordType *record) :
    MrfRecord<RecordType>(record, record->out), writeSuccessful(false), writeRequestValue(
        0), writeReplyValue(0) {
}

template<typename RecordType>
void MrfOutputRecord<RecordType>::initializeValue() {
  if (this->getRecordAddress().isReadOnInit()) {
    // We try to read the value from the device, so that we can initialize the
    // record’s value. If this fails, we do not let the exception bubble up,
    // because we still want the record to be initialized (so that it can be
    // processed later). In this case, the record will simply stay in an
    // undefined state (associated with an invalid alarm) until it is
    // successfully processed for the first time.
    try {
      std::shared_ptr<MrfMemoryCache> deviceCache =
          MrfDeviceRegistry::getInstance().getDeviceCache(
              this->getRecordAddress().getDeviceId());
      if (!deviceCache) {
        throw std::runtime_error(
            std::string("Could not find cache for device ")
                + this->getRecordAddress().getDeviceId() + ".");
      }
      switch (this->getRecordAddress().getDataType()) {
      case MrfRecordAddress::DataType::uInt16:
        this->writeRecordValue(
            this->convertFromDevice(
                deviceCache->readUInt16(
                    this->getRecordAddress().getMemoryAddress())));
        break;
      case MrfRecordAddress::DataType::uInt32:
        this->writeRecordValue(
            this->convertFromDevice(
                deviceCache->readUInt32(
                    this->getRecordAddress().getMemoryAddress())));
        break;
      }
    } catch (std::exception &e) {
      errorExtendedPrintf(
          "%s Reading initial value from device failed: %s",
          this->getRecord()->name,
          e.what());
      return;
    } catch (...) {
      errorExtendedPrintf(
          "%s Reading initial value from device failed: Unknown error.",
          this->getRecord()->name);
      return;
    }
    // The record's value has been initialized, therefore it is not undefined
    // any longer.
    this->getRecord()->udf = false;
    // We have to reset the alarm state explicitly, so that the record is not
    // marked as invalid. This is not optimal because the record will not be
    // placed in an alarm state if the value would usually trigger an alarm.
    // However, alarms on output records are uncommon and we do not use them
    // for the EVG or EVR, so this is fine. We also update the time stamp so
    // that it represents the current time.
    recGblGetTimeStamp(this->getRecord());
    recGblResetAlarms(this->getRecord());
  }
}

template<typename RecordType>
void MrfOutputRecord<RecordType>::processPrepare() {
  this->writeRequestValue = this->convertToDevice(this->readRecordValue());
  switch (this->getRecordAddress().getDataType()) {
  case MrfRecordAddress::DataType::uInt16: {
    auto callback = std::make_shared<CallbackImpl<std::uint16_t>>(*this);
    if (this->getRecordAddress().isZeroOtherBits()
        || this->getMask() == 0xffff) {
      this->getDevice()->writeUInt16(
          this->getRecordAddress().getMemoryAddress(), this->writeRequestValue,
          callback);
    } else {
      this->getDevice()->writeUInt16(
          this->getRecordAddress().getMemoryAddress(), this->writeRequestValue,
          this->getMask(), callback);
    }
    break;
  }
  case MrfRecordAddress::DataType::uInt32: {
    auto callback = std::make_shared<CallbackImpl<std::uint32_t>>(*this);
    if (this->getRecordAddress().isZeroOtherBits()
        || this->getMask() == 0xffffffff) {
      this->getDevice()->writeUInt32(
          this->getRecordAddress().getMemoryAddress(), this->writeRequestValue,
          callback);
    } else {
      this->getDevice()->writeUInt32(
          this->getRecordAddress().getMemoryAddress(), this->writeRequestValue,
          this->getMask(), callback);
    }
    break;
  }
  }

}

template<typename RecordType>
void MrfOutputRecord<RecordType>::processComplete() {
  if (writeSuccessful) {
    if (this->getRecordAddress().isVerify()) {
      if ((writeReplyValue & this->getMask())
          != (writeRequestValue & this->getMask())) {
        recGblSetSevr(this->getRecord(), WRITE_ALARM, INVALID_ALARM);
        throw std::runtime_error(
            "Mismatch between the value written to the device and the value read back from the device.");
      }
    }
  } else {
    recGblSetSevr(this->getRecord(), WRITE_ALARM, INVALID_ALARM);
    throw std::runtime_error(writeErrorMessage);
  }
}

template<typename RecordType>
template<typename T>
MrfOutputRecord<RecordType>::CallbackImpl<T>::CallbackImpl(
    MrfOutputRecord &record) :
    record(record) {
}

template<typename RecordType>
template<typename T>
void MrfOutputRecord<RecordType>::CallbackImpl<T>::success(std::uint32_t,
    T value) {
  record.writeSuccessful = true;
  record.writeReplyValue = value;
  record.scheduleProcessing();
}

template<typename RecordType>
template<typename T>
void MrfOutputRecord<RecordType>::CallbackImpl<T>::failure(
    std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
    const std::string &details) {
  record.writeSuccessful = false;
  try {
    record.writeErrorMessage = std::string("Error writing to address ")
        + mrfMemoryAddressToString(address) + ": "
        + (details.empty() ? mrfErrorCodeToString(errorCode) : details);
  } catch (...) {
    // We want to schedule processing of the record even if we cannot assemble
    // the error message for some obscure reason.
  }
  record.scheduleProcessing();
}

}
}
}

#endif // ANKA_MRF_EPICS_OUTPUT_RECORD_H
