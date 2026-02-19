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

#include "MrfUdpIpMemoryAccess.h"

namespace anka {
namespace mrf {

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

MrfUdpIpMemoryAccess::~MrfUdpIpMemoryAccess() {
}

void MrfUdpIpMemoryAccess::readUInt16(
  std::uint32_t address, std::shared_ptr<CallbackUInt16> callback
) {
  std::shared_ptr<UInt16Callback> internalCallback = (
    std::make_shared<UInt16Callback>(address, callback)
  );
  client.queueReadRequest16(baseAddress + address, internalCallback);
}

void MrfUdpIpMemoryAccess::writeUInt16(
  std::uint32_t address,
  std::uint16_t value,
  std::shared_ptr<CallbackUInt16> callback,
  ReadbackMode readbackMode
) {
  std::shared_ptr<UInt16Callback> internalCallback = (
    std::make_shared<UInt16Callback>(address, callback)
  );
  bool readback;
  switch (readbackMode) {
  case ReadbackMode::may:
    if (client.getProtocolVersion() == ProtocolVersion::V1) {
      readback = true;
    } else {
      readback = false;
    }
    break;
  case ReadbackMode::must:
    readback = true;
    break;
  case ReadbackMode::mustNot:
    if (client.getProtocolVersion() == ProtocolVersion::V1) {
      throw std::invalid_argument(
        "ReadbackMode::mustNot is not supported for protocol version 1."
      );
    }
    readback = false;
    break;
  }
  client.queueWriteRequest16(
    baseAddress + address, value, internalCallback, readback
  );
}

MrfUdpIpMemoryAccess::MrfUdpIpMemoryAccess(
  ProtocolVersion protocolVersion,
  const std::string &hostName,
  std::uint32_t baseAddress
) :
  MrfUdpIpMemoryAccess(
    protocolVersion,
    hostName,
    baseAddress,
    std::chrono::duration<double>(0),
    std::chrono::duration<double>(5.0)
  )
{
}

MrfUdpIpMemoryAccess::MrfUdpIpMemoryAccess(
  ProtocolVersion protocolVersion,
  const std::string &hostName,
  std::uint32_t baseAddress,
  const std::chrono::duration<double> &queueTimeout,
  const std::chrono::duration<double> &requestTimeout
) :
  baseAddress(baseAddress),
  client(protocolVersion, hostName, queueTimeout, requestTimeout)
{
}

std::pair<
  MrfMemoryAccess::ErrorCode, std::string
> MrfUdpIpMemoryAccess::exceptionToErrorCodeAndMessage(
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

MrfMemoryAccess::ErrorCode MrfUdpIpMemoryAccess::statusToErrorCode(
  std::int8_t status
) {
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

MrfUdpIpMemoryAccess::UInt16Callback::UInt16Callback(
  std::uint32_t address,
  std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback
) :
  address(address),
  callback(callback)
{
}

void MrfUdpIpMemoryAccess::UInt16Callback::operator()(
  std::uint16_t receivedData,
  std::int8_t receivedStatus,
  std::exception_ptr exception
) {
  if (callback) {
    if (exception) {
      ErrorCode errorCode;
      std::string message;
      std::tie(errorCode, message) = exceptionToErrorCodeAndMessage(exception);
      callback->failure(this->address, errorCode, message);
    } else if (receivedStatus != 0) {
      callback->failure(
        this->address, statusToErrorCode(receivedStatus), std::string()
      );
    } else {
      callback->success(this->address, receivedData);
    }
  }
}

} // namespace mrf
} // namespace anka
