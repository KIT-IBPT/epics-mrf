/*
 * Copyright 2015-2024 aquenos GmbH.
 * Copyright 2015-2024 Karlsruhe Institute of Technology.
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
#include <cstring>
#include <stdexcept>
#include <system_error>

extern "C" {
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
}

#include <mrfErrorUtil.h>

#include "MrfUdpIpMemoryAccess.h"

namespace anka {
namespace mrf {

// The constants are initialized in the class definition, so we only have to
// define them here so that they get storage assigned (otherwise, the linker
// will complain about missing symbols when they are used by reference).
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvgCrCsr;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvgRegister;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvrCrCsr;
constexpr std::uint32_t MrfUdpIpMemoryAccess::baseAddressVmeEvrRegister;

MrfUdpIpMemoryAccess::MrfUdpIpMemoryAccess(const std::string &hostName,
    std::uint32_t baseAddress) :
    MrfUdpIpMemoryAccess(hostName, baseAddress, MrfTime(0, 400000),
        MrfTime(0, 5000000), 5) {
}

MrfUdpIpMemoryAccess::MrfUdpIpMemoryAccess(const std::string &hostName,
    std::uint32_t baseAddress, const MrfTime &delayBetweenPackets,
    const MrfTime &udpTimeout, int maximumNumberOfTries) :
    hostName(hostName), baseAddress(baseAddress), shutdown(false), delayBetweenPackets(
        delayBetweenPackets), udpTimeout(udpTimeout), maximumNumberOfTries(
        maximumNumberOfTries) {
  if (delayBetweenPackets.getSeconds() < 0) {
    throw std::invalid_argument(
        "The delay between packets must not be negative.");
  }
  if (udpTimeout.getSeconds() < 0) {
    throw std::invalid_argument("The UDP timeout must not be negative.");
  }
  if (maximumNumberOfTries < 1) {
    throw std::invalid_argument(
        "The maximum number of tries must be greater than zero.");
  }
  // Resolve the host name.
  ::addrinfo addrInfoHint = { 0, PF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, nullptr,
      nullptr, nullptr };
  ::addrinfo *addrInfo = nullptr;
  int addrInfoStatus = ::getaddrinfo(hostName.c_str(), nullptr, &addrInfoHint,
      &addrInfo);
  if (addrInfoStatus) {
    throw std::system_error(addrInfoStatus, std::system_category(),
        std::string("Could not resolve ") + hostName + ": "
            + ::gai_strerror(addrInfoStatus));
  }
  sockaddr_in socketAddress;
  bool haveSocketAddress = false;
  ::addrinfo *nextAddrInfo = addrInfo;
  while (!haveSocketAddress && nextAddrInfo) {
    if (addrInfo->ai_addrlen == sizeof(sockaddr_in)) {
      haveSocketAddress = true;
      std::memcpy(&socketAddress, addrInfo->ai_addr, sizeof(sockaddr_in));
    }
    nextAddrInfo = nextAddrInfo->ai_next;
  }
  ::freeaddrinfo(addrInfo);
  addrInfo = nullptr;
  if (!haveSocketAddress) {
    throw new std::runtime_error(
        "Addressed returned by getaddrinfo had an unexpected size.");
  }

  // Create and connect the socket.
  this->socketDescriptor = ::socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (this->socketDescriptor == -1) {
    throw systemErrorFromErrNo(
        "Could not create UDP socket for communication with " + hostName);
  }
  if (::fcntl(this->socketDescriptor, F_SETFL, O_NONBLOCK) == -1) {
    int savedErrorNumber = errno;
    ::close(this->socketDescriptor);
    this->socketDescriptor = -1;
    throw systemErrorForErrNo("Could not put socket into non-blocking mode",
        savedErrorNumber);
  }
  socketAddress.sin_port = htons(2000);
  if (::connect(this->socketDescriptor,
      reinterpret_cast<sockaddr *>(&socketAddress), sizeof(sockaddr_in))) {
    int savedErrorNumber = errno;
    close(this->socketDescriptor);
    this->socketDescriptor = -1;
    throw systemErrorForErrNo(
        "Could not connect UDP socket for communication with " + hostName,
        savedErrorNumber);
  }
  // Create background threads.
  try {
    this->receiveThread = std::thread([this]() {runReceiveThread();});
    this->sendThread = std::thread([this]() {runSendThread();});
  } catch (...) {
    close(this->socketDescriptor);
    this->socketDescriptor = -1;
    throw;
  }
}

MrfUdpIpMemoryAccess::~MrfUdpIpMemoryAccess() {
  // Close the connection and terminate the background threads.
  try {
    {
      std::unique_lock<std::recursive_mutex> lock;
      shutdown.store(true, std::memory_order_release);
      receiveSelector.wakeUp();
      sendSelector.wakeUp();
    }
    if (sendThread.joinable()) {
      sendThread.join();
    }
    if (receiveThread.joinable()) {
      receiveThread.join();
    }
  } catch (...) {
    // A destructor should never throw and we also want to make sure that the
    // socket is closed.
  }
  if (socketDescriptor != -1) {
    ::close(socketDescriptor);
  }
  socketDescriptor = -1;
}

static MrfMemoryAccess::ErrorCode statusToErrorCode(std::int8_t status) {
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

MrfUdpIpMemoryAccess::UInt16Callback::UInt16Callback(std::uint32_t address,
    std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback) :
    address(address), callback(callback) {
}

void MrfUdpIpMemoryAccess::UInt16Callback::operator()(
    std::uint16_t receivedData, std::int8_t status, bool timeout) {
  if (callback) {
    if (timeout) {
      callback->failure(this->address, ErrorCode::networkTimeout,
          std::string());
    } else if (status != 0) {
      callback->failure(this->address, statusToErrorCode(status),
          std::string());
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
    memoryAccess.queueReadRequest(address,
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
    std::uint16_t receivedData, std::int8_t status, bool timeout) {
  if (timeout) {
    sharedData->failure(ErrorCode::networkTimeout, std::string());
  } else if (status != 0) {
    sharedData->failure(statusToErrorCode(status), std::string());
  } else {
    sharedData->receivedLow(receivedData);
  }
}

MrfUdpIpMemoryAccess::UInt32ReadHighCallback::UInt32ReadHighCallback(
    std::shared_ptr<UInt32ReadShared> sharedData) :
    sharedData(sharedData) {
}

void MrfUdpIpMemoryAccess::UInt32ReadHighCallback::operator()(
    std::uint16_t receivedData, std::int8_t status, bool timeout) {
  if (timeout) {
    sharedData->failure(ErrorCode::networkTimeout, std::string());
  } else if (status != 0) {
    sharedData->failure(statusToErrorCode(status), std::string());
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
    std::uint16_t receivedData, std::int8_t status, bool timeout) {
  if (callback) {
    if (timeout) {
      callback->failure(address, ErrorCode::networkTimeout, std::string());
    } else if (status != 0) {
      callback->failure(address, statusToErrorCode(status), std::string());
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
    std::uint16_t receivedData, std::int8_t status, bool timeout) {
  if (timeout) {
    if (callback) {
      callback->failure(address, ErrorCode::networkTimeout, std::string());
    }
  } else if (status != 0) {
    if (callback) {
      callback->failure(address, statusToErrorCode(status), std::string());
    }
  } else {
    try {
      std::shared_ptr<UInt32WriteLowCallback> internalCallback =
          std::make_shared<UInt32WriteLowCallback>(address, receivedData,
              callback);
      memoryAccess.queueWriteRequest(address + 2, lowData, internalCallback);
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
  queueReadRequest(address, internalCallback);
}

void MrfUdpIpMemoryAccess::writeUInt16(std::uint32_t address,
    std::uint16_t value, std::shared_ptr<CallbackUInt16> callback) {
  std::shared_ptr<UInt16Callback> internalCallback = std::make_shared<
      UInt16Callback>(address, callback);
  queueWriteRequest(address, value, internalCallback);
}

void MrfUdpIpMemoryAccess::readUInt32(std::uint32_t address,
    std::shared_ptr<CallbackUInt32> callback) {
  std::shared_ptr<UInt32ReadShared> sharedData = std::make_shared<
      UInt32ReadShared>(*this, address, callback);
  // The low word should be read first.
  std::shared_ptr<UInt32ReadLowCallback> lowCallback = std::make_shared<
      UInt32ReadLowCallback>(sharedData);
  queueReadRequest(address + 2, lowCallback);
  // The high word should be read second. If we cannot queue the second read
  // request, we do not throw but call the failure method on the callback
  // shared data object instead. Otherwise, the callback might be called if the
  // request to read the lower word fails but the calling code will not expect
  // the callback to be called because from its perspective, the attempt to
  // queue the request failed.
  try {
    std::shared_ptr<UInt32ReadHighCallback> highCallback = std::make_shared<
        UInt32ReadHighCallback>(sharedData);
    queueReadRequest(address, highCallback);
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
  queueWriteRequest(address, highWord, internalCallback);
}

void MrfUdpIpMemoryAccess::queueReadRequest(std::uint32_t address,
    std::shared_ptr<MrfRequestCallback> callback) {
  MrfRequest request;
  request.packet.accessType = 1;
  request.packet.address = htonl(baseAddress + address);
  request.packet.data = 0;
  request.packet.status = 0;
  request.callback = callback;
  request.numberOfTries = 0;
  // We have to hold the mutex while incrementing the counter and modifying the
  // request queue.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    // We do not have to convert the byte order of the counter because this
    // field is not touched by the server but simply mirrored back.
    request.packet.ref = nextRequestCounter;
    ++nextRequestCounter;
    requestQueue.push_back(request);
    sendSelector.wakeUp();
  }
}

void MrfUdpIpMemoryAccess::queueWriteRequest(std::uint32_t address,
    std::uint16_t data, std::shared_ptr<MrfRequestCallback> callback) {
  MrfRequest request;
  request.packet.accessType = 2;
  request.packet.address = htonl(baseAddress + address);
  request.packet.data = htons(data);
  request.packet.status = 0;
  request.callback = callback;
  request.numberOfTries = 0;
  // We have to hold the mutex while incrementing the counter and modifying the
  // request queue.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    // We do not have to convert the byte order of the counter because this
    // field is not touched by the server but simply mirrored back.
    request.packet.ref = nextRequestCounter;
    ++nextRequestCounter;
    requestQueue.push_back(request);
    sendSelector.wakeUp();
  }
}

void MrfUdpIpMemoryAccess::runReceiveThread() {
  int numberOfConsecutiveReadFailures = 0;
  while (!shutdown.load(std::memory_order_acquire)) {
    ::fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(socketDescriptor, &readFds);
    receiveSelector.select(&readFds, nullptr, nullptr, socketDescriptor,
        nullptr);
    // We use a buffer that is slightly larger than needed so that we can detect
    // too large packets.
    char buffer[sizeof(MrfUdpPacket) + 4];
    int numberOfBytesRead = ::read(socketDescriptor, buffer, sizeof(buffer));
    if (numberOfBytesRead == -1) {
      // A EAGAIN error is not considered an error. The next select operation
      // should block until reading is possible again. We also ignore a
      // connection refused error because this simply means that the peer is
      // temporarily unavailable.
      if (errno != EAGAIN && errno != ECONNREFUSED) {
        // We count the number of consecutive errors. If this number gets too
        // high, it is very likely that we have a non-recoverable problem and
        // we better stop the receive thread. We do not stop the send thread
        // because we want requests to be processed and result in a timeout.
        ++numberOfConsecutiveReadFailures;
        if (numberOfConsecutiveReadFailures >= 50) {
          break;
        }
      }
      continue;
    }
    // Reset the error counter.
    numberOfConsecutiveReadFailures = 0;
    if (numberOfBytesRead != sizeof(MrfUdpPacket)) {
      // Ignore packets with an odd size.
      continue;
    }
    MrfUdpPacket *packet = reinterpret_cast<MrfUdpPacket *>(buffer);
    packet->data = ntohs(packet->data);
    packet->address = ntohl(packet->address);
    // We do not swap the reference field because it contains the same sequence
    // of bytes that was sent by us.
    MrfRequest request;
    {
      // We have to hold the mutex while modifying the pendingRequests
      // structure.
      std::lock_guard<std::recursive_mutex> lock(mutex);
      auto elementIterator = pendingRequests.find(packet->ref);
      if (elementIterator != pendingRequests.end()) {
        request = elementIterator->second;
        pendingRequests.erase(elementIterator);
      } else {
        // If we cannot find the request it probably timed out, so we simply
        // ignore the packet that we just received.
        continue;
      }
    }
    // We call the callback without holding the mutex in order to avoid a dead
    // lock.
    if (request.callback) {
      try {
        (*request.callback)(packet->data, packet->status, false);
      } catch (...) {
        // We catch all errors so that an exception that is thrown by a callback
        // does not stop the receive thread.
      }
    }
  }
}

void MrfUdpIpMemoryAccess::runSendThread() {
  MrfTime nextSendTime;
  bool needTimeoutCheck = false;
  MrfTime nextTimeoutCheckTime;
  while (!shutdown.load(std::memory_order_acquire)) {
    // We get the current time here so that we can avoid unnecessary system
    // calls and the current time used is consistent among the whole code.
    MrfTime now = MrfTime::now();
    // Check whether any pending requests have timed out. Checking all pending
    // requests is not the most sufficient way to do this, however it frees us
    // from the burden to maintain a second data structure and usually the
    // number of pending requests should be small.
    if (needTimeoutCheck) {
      if (nextTimeoutCheckTime <= now) {
        // We need to hold the mutex while modifying pendingRequests and
        // requestQueue.
        std::lock_guard<std::recursive_mutex> lock(mutex);
        needTimeoutCheck = false;
        for (auto pendingRequestIterator = pendingRequests.begin();
            pendingRequestIterator != pendingRequests.end();) {
          MrfRequest &request = pendingRequestIterator->second;
          // If a request timed out, we add it back to the queue. Otherwise, we
          // schedule a timeout check that should be run when the request times
          // out. We do not check whether the maximum number of tries has been
          // reached. We will do this later when processing the request. This
          // way, we can avoid to hold the mutex when calling the callback.
          if (request.timeout <= now) {
            requestQueue.push_back(request);
            pendingRequestIterator = pendingRequests.erase(
                pendingRequestIterator);
          } else {
            if (!needTimeoutCheck) {
              needTimeoutCheck = true;
              nextTimeoutCheckTime = request.timeout;
            } else {
              if (nextTimeoutCheckTime > request.timeout) {
                nextTimeoutCheckTime = request.timeout;
              }
            }
            // We increment the iterator here instead of doing this in the for
            // header because we do not want to increment the iterator if it
            // has been incremented by removing the current element.
            pendingRequestIterator++;
          }
        }
      }
    }
    bool queueEmpty;
    bool haveRequest;
    MrfRequest request;
    {
      std::lock_guard<std::recursive_mutex> lock(mutex);
      queueEmpty = requestQueue.empty();
      if (!queueEmpty) {
        request = requestQueue.front();
        haveRequest = true;
      } else {
        haveRequest = false;
      }
    }
    bool delayNextSend = (nextSendTime > now);
    if (haveRequest && request.numberOfTries >= maximumNumberOfTries) {
      // We remove the request from the queue and notify the callback.
      {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        requestQueue.pop_front();
        queueEmpty = requestQueue.empty();
      }
      // We call the callback without holding the mutex in order to avoid a dead
      // lock.
      if (request.callback) {
        try {
          (*request.callback)(0, 0, true);
        } catch (...) {
          // We catch all errors so that an exception that is thrown by a
          // callback does not stop the receive thread.
        }
      }
    } else if (haveRequest && !delayNextSend) {
      int bytesSent = ::send(socketDescriptor, &request.packet,
          sizeof(MrfUdpPacket), 0);
      if (bytesSent == -1) {
        // An error code of EAGAIN is not considered an error. It just means
        // that the packet could not be sent immediately, so we keep it in the
        // queue and try again after the next select operation. For other error
        // codes we treat the request like it had been sent (incrementing the
        // try counter). This means that eventually, the request will time out
        // and be tried again until the maximum number of tries is reached.
        if (errno != EAGAIN) {
          request.numberOfTries += 1;
          request.timeout = MrfTime::now() + udpTimeout;
          if (!needTimeoutCheck) {
            needTimeoutCheck = true;
            nextTimeoutCheckTime = request.timeout;
          }
          // Remove request from queue and add it to the list of pending
          // requests.
          {
            std::lock_guard<std::recursive_mutex> lock(mutex);
            pendingRequests.insert(std::make_pair(request.packet.ref, request));
            requestQueue.pop_front();
            queueEmpty = requestQueue.empty();
          }
        }
      } else {
        MrfTime newNow = MrfTime::now();
        request.numberOfTries += 1;
        request.timeout = newNow + udpTimeout;
        if (!needTimeoutCheck) {
          needTimeoutCheck = true;
          nextTimeoutCheckTime = request.timeout;
        }
        nextSendTime = newNow + delayBetweenPackets;
        // Remove request from queue and add it to the list of pending requests.
        {
          std::lock_guard<std::recursive_mutex> lock(mutex);
          pendingRequests.insert(std::make_pair(request.packet.ref, request));
          requestQueue.pop_front();
          queueEmpty = requestQueue.empty();
        }
      }
    }
    // We want the select operation to be cancelled when we have to take care of
    // another action (sending the next packet or checking for timeouts).
    bool needAction;
    MrfTime nextActionTime;
    if (delayNextSend && !queueEmpty && needTimeoutCheck) {
      if (nextSendTime < nextTimeoutCheckTime) {
        nextActionTime = nextSendTime;
      } else {
        nextActionTime = nextTimeoutCheckTime;
      }
      needAction = true;
    } else if (delayNextSend && !queueEmpty) {
      nextActionTime = nextSendTime;
      needAction = true;
    } else if (needTimeoutCheck) {
      nextActionTime = nextTimeoutCheckTime;
      needAction = true;
    } else {
      needAction = false;
    }
    ::timeval waitTime;
    if (needAction) {
      // We use the current time for the check because some time might have
      // passed since now was initialized.
      MrfTime newNow = MrfTime::now();
      if (nextActionTime > newNow) {
        waitTime = nextActionTime - newNow;
      } else {
        // If we need immediate action, we can skip the select call and continue
        // right away.
        continue;
      }
    }
    ::fd_set writeFds;
    FD_ZERO(&writeFds);
    if (!queueEmpty && !delayNextSend) {
      FD_SET(socketDescriptor, &writeFds);
    }
    sendSelector.select(nullptr, &writeFds, nullptr, socketDescriptor,
        needAction ? &waitTime : nullptr);
  }
}

}
}
