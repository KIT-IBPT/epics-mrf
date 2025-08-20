/*
 * Copyright 2015-2025 aquenos GmbH.
 * Copyright 2015-2025 Karlsruhe Institute of Technology.
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

#include "MrfUdpIpMemoryAccess.h"

namespace anka {
namespace mrf {

namespace {

  std::pair<
    MrfMemoryAccess::ErrorCode, std::string> exceptionToErrorCodeAndMessage(
    std::exception_ptr exception) {
  try {
    std::rethrow_exception(exception);
  } catch (MrfUdpIpClient::TimeoutException &e) {
    return std::make_pair(
      MrfMemoryAccess::ErrorCode::networkTimeout, std::string());
  } catch (std::exception &e) {
    return std::make_pair(
      MrfMemoryAccess::ErrorCode::unknown, std::string(e.what()));
  } catch (...) {
    return std::make_pair(MrfMemoryAccess::ErrorCode::unknown, std::string());
  }
}

MrfMemoryAccess::ErrorCode statusToErrorCode(std::int8_t status) {
  switch (status) {
  case -1:
    return MrfMemoryAccess::ErrorCode::invalidAddress;
  case -2:
    return MrfMemoryAccess::ErrorCode::fpgaTimeout;
  case -3:
    return MrfMemoryAccess::ErrorCode::invalidCommand;
  default:
    return MrfMemoryAccess::ErrorCode::unknown;
  }
}

} // anonymous namespace.

// The constants are initialized in the class definition, so we only have to
// define them here so that they get storage assigned (otherwise, the linker
// will complain about missing symbols when they are used by reference).
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvgCrCsr;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvgRegister;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvmCrCsr;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvmRegister;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvr230CrCsr;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvr230Register;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvr300CrCsr;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvr300Register;

MrfUdpIpMemoryAccess::MrfUdpIpMemoryAccess(
    const std::string &hostName, std::uint32_t baseAddress) :
    MrfUdpIpMemoryAccess(
      hostName,
      baseAddress,
      std::chrono::duration<double>(0),
      std::chrono::duration<double>(5.0)) {
}

MrfUdpIpMemoryAccess::MrfUdpIpMemoryAccess(
    const std::string &hostName,
    std::uint32_t baseAddress,
    const std::chrono::duration<double> &queueTimeout,
    const std::chrono::duration<double> &requestTimeout) :
    baseAddress(baseAddress),
    client(hostName, queueTimeout, requestTimeout) {
}

MrfUdpIpMemoryAccess::~MrfUdpIpMemoryAccess() {
}

MrfUdpIpMemoryAccess::UInt16Callback::UInt16Callback(std::uint32_t address,
    std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback) :
    address(address), callback(callback) {
}

void MrfUdpIpMemoryAccess::UInt16Callback::operator()(
    std::uint16_t receivedData,
    std::int8_t receivedStatus,
    std::exception_ptr exception) {
  if (callback) {
    if (exception) {
      ErrorCode errorCode;
      std::string message;
      std::tie(errorCode, message) = exceptionToErrorCodeAndMessage(exception);
      callback->failure(this->address, errorCode, message);
    } else if (receivedStatus != 0) {
      callback->failure(
        this->address, statusToErrorCode(receivedStatus), std::string());
    } else {
      callback->success(this->address, receivedData);
    }
  }
}

MrfUdpIpMemoryAccess::UInt32ReadShared::UInt32ReadShared(
    MrfUdpIpMemoryAccess &memoryAccess, std::uint32_t address,
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback) :
    memoryAccess(memoryAccess), address(address), callback(callback) {
}

void MrfUdpIpMemoryAccess::UInt32ReadShared::receivedLow(std::uint16_t data) {
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
    memoryAccess.client.queueReadRequest(
      memoryAccess.baseAddress + address,
      std::make_shared<UInt32ReadHighCallback>(this->shared_from_this()));
  }
}

void MrfUdpIpMemoryAccess::UInt32ReadShared::receivedHigh(std::uint16_t data) {
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

void MrfUdpIpMemoryAccess::UInt32ReadShared::failure(
    MrfMemoryAccess::ErrorCode errorCode, const std::string &details) {
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

MrfUdpIpMemoryAccess::UInt32ReadLowCallback::UInt32ReadLowCallback(
    std::shared_ptr<UInt32ReadShared> sharedData) :
    sharedData(sharedData) {
}
void MrfUdpIpMemoryAccess::UInt32ReadLowCallback::operator()(
    std::uint16_t receivedData,
    std::int8_t receivedStatus,
    std::exception_ptr exception) {
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

MrfUdpIpMemoryAccess::UInt32ReadHighCallback::UInt32ReadHighCallback(
    std::shared_ptr<UInt32ReadShared> sharedData) :
    sharedData(sharedData) {
}

void MrfUdpIpMemoryAccess::UInt32ReadHighCallback::operator()(
    std::uint16_t receivedData,
    std::int8_t receivedStatus,
    std::exception_ptr exception) {
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

MrfUdpIpMemoryAccess::UInt32WriteLowCallback::UInt32WriteLowCallback(
    std::uint32_t address, std::uint16_t highData,
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback) :
    address(address), highData(highData), callback(callback) {
}

void MrfUdpIpMemoryAccess::UInt32WriteLowCallback::operator()(
    std::uint16_t receivedData,
    std::int8_t receivedStatus,
    std::exception_ptr exception) {
  if (callback) {
    if (exception) {
      ErrorCode errorCode;
      std::string message;
      std::tie(errorCode, message) = exceptionToErrorCodeAndMessage(exception);
      callback->failure(address, errorCode, message);
    } else if (receivedStatus != 0) {
      callback->failure(
        address, statusToErrorCode(receivedStatus), std::string());
    } else {
      std::uint32_t data = static_cast<std::uint32_t>(highData) << 16;
      data |= static_cast<std::uint32_t>(receivedData);
      callback->success(address, data);
    }
  }
}

MrfUdpIpMemoryAccess::UInt32WriteHighCallback::UInt32WriteHighCallback(
    MrfUdpIpMemoryAccess &memoryAccess, std::uint32_t address,
    std::uint16_t lowData,
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback) :
    memoryAccess(memoryAccess), address(address), lowData(lowData), callback(
        callback) {
}

void MrfUdpIpMemoryAccess::UInt32WriteHighCallback::operator()(
    std::uint16_t receivedData,
    std::int8_t receivedStatus,
    std::exception_ptr exception) {
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
        address, statusToErrorCode(receivedStatus), std::string());
    }
  } else {
    try {
      std::shared_ptr<UInt32WriteLowCallback> internalCallback =
          std::make_shared<UInt32WriteLowCallback>(address, receivedData,
              callback);
      memoryAccess.client.queueWriteRequest(
        memoryAccess.baseAddress + address + 2, lowData, internalCallback);
    } catch (std::exception &e) {
      callback->failure(address, ErrorCode::unknown,
          std::string("The write request could not be queued: ") + e.what());
    } catch (...) {
      callback->failure(address, ErrorCode::unknown,
          std::string("The write request could not be queued."));
    }
  }
}

void MrfUdpIpMemoryAccess::readUInt16(std::uint32_t address,
    std::shared_ptr<CallbackUInt16> callback) {
  std::shared_ptr<UInt16Callback> internalCallback = std::make_shared<
      UInt16Callback>(address, callback);
  client.queueReadRequest(baseAddress + address, internalCallback);
}

void MrfUdpIpMemoryAccess::writeUInt16(std::uint32_t address,
    std::uint16_t value, std::shared_ptr<CallbackUInt16> callback) {
  std::shared_ptr<UInt16Callback> internalCallback = std::make_shared<
      UInt16Callback>(address, callback);
  client.queueWriteRequest(baseAddress + address, value, internalCallback);
}

void MrfUdpIpMemoryAccess::readUInt32(std::uint32_t address,
    std::shared_ptr<CallbackUInt32> callback) {
  std::shared_ptr<UInt32ReadShared> sharedData = std::make_shared<
      UInt32ReadShared>(*this, address, callback);
  // The low word should be read first.
  std::shared_ptr<UInt32ReadLowCallback> lowCallback = std::make_shared<
      UInt32ReadLowCallback>(sharedData);
  client.queueReadRequest(baseAddress + address + 2, lowCallback);
  // The high word should be read second. If we cannot queue the second read
  // request, we do not throw but call the failure method on the callback
  // shared data object instead. Otherwise, the callback might be called if the
  // request to read the lower word fails but the calling code will not expect
  // the callback to be called because from its perspective, the attempt to
  // queue the request failed.
  try {
    std::shared_ptr<UInt32ReadHighCallback> highCallback = std::make_shared<
        UInt32ReadHighCallback>(sharedData);
    client.queueReadRequest(baseAddress + address, highCallback);
  } catch (std::exception &e) {
    try {
      callback->failure(address, ErrorCode::unknown,
          std::string("The read request could not be queued: ") + e.what());
    } catch (...) {
      // We do not want an exception in the callback to bubble up to the calling
      // code.
    }
  } catch (...) {
    try {
      callback->failure(address, ErrorCode::unknown,
          std::string("The read request could not be queued."));
    } catch (...) {
      // We do not want an exception in the callback to bubble up to the calling
      // code.
    }
  }
}

void MrfUdpIpMemoryAccess::writeUInt32(std::uint32_t address,
    std::uint32_t value, std::shared_ptr<CallbackUInt32> callback) {
  std::uint16_t lowWord = static_cast<std::uint16_t>(value);
  std::uint16_t highWord = static_cast<std::uint16_t>(value >> 16);
  // We have to write the high word first. Once it has been written, we can
  // write the low word. We do not queue both, because unlike a read request,
  // a write request where the low word is processed first could cause
  // inconsistent data in the device.
  std::shared_ptr<UInt32WriteHighCallback> internalCallback = std::make_shared<
      UInt32WriteHighCallback>(*this, address, lowWord, callback);
  client.queueWriteRequest(baseAddress + address, highWord, internalCallback);
}

} // namespace mrf
} // namespace anka
