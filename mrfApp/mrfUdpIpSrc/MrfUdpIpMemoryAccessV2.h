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

#ifndef ANKA_MRF_UDP_IP_MEMORY_ACCESS_V2_H
#define ANKA_MRF_UDP_IP_MEMORY_ACCESS_V2_H

#include <chrono>
#include <cstdint>
#include <string>

#include "MrfUdpIpMemoryAccess.h"

namespace anka {
namespace mrf {

/**
 * MRF memory access implementation that provides access to an MRF device
 * through version 2 of the UDP/IP protocol. At the time of writing, only
 * recent VME-based devices (e.g. VME-EVM-300) that have a sufficiently new
 * firmware installed on their SAM3X microcontrolelr support this kind of
 * access.
 */
class MrfUdpIpMemoryAccessV2 : public MrfUdpIpMemoryAccess {

public:

  /**
   * Creates a memory-access object for an MRF device that can be controlled
   * via UDP/IP.
   *
   * The specified host name can either be a DNS name or an IP address.
   *
   * The base address is added to the address specified in a read or write
   * request. This is useful if different devices with the same register layout
   * but different base addresses shall be accessed.
   *
   * This constructor uses a value of zero for the queue timeout (thus
   * disabling the queue timeout) and a value of five seconds for the reply
   * timeout.
   *
   * The constructor creates an instance of MrfUdpIpClient that takes care of
   * the low-level communication with the device.
   *
   * Throws an exception if the client cannot be created because the socket
   * cannot be initialized, the background threads cannot be created, or if one
   * of the parameters is invalid.
   */
  MrfUdpIpMemoryAccessV2(
    const std::string &hostName, std::uint32_t baseAddress
  );

  /**
   * Creates a memory-access object for an MRF device that can be controlled
   * via UDP/IP.
   *
   * The specified host name can either be a DNS name or an IP address.
   *
   * The base address is added to the address specified in a read or write
   * request. This is useful if different devices with the same register layout
   * but different base addresses shall be accessed.
   *
   * The queue timeout is the time between queuing a request and it being
   * discarded because it could not be sent. Once a request has been sent for
   * the first time, this timeout does not apply any longer and the request
   * timeout takes over instead. The queue timeout can be zero, meaning that
   * requests are kept in the queue indefinitely, until they finally can be
   * sent (or a are discarded because the peer is offline).
   *
   * The request timeout is the time between sending a packet for a request for
   * the first time and the request failing with a timeout if no valid response
   * is received. Within this time, retransmissions of the request may happen
   * automatically. The request timeout must not be zero.
   *
   * The constructor creates an instance of MrfUdpIpClient that takes care of
   * the low-level communication with the device.
   *
   * Throws an exception if the client cannot be created because the socket
   * cannot be initialized, the background threads cannot be created, or if one
   * of the parameters is invalid.
   */
  MrfUdpIpMemoryAccessV2(
    const std::string &hostName,
    std::uint32_t baseAddress,
    const std::chrono::duration<double> &queueTimeout,
    const std::chrono::duration<double> &requestTimeout
  );

  /**
   * Destructor. Shuts down and destroys the underlying UDP client.
   */
  virtual ~MrfUdpIpMemoryAccessV2();

  /**
   * Reads from an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void readUInt32(
    std::uint32_t address, std::shared_ptr<CallbackUInt32> callback
  );

  /**
   * Writes to an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void writeUInt32(
    std::uint32_t address, std::uint32_t value, std::shared_ptr<CallbackUInt32>
  );

  // We want the methods from the base class to participate in overload
  // resolution.
  using MrfUdpIpMemoryAccess::readUInt16;
  using MrfUdpIpMemoryAccess::readUInt32;
  using MrfUdpIpMemoryAccess::writeUInt16;
  using MrfUdpIpMemoryAccess::writeUInt32;

private:

  /**
   * Internal callback for a uint32 read or write request.
   */
  struct UInt32Callback: MrfUdpIpClient::RequestCallback32 {
    std::uint32_t address;
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback;

    UInt32Callback(
      std::uint32_t address,
      std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback
    );

    void operator()(
      std::uint32_t receivedData,
      std::int8_t receivedStatus,
      std::exception_ptr exception
    );
  };

  // We do not want to allow copy or move construction or assignment.
  MrfUdpIpMemoryAccessV2(const MrfUdpIpMemoryAccess &) = delete;
  MrfUdpIpMemoryAccessV2(MrfUdpIpMemoryAccess &&) = delete;
  MrfUdpIpMemoryAccessV2 &operator=(const MrfUdpIpMemoryAccess &) = delete;
  MrfUdpIpMemoryAccessV2 &operator=(MrfUdpIpMemoryAccess &&) = delete;

};

} // namespace mrf
} // namespace anka

#endif // ANKA_MRF_UDP_IP_MEMORY_ACCESS_V2_H
