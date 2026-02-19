/*
 * Copyright 2025-2026 aquenos GmbH.
 * Copyright 2025-2026 Karlsruhe Institute of Technology.
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
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <system_error>

extern "C" {
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
} // extern "C"

#include "MrfUdpPacket.h"

namespace anka {
namespace mrf {

MrfUdpPacket::MrfUdpPacket() {
}

MrfUdpPacket::MrfUdpPacket(
  std::uint8_t accessType,
  std::uint32_t address,
  std::uint16_t data,
  std::uint32_t ref,
  std::int8_t status
) :
  protocolVersion(ProtocolVersion::V1)
{
  this->packet.v1.accessType = accessType;
  this->packet.v1.address = htonl(address);
  this->packet.v1.data = htonl(data);
  this->packet.v1.ref = htonl(ref);
  this->packet.v1.status = status;
}

MrfUdpPacket::MrfUdpPacket(
  std::uint8_t accessType,
  std::uint32_t address,
  std::uint32_t data,
  std::uint32_t ref,
  std::int8_t status
) :
  protocolVersion(ProtocolVersion::V2)
{
  this->packet.v2.accessType = accessType;
  this->packet.v2.address = htonl(address);
  this->packet.v2.data = htonl(data);
  this->packet.v2.ref = htonl(ref);
  this->packet.v2.reserved = 0;
  this->packet.v2.status = status;
}

MrfUdpPacket::MrfUdpPacket(const MrfUdpPacket &copyFrom) {
  *this = copyFrom;
}

MrfUdpPacket &MrfUdpPacket::operator=(const MrfUdpPacket &assignFrom) {
  this->packet = assignFrom.packet;
  return *this;
}

std::uint8_t MrfUdpPacket::getAccessType() const {
  switch (this->protocolVersion) {
  case ProtocolVersion::V1:
    return this->packet.v1.accessType;
  case ProtocolVersion::V2:
    return this->packet.v2.accessType;
  }
  throw std::logic_error("Unhandled protocol version");
}

std::uint32_t MrfUdpPacket::getAddress() const {
  switch (this->protocolVersion) {
  case ProtocolVersion::V1:
    return ntohl(this->packet.v1.address);
  case ProtocolVersion::V2:
    return ntohl(this->packet.v2.address);
  }
  throw std::logic_error("Unhandled protocol version");
}

std::uint32_t MrfUdpPacket::getData() const {
  switch (this->protocolVersion) {
  case ProtocolVersion::V1:
    return ntohs(this->packet.v1.data);
  case ProtocolVersion::V2:
    return ntohl(this->packet.v2.data);
  }
  throw std::logic_error("Unhandled protocol version");
}

std::uint32_t MrfUdpPacket::getRef() const {
  switch (this->protocolVersion) {
  case ProtocolVersion::V1:
    return ntohl(this->packet.v1.ref);
  case ProtocolVersion::V2:
    return ntohl(this->packet.v2.ref);
  }
  throw std::logic_error("Unhandled protocol version");
}

std::int8_t MrfUdpPacket::getStatus() const {
  switch (this->protocolVersion) {
  case ProtocolVersion::V1:
    return this->packet.v1.status;
  case ProtocolVersion::V2:
    return this->packet.v2.status;
  }
  throw std::logic_error("Unhandled protocol version");
}

void MrfUdpPacket::receive(int socket) {
  // The buffer is a (slightly larger) overlay over the packet data structure,
  // so by reading into the buffer, we also read into the packet data
  // structure.
  ::ssize_t numberOfBytesRead = ::read(
    socket, this->buffer, sizeof(this->buffer));
  if (numberOfBytesRead == -1) {
    throw std::system_error(errno, std::generic_category());
  }
  if (numberOfBytesRead == sizeof(OnWirePacketV1)) {
    this->protocolVersion = ProtocolVersion::V1;
  } else if (numberOfBytesRead == sizeof(OnWirePacketV2)) {
    this->protocolVersion = ProtocolVersion::V2;
  } else {
    throw std::range_error("Received UDP packet has wrong size.");
  }
}

void MrfUdpPacket::send(int socket, int flags) const {
  // The initialization here is not really needed because one of the branches
  // that set the variable later should always apply, but GCC cannot detect
  // this, so we initialize the variable in order to avoid a compiler warning.
  std::size_t packetSize = 0;
  switch (this->protocolVersion) {
  case ProtocolVersion::V1:
    packetSize = sizeof(OnWirePacketV1);
    break;
  case ProtocolVersion::V2:
    packetSize = sizeof(OnWirePacketV2);
    break;
  }
  if (::send(socket, &this->packet, packetSize, flags) == -1) {
    throw std::system_error(
      errno, std::generic_category(), "Send operation failed");
  }
}

void MrfUdpPacket::setRef(std::uint32_t ref) {
  switch (this->protocolVersion) {
  case ProtocolVersion::V1:
    this->packet.v1.ref = htonl(ref);
  case ProtocolVersion::V2:
    this->packet.v2.ref = htonl(ref);
  }
}

MrfUdpPacket::MrfUdpPacket(
  ProtocolVersion protocolVersion, OnWirePacket &&packet
) {
  this->packet = packet;
  this->protocolVersion = protocolVersion;
}

} // namespace mrf
} // namespace anka
