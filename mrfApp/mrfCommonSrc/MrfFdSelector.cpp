/*
 * Copyright 2015-2016 aquenos GmbH.
 * Copyright 2015-2016 Karlsruhe Institute of Technology.
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

#include <cerrno>
#include <system_error>

extern "C" {
#include <fcntl.h>
#include <unistd.h>
}

#include "MrfFdSelector.h"

namespace anka {
namespace mrf {

MrfFdSelector::MrfFdSelector() {
  int fileDescriptors[2];
  if (::pipe(fileDescriptors)) {
    throw std::system_error(
      errno,
      std::generic_category(),
      "Could not create pipe for the FD selector");
  }
  this->readFd = fileDescriptors[0];
  this->writeFd = fileDescriptors[1];
  if ((::fcntl(this->readFd, F_SETFL, O_NONBLOCK) == -1)
      || (::fcntl(this->writeFd, F_SETFL, O_NONBLOCK) == -1)) {
    int savedErrorNumber = errno;
    ::close(this->readFd);
    this->readFd = -1;
    ::close(this->writeFd);
    this->writeFd = -1;
    throw std::system_error(
      savedErrorNumber,
      std::generic_category(),
      "Could not put pipe FD into non-blocking mode");
  }
}

MrfFdSelector::~MrfFdSelector() {
  if (readFd != -1) {
    ::close(readFd);
    readFd = -1;
  }
  if (writeFd != -1) {
    ::close(writeFd);
    writeFd = -1;
  }
}

void MrfFdSelector::select(::fd_set *readFds, ::fd_set *writeFds,
    ::fd_set *errorFds, int maxFd, ::timeval *timeout) {
  ::fd_set myReadFds;
  if (!readFds) {
    FD_ZERO(&myReadFds);
    readFds = &myReadFds;
  }
  FD_SET(readFd, readFds);
  if (readFd > maxFd) {
    maxFd = readFd;
  }
  if (::select(maxFd + 1, readFds, writeFds, errorFds, timeout) == -1) {
    throw std::system_error(
      errno, std::generic_category(), "Select operation failed");
  }
  // If the read FD is flagged, we should consume all bytes so that the next
  // select call will work as expected.
  if (FD_ISSET(readFd, readFds)) {
    char c;
    while (::read(readFd, &c, 1) == 1)
      ;
    FD_CLR(readFd, readFds);
  }
}

void MrfFdSelector::wakeUp() {
  char c = 0;
  if (::write(writeFd, &c, 1) == -1) {
    // The write failing because it would block is not considered an error. This
    // can happen when wakeUp() is called repeatedly without a select(...) call
    // in between. In this case the buffer will fill up. Still, the next
    // select(...) call will return immediately because the buffer is not empty.
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    throw std::system_error(
      errno, std::generic_category(), "Write to pipe failed");
  }
}

} // namespace mrf
} // namespace anka
