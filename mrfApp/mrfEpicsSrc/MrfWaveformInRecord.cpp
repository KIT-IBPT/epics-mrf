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

#include <cstring>
#include <stdexcept>

#include <alarm.h>
#include <dbFldTypes.h>
#include <recGbl.h>

#include "MrfDeviceRegistry.h"

#include "MrfWaveformInRecord.h"

namespace anka {
namespace mrf {
namespace epics {

namespace {

MrfRecordAddress readRecordAddress(const ::DBLINK &addressField) {
  if (addressField.type != INST_IO) {
    throw std::runtime_error(
        "Invalid device address. Maybe mixed up INP/OUT or forgot '@'?");
  }
  return MrfRecordAddress(
      addressField.value.instio.string == nullptr ?
          "" : addressField.value.instio.string);
}

} // End of anonymous namespace

void MrfWaveformInRecord::CallbackImpl::success(uint32_t address,
    uint32_t value) {
  std::unique_lock<std::recursive_mutex> lock(deviceSupport.mutex);
  try {
    std::uint32_t arrayIndex = (address
        - deviceSupport.address.getMemoryAddress())
        / (sizeof(std::uint32_t) + deviceSupport.address.getElementDistance());
    // The address might come from the network, so we should not trust the value
    // but make a sanity check.
    if (arrayIndex < deviceSupport.lastValueRead.size()) {
      deviceSupport.lastValueRead[arrayIndex] = value;
    }
  } catch (...) {
    // If there is any error, we still want to decrement the
    // pendingReadRequests counter and process the record again.
  }
  --deviceSupport.pendingReadRequests;
  if (deviceSupport.pendingReadRequests == 0) {
    ::callbackRequestProcessCallback(&deviceSupport.processCallback,
    priorityMedium, deviceSupport.record);
  }
}

void MrfWaveformInRecord::CallbackImpl::failure(uint32_t address,
    MrfMemoryAccess::ErrorCode errorCode, const std::string &details) {
  std::unique_lock<std::recursive_mutex> lock(deviceSupport.mutex);
  try {
    // We want to use the message from the first error.
    if (deviceSupport.readSuccessful) {
      deviceSupport.readSuccessful = false;
      deviceSupport.readErrorMessage = std::string(
          "Error reading from address ") + mrfMemoryAddressToString(address)
          + ": "
          + (details.empty() ? mrfErrorCodeToString(errorCode) : details);
    }
  } catch (...) {
    // We ignore any error that might be caused by creating the error message.
    deviceSupport.readErrorMessage = "";
  }
  --deviceSupport.pendingReadRequests;
  if (deviceSupport.pendingReadRequests == 0) {
    ::callbackRequestProcessCallback(&deviceSupport.processCallback,
    priorityMedium, deviceSupport.record);
  }
}

MrfWaveformInRecord::MrfWaveformInRecord(::waveformRecord *record) :
    address(readRecordAddress(record->inp)), record(record), readCallback(
        std::make_shared<CallbackImpl>(*this)), readSuccessful(false), pendingReadRequests(
        0), lastValueRead(record->nelm) {
  if (this->record->ftvl != DBF_CHAR && this->record->ftvl != DBF_UCHAR
      && this->record->ftvl != DBF_SHORT && this->record->ftvl != DBF_USHORT
      && this->record->ftvl != DBF_LONG && this->record->ftvl != DBF_ULONG) {
    throw std::runtime_error(
        "The value type of the array must be CHAR, UCHAR, SHORT, USHORT, LONG, or ULONG.");
  }
  if (this->address.getDataType() != MrfRecordAddress::DataType::uInt32) {
    throw std::runtime_error(
        "The waveform record only supports 32-bit unsigned integer registers.");
  }
  if (this->address.getMemoryAddressHighestBit() != 31
      || this->address.getMemoryAddressLowestBit() != 0) {
    throw std::runtime_error(
        "The waveform record does not support reading individual bits of a register.");
  }
  this->device = MrfDeviceRegistry::getInstance().getDevice(
      this->address.getDeviceId());
  if (!this->device) {
    throw std::runtime_error(
        std::string("Could not find device ") + this->address.getDeviceId()
            + ".");
  }
  // Make sure that all elements are initialized with zeros.
  switch (this->record->ftvl) {
  case DBF_CHAR:
  case DBF_UCHAR:
    std::memset(this->record->bptr, 0, this->record->nelm);
    break;
  case DBF_SHORT:
  case DBF_USHORT:
    std::memset(this->record->bptr, 0, this->record->nelm * 2);
    break;
  case DBF_LONG:
  case DBF_ULONG:
    std::memset(this->record->bptr, 0, this->record->nelm * 4);
    break;
  }
}

void MrfWaveformInRecord::processRecord() {
  if (this->record->pact) {
    this->record->pact = false;
    if (!readSuccessful) {
      recGblSetSevr(this->record, READ_ALARM, INVALID_ALARM);
      throw std::runtime_error(readErrorMessage);
    } else {
      std::uint8_t *recordValueBufferUInt8 =
          reinterpret_cast<std::uint8_t *>(this->record->bptr);
      std::uint16_t *recordValueBufferUInt16 =
          reinterpret_cast<std::uint16_t *>(this->record->bptr);
      std::uint32_t *recordValueBufferUInt32 =
          reinterpret_cast<std::uint32_t *>(this->record->bptr);
      for (std::uint32_t arrayIndex = 0; arrayIndex < record->nelm;
          ++arrayIndex) {
        switch (record->ftvl) {
        case DBF_CHAR:
        case DBF_UCHAR:
          recordValueBufferUInt8[arrayIndex] = lastValueRead[arrayIndex];
          break;
        case DBF_SHORT:
        case DBF_USHORT:
          recordValueBufferUInt16[arrayIndex] = lastValueRead[arrayIndex];
          break;
        case DBF_LONG:
        case DBF_ULONG:
          recordValueBufferUInt32[arrayIndex] = lastValueRead[arrayIndex];
          break;
        }
      }
      // We always read the specified number of elements, therefore we can set
      // NORD to NELM.
      this->record->nord = this->record->nelm;
      // The value has been read successfully, thus the record is not
      // undefined any longer.
      this->record->udf = false;
    }
  } else {
    // We have to hold the mutex in this block. That ensures that callbacks,
    // that are triggered asynchronously are not processed before we finish.
    std::unique_lock<std::recursive_mutex> lock(mutex);
    // We set the readSuccessful flag. If one of the read requests fails, it is
    // cleared by the callback.
    readSuccessful = true;
    // We start with a non-zero value for the pending read requests. This
    // ensures that the callback does not trigger actions prematurely if it is
    // called within the same thread.
    pendingReadRequests = 1;
    for (std::uint32_t arrayIndex = 0; arrayIndex < record->nelm;
        ++arrayIndex) {
      ++pendingReadRequests;
      device->readUInt32(
          address.getMemoryAddress()
              + (sizeof(std::uint32_t) + address.getElementDistance())
                  * arrayIndex, readCallback);
    }
    // Now we can decrement the number of pending read requests so that it
    // matches the actual number. If the remaining number is zero, we are
    // already finished.
    --pendingReadRequests;
    if (pendingReadRequests == 0) {
      if (!readSuccessful) {
        recGblSetSevr(this->record, READ_ALARM, INVALID_ALARM);
        throw std::runtime_error(readErrorMessage);
      } else {
        std::uint8_t *recordValueBufferUInt8 =
            reinterpret_cast<std::uint8_t *>(this->record->bptr);
        std::uint16_t *recordValueBufferUInt16 =
            reinterpret_cast<std::uint16_t *>(this->record->bptr);
        std::uint32_t *recordValueBufferUInt32 =
            reinterpret_cast<std::uint32_t *>(this->record->bptr);
        for (std::uint32_t arrayIndex = 0; arrayIndex < record->nelm;
            ++arrayIndex) {
          switch (record->ftvl) {
          case DBF_CHAR:
          case DBF_UCHAR:
            recordValueBufferUInt8[arrayIndex] = lastValueRead[arrayIndex];
            break;
          case DBF_SHORT:
          case DBF_USHORT:
            recordValueBufferUInt16[arrayIndex] = lastValueRead[arrayIndex];
            break;
          case DBF_LONG:
          case DBF_ULONG:
            recordValueBufferUInt32[arrayIndex] = lastValueRead[arrayIndex];
            break;
          }
        }
        // We always read the specified number of elements, therefore we can set
        // NORD to NELM.
        this->record->nord = this->record->nelm;
        // The value has been read successfully, thus the record is not
        // undefined any longer.
        this->record->udf = false;
      }
    } else {
      this->record->pact = true;
    }
  }
}

}
}
}
