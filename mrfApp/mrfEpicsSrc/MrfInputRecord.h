/*
 * Copyright 2015-2024 aquenos GmbH.
 * Copyright 2015-2024 Karlsruhe Institute of Technology.
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

#ifndef ANKA_MRF_EPICS_INPUT_RECORD_H
#define ANKA_MRF_EPICS_INPUT_RECORD_H

#include <stdexcept>
#include <string>

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
class MrfInputRecord: public MrfRecord<RecordType> {

protected:

  /**
   * Creates an instance of the device support class for the specified record
   * instance.
   */
  MrfInputRecord(RecordType *record) :
      MrfRecord<RecordType>(record, record->inp), readSuccessful(false), readValue(
          0) {
  }

  /**
   * Destructor.
   */
  virtual ~MrfInputRecord() {
  }

  virtual void processPrepare();

  virtual void processComplete();

private:

  template<typename T>
  struct CallbackImpl: MrfMemoryAccess::Callback<T> {
    CallbackImpl(MrfInputRecord &record);
    void success(std::uint32_t address, T value);
    void failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
        const std::string &details);

    // In EPICS, records are never destroyed. Therefore, we can safely keep a
    // reference to the device support object.
    MrfInputRecord &record;
  };

  // We do not want to allow copy or move construction or assignment.
  MrfInputRecord(const MrfInputRecord &) = delete;
  MrfInputRecord(MrfInputRecord &&) = delete;
  MrfInputRecord &operator=(const MrfInputRecord &) = delete;
  MrfInputRecord &operator=(MrfInputRecord &&) = delete;

  bool readSuccessful;
  std::uint32_t readValue;
  std::string readErrorMessage;

};

template<typename RecordType>
void MrfInputRecord<RecordType>::processPrepare() {
  switch (this->getRecordAddress().getDataType()) {
  case MrfRecordAddress::DataType::uInt16: {
    auto callback = std::make_shared<CallbackImpl<std::uint16_t>>(*this);
    this->getDevice()->readUInt16(this->getRecordAddress().getMemoryAddress(),
        callback);
    break;
  }
  case MrfRecordAddress::DataType::uInt32: {
    auto callback = std::make_shared<CallbackImpl<std::uint32_t>>(*this);
    this->getDevice()->readUInt32(this->getRecordAddress().getMemoryAddress(),
        callback);
    break;
  }
  }
}

template<typename RecordType>
void MrfInputRecord<RecordType>::processComplete() {
  if (readSuccessful) {
    this->writeRecordValue(this->convertFromDevice(readValue));
  } else {
    recGblSetSevr(this->getRecord(), READ_ALARM, INVALID_ALARM);
    throw std::runtime_error(readErrorMessage);
  }
}

template<typename RecordType>
template<typename T>
MrfInputRecord<RecordType>::CallbackImpl<T>::CallbackImpl(
    MrfInputRecord &record) :
    record(record) {
}

template<typename RecordType>
template<typename T>
void MrfInputRecord<RecordType>::CallbackImpl<T>::success(std::uint32_t,
    T value) {
  record.readSuccessful = true;
  record.readValue = value;
  record.scheduleProcessing();
}

template<typename RecordType>
template<typename T>
void MrfInputRecord<RecordType>::CallbackImpl<T>::failure(std::uint32_t address,
    MrfMemoryAccess::ErrorCode errorCode, const std::string &details) {
  record.readSuccessful = false;
  try {
    record.readErrorMessage = std::string("Error reading from address ")
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

#endif // ANKA_MRF_EPICS_INPUT_RECORD_H
