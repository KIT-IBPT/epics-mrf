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

#ifndef ANKA_MRF_UDP_IP_MEMORY_ACCESS_H
#define ANKA_MRF_UDP_IP_MEMORY_ACCESS_H

#include <chrono>
#include <cstdint>
#include <string>

#include <MrfMemoryAccess.h>
#include "MrfUdpIpClient.h"

namespace anka {
namespace mrf {

/**
 * MRF memory access implementation that provides access to an MRF device
 * through the UDP/IP protocol. This is the base class that is shared by the
 * implementations for both protocol versions 1 and 2.
 */
class MrfUdpIpMemoryAccess: public MrfMemoryAccess {

public:

  /**
   * Base address of the CR/CSR space in the VME-EVG-230.
   */
  static constexpr std::uint32_t baseAddressVmeEvgCrCsr = 0x00000000;

  /**
   * Base address of the EVG registers space in the VME-EVG-230.
   */
  static constexpr std::uint32_t baseAddressVmeEvgRegister = 0x80000000;

  /**
   * Base address of the CR/CSR space in the VME-EVM-300.
   */
  static constexpr std::uint32_t baseAddressVmeEvmCrCsr = 0x00000000;

  /**
   * Base address of the EVM registers space in the VME-EVM-300.
   */
  static constexpr std::uint32_t baseAddressVmeEvmRegister = 0x80000000;

  /**
   * Base address of the CR/CSR space in the VME-EVR-230(RF).
   */
  static constexpr std::uint32_t baseAddressVmeEvr230CrCsr = 0x78000000;

  /**
   * Base address of the EVR registers space in the VME-EVR-230(RF).
   */
  static constexpr std::uint32_t baseAddressVmeEvr230Register = 0x7a000000;

  /**
   * Base address of the CR/CSR space in the VME-EVR-300.
   */
  static constexpr std::uint32_t baseAddressVmeEvr300CrCsr = 0x00000000;

  /**
   * Base address of the EVR registers space in the VME-EVR-300.
   */
  static constexpr std::uint32_t baseAddressVmeEvr300Register = 0x80000000;

  /**
   * Destructor. Shuts down and destroys the underlying UDP client.
   */
  virtual ~MrfUdpIpMemoryAccess();

  /**
   * Reads from an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void readUInt16(
    std::uint32_t address, std::shared_ptr<CallbackUInt16> callback
  );

  /**
   * Writes to an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void writeUInt16(
    std::uint32_t address,
    std::uint16_t value,
    std::shared_ptr<CallbackUInt16>,
    ReadbackMode readbackMode = ReadbackMode::must
  );

  // We want the methods from the base class to participate in overload
  // resolution.
  using MrfMemoryAccess::readUInt16;
  using MrfMemoryAccess::readUInt32;
  using MrfMemoryAccess::writeUInt16;
  using MrfMemoryAccess::writeUInt32;

protected:

  using ProtocolVersion = MrfUdpIpClient::ProtocolVersion;

  /**
   * Creates a memory-access object for an MRF device that can be controlled
   * via UDP/IP.
   *
   * The protocol version does not only define the on-wire format but also
   * defines the feature set that is available. When using protocol version 1,
   * 32-bit operations and write operations without readback are not supported.
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
  MrfUdpIpMemoryAccess(
    ProtocolVersion protocolVersion,
    std::string const &hostName,
    std::uint32_t baseAddress
  );

  /**
   * Creates a memory-access object for an MRF device that can be controlled
   * via UDP/IP.
   *
   * The protocol version does not only define the on-wire format but also
   * defines the feature set that is available. When using protocol version 1,
   * 32-bit operations and write operations without readback are not supported.
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
  MrfUdpIpMemoryAccess(
    ProtocolVersion protocolVersion,
    const std::string &hostName,
    std::uint32_t baseAddress,
    const std::chrono::duration<double> &queueTimeout,
    const std::chrono::duration<double> &requestTimeout
  );

  /**
   * Base address for memory access.
   *
   * This is applied as an offset for all addresses that are specified in
   * read or write operations.
   */
  std::uint32_t const baseAddress;

  /**
   * Client for the UDP/IP protocol.
   */
  MrfUdpIpClient client;

  /**
   * Converts an exception that is received by a callback into an error code.
   */
  static std::pair<
    MrfMemoryAccess::ErrorCode, std::string
  > exceptionToErrorCodeAndMessage(std::exception_ptr exception);

  /**
   * Converts a status code that is received by a callback into an error code.
   */
  static MrfMemoryAccess::ErrorCode statusToErrorCode(std::int8_t status);

private:

  /**
   * Internal callback for a uint16 read or write request.
   */
  struct UInt16Callback: MrfUdpIpClient::RequestCallback16 {
    std::uint32_t address;
    std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback;

    UInt16Callback(
      std::uint32_t address,
      std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback
    );

    void operator()(
      std::uint16_t receivedData,
      std::int8_t receivedStatus,
      std::exception_ptr exception
    );
  };

  // We do not want to allow copy or move construction or assignment.
  MrfUdpIpMemoryAccess(const MrfUdpIpMemoryAccess &) = delete;
  MrfUdpIpMemoryAccess(MrfUdpIpMemoryAccess &&) = delete;
  MrfUdpIpMemoryAccess &operator=(const MrfUdpIpMemoryAccess &) = delete;
  MrfUdpIpMemoryAccess &operator=(MrfUdpIpMemoryAccess &&) = delete;

};

} // namespace mrf
} // namespace anka

#endif // ANKA_MRF_UDP_IP_MEMORY_ACCESS_H
