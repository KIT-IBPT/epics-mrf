/*
 * Copyright 2015-2026 aquenos GmbH.
 * Copyright 2015-2026 Karlsruhe Institute of Technology.
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

#include <cstdint>
#include <stdexcept>
#include <tuple>
#include <utility>

#include "MrfUdpIpMemoryAccessV1.h"

namespace anka {
namespace mrf {

MrfUdpIpMemoryAccessV1::MrfUdpIpMemoryAccessV1(
  const std::string &hostName, std::uint32_t baseAddress
) :
  MrfUdpIpMemoryAccess(hostName, baseAddress)
{
}

MrfUdpIpMemoryAccessV1::MrfUdpIpMemoryAccessV1(
  const std::string &hostName,
  std::uint32_t baseAddress,
  const std::chrono::duration<double> &queueTimeout,
  const std::chrono::duration<double> &requestTimeout
) :
  MrfUdpIpMemoryAccess(hostName, baseAddress, queueTimeout, requestTimeout)
{
}

MrfUdpIpMemoryAccessV1::~MrfUdpIpMemoryAccessV1() {
}

MrfUdpIpMemoryAccessV1::UInt32ReadShared::UInt32ReadShared(
  MrfUdpIpMemoryAccessV1 &memoryAccess,
  std::uint32_t address,
  std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback
) :
  memoryAccess(memoryAccess),
  address(address),
  callback(callback)
{
}

void MrfUdpIpMemoryAccessV1::UInt32ReadShared::receivedLow(
  std::uint16_t data
) {
  bool sendHighAgain;
  // We have to lock the mutex in order to avoid a race condition (most
  // actions are processed by the receive thread, but timeouts are processed
  // by the send thread).
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (failed) {
      // The request has already failed, thus we discard the result.
      return;
    }
    if (gotLow) {
      // Ignore a duplicate response.
      return;
    }
    gotLow = true;
    this->data = static_cast<std::uint32_t>(data);
    // The high word should always be read after the low word. Therefore, we
    // have to send the request for the high word again, if we received it
    // before the low word. There is still a slim chance that a read might
    // happen out of order, because the request for the high word might arrive
    // before the request for the low word but the response might be delayed
    // in the opposite order. However, this seems very unlikely.
    sendHighAgain = gotHigh;
    gotHigh = false;
  }
  if (sendHighAgain) {
    // Send request for high word again.
    memoryAccess.client.queueReadRequest16(
      memoryAccess.baseAddress + address,
      std::make_shared<UInt32ReadHighCallback>(this->shared_from_this())
    );
  }
}

void MrfUdpIpMemoryAccessV1::UInt32ReadShared::receivedHigh(std::uint16_t data) {
  bool complete;
  // We have to lock the mutex in order to avoid a race condition (most
  // actions are processed by the receive thread, but timeouts are processed
  // by the send thread).
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (failed) {
      // The request has already failed, thus we discard the result.
      return;
    }
    if (gotHigh) {
      // Ignore a duplicate response.
      return;
    }
    gotHigh = true;
    this->data |= static_cast<std::uint32_t>(data) << 16;
    complete = gotLow;
  }
  if (complete) {
    if (callback) {
      callback->success(address, this->data);
    }
  }
}

void MrfUdpIpMemoryAccessV1::UInt32ReadShared::failure(
  MrfMemoryAccess::ErrorCode errorCode, const std::string &details
) {
  // We have to lock the mutex in order to avoid a race condition (most
  // actions are processed by the receive thread, but timeouts are processed
  // by the send thread).
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (failed) {
      // Notification of failure was already made.
      return;
    }
    failed = true;
    if (gotLow && gotHigh) {
      // Data was already received completely.
      return;
    }
  }
  if (callback) {
    callback->failure(address, errorCode, details);
  }
}

MrfUdpIpMemoryAccessV1::UInt32ReadLowCallback::UInt32ReadLowCallback(
  std::shared_ptr<UInt32ReadShared> sharedData
) : sharedData(sharedData) {
}

void MrfUdpIpMemoryAccessV1::UInt32ReadLowCallback::operator()(
  std::uint16_t receivedData,
  std::int8_t receivedStatus,
  std::exception_ptr exception
) {
  if (exception) {
    ErrorCode errorCode;
    std::string message;
    std::tie(errorCode, message) = exceptionToErrorCodeAndMessage(exception);
    sharedData->failure(errorCode, message);
  } else if (receivedStatus != 0) {
    sharedData->failure(statusToErrorCode(receivedStatus), std::string());
  } else {
    sharedData->receivedLow(receivedData);
  }
}

MrfUdpIpMemoryAccessV1::UInt32ReadHighCallback::UInt32ReadHighCallback(
  std::shared_ptr<UInt32ReadShared> sharedData
) : sharedData(sharedData) {
}

void MrfUdpIpMemoryAccessV1::UInt32ReadHighCallback::operator()(
  std::uint16_t receivedData,
  std::int8_t receivedStatus,
  std::exception_ptr exception
) {
  if (exception) {
    ErrorCode errorCode;
    std::string message;
    std::tie(errorCode, message) = exceptionToErrorCodeAndMessage(exception);
    sharedData->failure(errorCode, message);
  } else if (receivedStatus != 0) {
    sharedData->failure(statusToErrorCode(receivedStatus), std::string());
  } else {
    sharedData->receivedHigh(receivedData);
  }
}

MrfUdpIpMemoryAccessV1::UInt32WriteLowCallback::UInt32WriteLowCallback(
  std::uint32_t address,
  std::uint16_t highData,
  std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback
) :
  address(address),
  highData(highData),
  callback(callback)
{
}

void MrfUdpIpMemoryAccessV1::UInt32WriteLowCallback::operator()(
  std::uint16_t receivedData,
  std::int8_t receivedStatus,
  std::exception_ptr exception
) {
  if (callback) {
    if (exception) {
      ErrorCode errorCode;
      std::string message;
      std::tie(errorCode, message) = exceptionToErrorCodeAndMessage(exception);
      callback->failure(address, errorCode, message);
    } else if (receivedStatus != 0) {
      callback->failure(
        address, statusToErrorCode(receivedStatus), std::string()
      );
    } else {
      std::uint32_t data = static_cast<std::uint32_t>(highData) << 16;
      data |= static_cast<std::uint32_t>(receivedData);
      callback->success(address, data);
    }
  }
}

MrfUdpIpMemoryAccessV1::UInt32WriteHighCallback::UInt32WriteHighCallback(
  MrfUdpIpMemoryAccessV1 &memoryAccess,
  std::uint32_t address,
  std::uint16_t lowData,
  std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback
) :
  memoryAccess(memoryAccess),
  address(address),
  lowData(lowData),
  callback(callback)
{
}

void MrfUdpIpMemoryAccessV1::UInt32WriteHighCallback::operator()(
  std::uint16_t receivedData,
  std::int8_t receivedStatus,
  std::exception_ptr exception
) {
  if (exception) {
    if (callback) {
      ErrorCode errorCode;
      std::string message;
      std::tie(errorCode, message) = exceptionToErrorCodeAndMessage(exception);
      callback->failure(address, errorCode, message);
    }
  } else if (receivedStatus != 0) {
    if (callback) {
      callback->failure(
        address, statusToErrorCode(receivedStatus), std::string()
      );
    }
  } else {
    try {
      std::shared_ptr<UInt32WriteLowCallback> internalCallback = (
        std::make_shared<UInt32WriteLowCallback>(
          address, receivedData, callback
        )
      );
      memoryAccess.client.queueWriteRequest16(
        memoryAccess.baseAddress + address + 2, lowData, internalCallback
      );
    } catch (std::exception &e) {
      callback->failure(address, ErrorCode::unknown,
          std::string("The write request could not be queued: ") + e.what());
    } catch (...) {
      callback->failure(address, ErrorCode::unknown,
          std::string("The write request could not be queued."));
    }
  }
}

void MrfUdpIpMemoryAccessV1::readUInt32(
  std::uint32_t address, std::shared_ptr<CallbackUInt32> callback
) {
  std::shared_ptr<UInt32ReadShared> sharedData = (
    std::make_shared<UInt32ReadShared>(*this, address, callback)
  );
  // The low word should be read first.
  std::shared_ptr<UInt32ReadLowCallback> lowCallback = (
    std::make_shared<UInt32ReadLowCallback>(sharedData)
  );
  client.queueReadRequest16(baseAddress + address + 2, lowCallback);
  // The high word should be read second. If we cannot queue the second read
  // request, we do not throw but call the failure method on the callback
  // shared data object instead. Otherwise, the callback might be called if the
  // request to read the lower word fails but the calling code will not expect
  // the callback to be called because from its perspective, the attempt to
  // queue the request failed.
  try {
    std::shared_ptr<UInt32ReadHighCallback> highCallback = (
      std::make_shared<UInt32ReadHighCallback>(sharedData)
    );
    client.queueReadRequest16(baseAddress + address, highCallback);
  } catch (std::exception &e) {
    try {
      callback->failure(
        address,
        ErrorCode::unknown,
        std::string("The read request could not be queued: ") + e.what()
      );
    } catch (...) {
      // We do not want an exception in the callback to bubble up to the calling
      // code.
    }
  } catch (...) {
    try {
      callback->failure(
        address,
        ErrorCode::unknown,
        std::string("The read request could not be queued.")
      );
    } catch (...) {
      // We do not want an exception in the callback to bubble up to the calling
      // code.
    }
  }
}

void MrfUdpIpMemoryAccessV1::writeUInt32(
  std::uint32_t address,
  std::uint32_t value,
  std::shared_ptr<CallbackUInt32> callback
) {
  std::uint16_t lowWord = static_cast<std::uint16_t>(value);
  std::uint16_t highWord = static_cast<std::uint16_t>(value >> 16);
  // We have to write the high word first. Once it has been written, we can
  // write the low word. We do not queue both, because unlike a read request,
  // a write request where the low word is processed first could cause
  // inconsistent data in the device.
  std::shared_ptr<UInt32WriteHighCallback> internalCallback = (
    std::make_shared<UInt32WriteHighCallback>(
      *this, address, lowWord, callback
    )
  );
  client.queueWriteRequest16(
    baseAddress + address, highWord, internalCallback
  );
}

} // namespace mrf
} // namespace anka
