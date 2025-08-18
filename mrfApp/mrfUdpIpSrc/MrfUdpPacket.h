/*
 * Copyright 2025 aquenos GmbH.
 * Copyright 2025 Karlsruhe Institute of Technology.
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

#ifndef ANKA_MRF_UDP_PACKET_H
#define ANKA_MRF_UDP_PACKET_H

#include <cstdint>

namespace anka {
namespace mrf {

/**
 * Packet that can be send to a MRF device via UDP or that can be received from
 * such a device.
 */
class MrfUdpPacket {

public:

  /**
   * Default constructor.
   *
   * An instance that is constructed this way has fields that are undefined, so
   * it is only useful for a subsequent receive operation or for being assigned
   * from another instance.
   */
  MrfUdpPacket();

  /**
   * Creates a packet with its fields initialized to the specified values.
   */
  MrfUdpPacket(
      std::uint8_t accessType,
      std::uint32_t address,
      std::uint16_t data,
      std::uint32_t ref,
      std::int8_t status);

  /**
   * Creates a copy of an existing instance.
   */
  MrfUdpPacket(const MrfUdpPacket &copyFrom);

  /**
   * Updates the fields of this packet with the values of the fields of the
   * specified packet.
   */
  MrfUdpPacket &operator=(const MrfUdpPacket &assignFrom);

  /**
   * Returns the access type.
   */
  std::uint8_t getAccessType() const;

  /**
   * Returns the address type.
   */
  std::uint32_t getAddress() const;

  /**
   * Returns the data.
   */
  std::uint16_t getData() const;

  /**
   * Returns the reference.
   */
  std::uint32_t getRef() const;

  /**
   * Returns the status.
   */
  std::int8_t getStatus() const;

  /**
   * Receives a packet from the specified socket.
   *
   * The data received from the socket is assigned to this instance.
   *
   * Throws an std::system_error if the operating system reports an error.
   * Throws an std::range_error if the packet that is received does not match
   * the expected length.
   */
  void receive(int socket);

  /**
   * Sends the packet over the specified socket.
   *
   * Throws an std::system_error if the send operation fails.
   */
  void send(int socket, int flags) const;

  /**
   * Sets the reference.
   */
  void setRef(std::uint32_t ref);

private:

  /**
   * Data structure for the on-wire format of a packet.
   */
// We have to pack the structure so that it matches the network representation.
#pragma pack(push, 1)
  struct OnWirePacket {
    std::uint8_t accessType;
    std::int8_t status;
    std::uint16_t data;
    std::uint32_t address;
    std::uint32_t ref;
  };
#pragma pack(pop)

  /**
   * Construct a packet instance from the on-wire representation, possibly
   * invalidating the original instance. This constructor is only intended for
   * internal use by the receive(…) function.
   */
  MrfUdpPacket(OnWirePacket &&packet);

  /**
   * On-wire representation of the packet.
   *
   * We reserve a bit more space than we actually need, so that we can
   * (potentially) read extra bytes in order to detect that a received packet
   * was too large.
   */
  union {
    char buffer[sizeof(OnWirePacket) + 4];
    OnWirePacket packet;
  };

};

} // namespace mrf
} // namespace anka

#endif // ANKA_MRF_UDP_PACKET_H
