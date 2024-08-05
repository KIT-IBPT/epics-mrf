/*
 * Copyright 2021-2024 aquenos GmbH.
 * Copyright 2021-2024 Karlsruhe Institute of Technology.
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

#include <arpa/inet.h>

#include <alarm.h>
#include <dbFldTypes.h>
#include <recGbl.h>

#include "MrfDeviceRegistry.h"

#include "MrfStringinRecord.h"

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

} // anonymous namespace

void MrfStringinRecord::CallbackImpl::success(
    uint32_t address, uint16_t value) {
  std::unique_lock<std::recursive_mutex> lock(deviceSupport.mutex);
  try {
    std::uint32_t offset = address - deviceSupport.address.getMemoryAddress();
    // The address might come from the network, so we should not trust the value
    // but make a sanity check.
    if (offset + sizeof(value)
        <= static_cast<std::uint32_t>(
          deviceSupport.address.getStringLength())) {
      // The MRF devices internally use big endian byte order, but this gets
      // converted to the host-specific byte order by the memory access class.
      // For strings, we need the bytes in their original order, so we convert
      // back to big endian. When running on a big endian architecture, this is
      // a no-op. We cannot qualify htons (::htons), because it is a macro on
      // some platforms.
      value = htons(value);
      std::memcpy(deviceSupport.lastValueRead + offset, &value, sizeof(value));
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

void MrfStringinRecord::CallbackImpl::success(
    uint32_t address, uint32_t value) {
  std::unique_lock<std::recursive_mutex> lock(deviceSupport.mutex);
  try {
    std::uint32_t offset = address - deviceSupport.address.getMemoryAddress();
    // The address might come from the network, so we should not trust the value
    // but make a sanity check.
    if (offset + sizeof(value)
        <= static_cast<std::uint32_t>(
          deviceSupport.address.getStringLength())) {
      // The MRF devices internally use big endian byte order, but this gets
      // converted to the host-specific byte order by the memory access class.
      // For strings, we need the bytes in their original order, so we convert
      // back to big endian. When running on a big endian architecture, this is
      // a no-op. We cannot qualify htonl (::htonl), because it is a macro on
      // some platforms.
      value = htonl(value);
      std::memcpy(deviceSupport.lastValueRead + offset, &value, sizeof(value));
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

void MrfStringinRecord::CallbackImpl::failure(uint32_t address,
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

MrfStringinRecord::MrfStringinRecord(::stringinRecord *record) :
    address(readRecordAddress(record->inp)), record(record),
    readCallback(std::make_shared<CallbackImpl>(*this)), readSuccessful(false),
    pendingReadRequests(0) {
  if (this->address.getElementDistance() != 0) {
    throw std::runtime_error(
      "The stringin record does not support setting an element distance.");
  }
  if (this->address.getStringLength() <= 0) {
    throw std::runtime_error(
      "The string length must be set to a positive value.");
  }
  int unitSize;
  switch (this->address.getDataType()) {
    case MrfRecordAddress::DataType::uInt16:
      unitSize = 2;
      break;
    case MrfRecordAddress::DataType::uInt32:
      unitSize = 4;
      break;
    default:
      throw std::runtime_error("Unsupported data type requested.");
  }
  if (this->address.getStringLength() % unitSize != 0) {
    throw std::runtime_error(
      std::string(
        "The stringin record only supports string lengths that are a multiple "
        "of ")
      + std::to_string(unitSize) + ".");
  }
  if (this->address.getStringLength() > maxStringLength) {
    throw std::runtime_error(
      std::string("The string length must not exceed ")
      + std::to_string(maxStringLength) + " bytes.");
  }
  if (this->address.getMemoryAddressHighestBit() != 31
      || this->address.getMemoryAddressLowestBit() != 0) {
    throw std::runtime_error(
      "The stringin record does not support reading individual bits of a "
      "register.");
  }
  this->device = MrfDeviceRegistry::getInstance().getDevice(
      this->address.getDeviceId());
  if (!this->device) {
    throw std::runtime_error(
        std::string("Could not find device ") + this->address.getDeviceId()
            + ".");
  }
}

void MrfStringinRecord::processRecord() {
  if (this->record->pact) {
    this->record->pact = false;
    if (!readSuccessful) {
      recGblSetSevr(this->record, READ_ALARM, INVALID_ALARM);
      throw std::runtime_error(readErrorMessage);
    } else {
      std::memcpy(this->record->val, this->lastValueRead, maxStringLength);
      // Ensure that the string always is null terminated.
      int stringLength = this->address.getStringLength();
      if (stringLength >= maxStringLength) {
        this->record->val[maxStringLength - 1] = '\0';
      } else {
        this->record->val[stringLength] = '\0';
      }
      // The value has been read successfully, thus the record is not undefined
      // any longer.
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
    if (address.getDataType() == MrfRecordAddress::DataType::uInt16) {
      for (
          std::uint32_t offset = 0;
          offset < static_cast<std::uint32_t>(address.getStringLength());
          offset += sizeof(std::uint16_t)) {
        ++pendingReadRequests;
        device->readUInt16(
          address.getMemoryAddress() + offset,
          readCallback);
      }
    } else if (address.getDataType() == MrfRecordAddress::DataType::uInt32) {
      for (
          std::uint32_t offset = 0;
          offset < static_cast<std::uint32_t>(address.getStringLength());
          offset += sizeof(std::uint32_t)) {
        ++pendingReadRequests;
        device->readUInt32(
          address.getMemoryAddress() + offset,
          readCallback);
      }
    } else {
      // This should never happen as this case should already be caught during
      // initialization.
      throw std::runtime_error("Unsupported data type.");
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
      std::memcpy(this->record->val, this->lastValueRead, maxStringLength);
        std::memcpy(this->record->val, this->lastValueRead, maxStringLength);
        // Ensure that the string always is null terminated.
        int stringLength = this->address.getStringLength();
        if (stringLength >= maxStringLength) {
          this->record->val[maxStringLength - 1] = '\0';
        } else {
          this->record->val[stringLength] = '\0';
        }
        // The value has been read successfully, thus the record is not
        // undefined any longer.
        this->record->udf = false;
      }
    } else {
      this->record->pact = true;
    }
  }
}

} // namespace epics
} // namespace mrf
} // namepace anka
