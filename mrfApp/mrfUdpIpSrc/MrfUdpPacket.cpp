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
    std::int8_t status) {
  this->packet.accessType = accessType;
  this->packet.address = htonl(address);
  this->packet.data = htonl(data);
  this->packet.ref = htonl(ref);
  this->packet.status = status;
}

MrfUdpPacket::MrfUdpPacket(const MrfUdpPacket &copyFrom) {
  *this = copyFrom;
}

MrfUdpPacket &MrfUdpPacket::operator=(const MrfUdpPacket &assignFrom) {
  this->packet = assignFrom.packet;
  return *this;
}

std::uint8_t MrfUdpPacket::getAccessType() const {
  return this->packet.accessType;
}

std::uint32_t MrfUdpPacket::getAddress() const {
  return ntohl(this->packet.address);
}

std::uint16_t MrfUdpPacket::getData() const {
  return ntohs(this->packet.data);
}

std::uint32_t MrfUdpPacket::getRef() const {
  return ntohl(this->packet.ref);
}

std::int8_t MrfUdpPacket::getStatus() const {
  return this->packet.status;
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
  if (numberOfBytesRead != sizeof(OnWirePacket)) {
    throw std::range_error("Received UDP packet has wrong size.");
  }
}

void MrfUdpPacket::send(int socket, int flags) const {
  if (::send(socket, &this->packet, sizeof(OnWirePacket), flags) == -1) {
    throw std::system_error(
      errno, std::generic_category(), "Send operation failed");
  }
}

void MrfUdpPacket::setRef(std::uint32_t ref) {
  this->packet.ref = htonl(ref);
}

MrfUdpPacket::MrfUdpPacket(OnWirePacket &&packet) {
  this->packet = packet;
}

} // namespace mrf
} // namespace anka
