/*
 * Copyright 2026 aquenos GmbH.
 * Copyright 2026 Karlsruhe Institute of Technology.
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

#include "MrfUdpIpMemoryAccessV2.h"

namespace anka {
namespace mrf {

MrfUdpIpMemoryAccessV2::MrfUdpIpMemoryAccessV2(
  const std::string &hostName, std::uint32_t baseAddress
) :
  MrfUdpIpMemoryAccess(hostName, baseAddress)
{
}

MrfUdpIpMemoryAccessV2::MrfUdpIpMemoryAccessV2(
  const std::string &hostName,
  std::uint32_t baseAddress,
  const std::chrono::duration<double> &queueTimeout,
  const std::chrono::duration<double> &requestTimeout
) :
  MrfUdpIpMemoryAccess(hostName, baseAddress, queueTimeout, requestTimeout)
{
}

MrfUdpIpMemoryAccessV2::~MrfUdpIpMemoryAccessV2() {
}

MrfUdpIpMemoryAccessV2::UInt32Callback::UInt32Callback(
  std::uint32_t address,
  std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback
) :
  address(address),
  callback(callback)
{
}

void MrfUdpIpMemoryAccessV2::UInt32Callback::operator()(
  std::uint32_t receivedData,
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

void MrfUdpIpMemoryAccessV2::readUInt32(
  std::uint32_t address, std::shared_ptr<CallbackUInt32> callback
) {
  std::shared_ptr<UInt32Callback> internalCallback = (
    std::make_shared<UInt32Callback>(address, callback)
  );
  client.queueReadRequest32(baseAddress + address, internalCallback);
}

void MrfUdpIpMemoryAccessV2::writeUInt32(
  std::uint32_t address,
  std::uint32_t value,
  std::shared_ptr<CallbackUInt32> callback
) {
  std::shared_ptr<UInt32Callback> internalCallback = (
    std::make_shared<UInt32Callback>(address, callback)
  );
  client.queueWriteRequest32(baseAddress + address, value, internalCallback);
}

} // namespace mrf
} // namespace anka
