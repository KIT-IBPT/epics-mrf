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

#include <algorithm>
#include <cstring>

#include <alarm.h>
#include <dbFldTypes.h>
#include <recGbl.h>

#include "MrfDeviceRegistry.h"
#include "mrfEpicsError.h"

#include "MrfWaveformOutRecord.h"

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

void MrfWaveformOutRecord::CallbackImpl::success(uint32_t address,
    uint32_t value) {
  std::unique_lock<std::recursive_mutex> lock(deviceSupport.mutex);
  try {
    std::uint32_t arrayIndex = (address
        - deviceSupport.address.getMemoryAddress())
        / (sizeof(std::uint32_t) + deviceSupport.address.getElementDistance());
    // The address might come from the network, so we should not trust the value
    // but make a sanity check.
    if (arrayIndex < deviceSupport.lastValueWritten.size()) {
      if (!deviceSupport.address.isVerify()
          || deviceSupport.lastValueWritten[arrayIndex] == value) {
        deviceSupport.lastValueWrittenValid[arrayIndex] = true;
      } else {
        // We want to use the message from the first error.
        if (deviceSupport.writeSuccessful) {
          deviceSupport.writeSuccessful = false;
          deviceSupport.writeErrorMessage =
              "Mismatch between the value written to the device and the value read back from the device.";
        }
      }
    }
  } catch (...) {
    // If there is any error, we still want to decrement the
    // pendingWriteRequests counter and process the record again.
  }
  --deviceSupport.pendingWriteRequests;
  if (deviceSupport.pendingWriteRequests == 0) {
    ::callbackRequestProcessCallback(&deviceSupport.processCallback,
    priorityMedium, deviceSupport.record);
  }
}

void MrfWaveformOutRecord::CallbackImpl::failure(uint32_t address,
    MrfMemoryAccess::ErrorCode errorCode, const std::string &details) {
  std::unique_lock<std::recursive_mutex> lock(deviceSupport.mutex);
  try {
    // We want to use the message from the first error.
    if (deviceSupport.writeSuccessful) {
      deviceSupport.writeSuccessful = false;
      deviceSupport.writeErrorMessage = std::string("Error writing to address ")
          + mrfMemoryAddressToString(address) + ": "
          + (details.empty() ? mrfErrorCodeToString(errorCode) : details);
    }
  } catch (...) {
    // We ignore any error that might be caused by creating the error message.
    deviceSupport.writeErrorMessage = "";
  }
  --deviceSupport.pendingWriteRequests;
  if (deviceSupport.pendingWriteRequests == 0) {
    ::callbackRequestProcessCallback(&deviceSupport.processCallback,
    priorityMedium, deviceSupport.record);
  }
}

MrfWaveformOutRecord::MrfWaveformOutRecord(::waveformRecord *record) :
    address(readRecordAddress(record->inp)), record(record), writeCallback(
        std::make_shared<CallbackImpl>(*this)), writeSuccessful(false), pendingWriteRequests(
        0), lastValueWritten(record->nelm), lastValueWrittenValid(record->nelm,
        false) {
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
        "The waveform record does not support writing to individual bits of a register.");
  }
  this->device = MrfDeviceRegistry::getInstance().getDevice(
      this->address.getDeviceId());
  if (!this->device) {
    throw std::runtime_error(
        std::string("Could not find device ") + this->address.getDeviceId()
            + ".");
  }
  std::uint8_t *recordValueBufferUInt8 =
      reinterpret_cast<std::uint8_t *>(this->record->bptr);
  std::uint16_t *recordValueBufferUInt16 =
      reinterpret_cast<std::uint16_t *>(this->record->bptr);
  std::uint32_t *recordValueBufferUInt32 =
      reinterpret_cast<std::uint32_t *>(this->record->bptr);
  // We set the number of valid elements equal to the number of elements because
  // we always deal with fix-sized blocks of data.
  this->record->nord = this->record->nelm;
  if (this->address.isReadOnInit()) {
    std::shared_ptr<MrfMemoryCache> deviceCache =
        MrfDeviceRegistry::getInstance().getDeviceCache(
            this->address.getDeviceId());
    if (!deviceCache) {
      throw std::runtime_error(
          std::string("Could not find cache for device ")
              + this->address.getDeviceId() + ".");
    }
    // Read current value from device and update record's value and last value
    // written.
    bool readFromDeviceSuccessful = true;
    for (std::uint32_t arrayIndex = 0; arrayIndex < this->record->nelm;
        ++arrayIndex) {
      // We try to read the value from the device, so that we can initialize the
      // record’s value. If this fails, we do not let the exception bubble up,
      // because we still want the record to be initialized (so that it can be
      // processed later). In this case, the record will simply stay in an
      // undefined state (associated with an invalid alarm) until it is
      // successfully processed for the first time.
      std::uint32_t value;
      try {
        value = deviceCache->readUInt32(
            this->address.getMemoryAddress()
                + (sizeof(std::uint32_t) + this->address.getElementDistance())
                    * arrayIndex);
        } catch (std::exception &e) {
          errorExtendedPrintf(
              "%s Reading initial value from device failed: %s",
              this->record->name,
              e.what());
          readFromDeviceSuccessful = false;
          break;
        } catch (...) {
          errorExtendedPrintf(
              "%s Reading initial value from device failed: Unknown error.",
              this->record->name);
          readFromDeviceSuccessful = false;
          break;
        }
      lastValueWritten[arrayIndex] = value;
      lastValueWrittenValid[arrayIndex] = true;
      switch (this->record->ftvl) {
      case DBF_CHAR:
      case DBF_UCHAR:
        recordValueBufferUInt8[arrayIndex] = value;
        break;
      case DBF_SHORT:
      case DBF_USHORT:
        recordValueBufferUInt16[arrayIndex] = value;
        break;
      case DBF_LONG:
      case DBF_ULONG:
        recordValueBufferUInt32[arrayIndex] = value;
        break;
      }
    }
    if (readFromDeviceSuccessful) {
      // The record's value has been initialized, thus it is not undefined any
      // longer.
      this->record->udf = false;
      // We have to reset the alarm state explicitly, so that the record is not
      // marked as invalid. This is not optimal because the record will not be
      // placed in an alarm state if the value would usually trigger an alarm.
      // However, alarms on output records are uncommon and we do not use them
      // for the EVG or EVR, so this is fine. We also update the time stamp so
      // that it represents the current time.
      recGblGetTimeStamp(this->record);
      recGblResetAlarms(this->record);
      // We return because we do not want to initialize the record’s value with
      // zeros.
      return;
    } else {
      // We reset the last value-written valid flag, because we don’t want to
      // use a partially initialized array. Having such an array could lead to
      // very confusing behavior that would not be transparent to the user at
      // all.
      std::fill(
          lastValueWrittenValid.begin(), lastValueWrittenValid.end(), false);
    }
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

void MrfWaveformOutRecord::processRecord() {
  // The number of valid elements is reset when elements are written to the
  // record. However, we always want all elements to be considered valid, even
  // if not all of them have been updated.
  this->record->nord = this->record->nelm;
  if (this->record->pact) {
    this->record->pact = false;
    if (!writeSuccessful) {
      recGblSetSevr(this->record, WRITE_ALARM, INVALID_ALARM);
      throw std::runtime_error(writeErrorMessage);
    } else {
      // The value has been written successfully, thus the record is not
      // undefined any longer.
      this->record->udf = false;
    }
  } else {
    // We have to hold the mutex in this block. That ensures that callbacks,
    // that are triggered asynchronously are not processed before we finish.
    std::unique_lock<std::recursive_mutex> lock(mutex);
    // We set the writeSuccessful flag. If one of the write requests fails, it
    // is cleared by the callback.
    writeSuccessful = true;
    std::uint8_t *recordValueBufferUInt8 =
        reinterpret_cast<std::uint8_t *>(this->record->bptr);
    std::uint16_t *recordValueBufferUInt16 =
        reinterpret_cast<std::uint16_t *>(this->record->bptr);
    std::uint32_t *recordValueBufferUInt32 =
        reinterpret_cast<std::uint32_t *>(this->record->bptr);
    // We start with a non-zero value for the pending write requests. This
    // ensures that the callback does not trigger actions prematurely if it is
    // called within the same thread.
    pendingWriteRequests = 1;
    for (std::uint32_t arrayIndex = 0; arrayIndex < record->nelm;
        ++arrayIndex) {
      std::uint32_t value;
      switch (record->ftvl) {
      case DBF_CHAR:
      case DBF_UCHAR:
        value = recordValueBufferUInt8[arrayIndex];
        break;
      case DBF_SHORT:
      case DBF_USHORT:
        value = recordValueBufferUInt16[arrayIndex];
        break;
      case DBF_LONG:
      case DBF_ULONG:
        value = recordValueBufferUInt32[arrayIndex];
        break;
      default:
        // The default case can never happen because we ensure earlier that we
        // have a supported type, but the compiler complains about use of an
        // uninitialized value if we do not have this branch.
        value = 0;
        break;
      }
      if (!address.isChangedElementsOnly() || !lastValueWrittenValid[arrayIndex]
          || lastValueWritten[arrayIndex] != value) {
        // We set the valid flag to false. This ensures that the element will
        // be written again the next time if the write attempt is not
        // successful. If it is successful, the flag will be set again by the
        // callback.
        lastValueWrittenValid[arrayIndex] = false;
        lastValueWritten[arrayIndex] = value;
        ++pendingWriteRequests;
        device->writeUInt32(
            address.getMemoryAddress()
                + (sizeof(std::uint32_t) + address.getElementDistance())
                    * arrayIndex, value, writeCallback);
      }
    }
    // Now we can decrement the number of pending write requests so that it
    // matches the actual number. If the remaining number is zero, we are
    // already finished.
    --pendingWriteRequests;
    if (pendingWriteRequests == 0) {
      if (!writeSuccessful) {
        recGblSetSevr(this->record, WRITE_ALARM, INVALID_ALARM);
        throw std::runtime_error(writeErrorMessage);
      } else {
        // The value has been written successfully, thus the record is not
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
