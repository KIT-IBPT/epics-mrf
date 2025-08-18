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

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <ratio>
#include <stdexcept>
#include <system_error>

extern "C" {
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
}

#include <mrfGaiErrorCategory.h>

#include "MrfUdpIpClient.h"

namespace anka {
namespace mrf {

namespace {

bool refGreaterThan(std::uint32_t ref1, std::uint32_t ref2) {
  // This function determines whether the packet using ref1 was sent after the
  // packet using ref2.
  // As the ref counter wraps, we cannot simply check whether ref1 is greater
  // than ref2. Instead, we calculate the difference and if this is less than
  // than half of the range of the refs, we can assume ref1 is greater than
  // ref2. Due to the wrapping behavior of unsigned integer types, if ref2 is
  // slightly greater than ref1, the difference is going to be very close to
  // the maximum value that can be represented by the type.
  auto refDiff = ref1 - ref2;
  return refDiff < std::numeric_limits<std::uint32_t>::max() / 2;
}

template<class Rep, class Period>
::timeval toTimeval(std::chrono::duration<Rep, Period> duration) {
  ::timeval converted;
  using TimevalSeconds = std::chrono::duration<
    decltype(converted.tv_sec), std::ratio<1>>;
  using TimevalMicroseconds = std::chrono::duration<
    decltype(converted.tv_usec), std::micro>;
  converted.tv_sec = std::chrono::duration_cast<TimevalSeconds>(
    duration).count();
  // If the original duration has better than second precision, we also
  // calculate the microseconds part. Otherwise, we set the microseconds part
  // to zero.
  if (std::ratio_less<Period, std::ratio<1>>::value) {
    constexpr auto oneSecond = std::chrono::duration<Rep, Period>(
      std::chrono::seconds(1));
    Rep fractionalPart = duration.count() % oneSecond.count();
    converted.tv_usec = std::chrono::duration_cast<TimevalMicroseconds>(
      std::chrono::duration<Rep, Period>(fractionalPart)).count();
  } else {
    converted.tv_usec = 0;
  }
  return converted;
}

} // anonymous namespace

MrfUdpIpClient::MrfUdpIpClient(
    const std::string &hostName,
    const Clock::duration &queueTimeout,
    const Clock::duration &requestTimeout) :
    hostName(hostName),
    queueTimeout(queueTimeout),
    requestTimeout(requestTimeout),
    shutdown(false) {
  if (queueTimeout < Clock::duration::zero()) {
    throw std::invalid_argument(
      "The queue timeout must be zero or positive.");
  }
  if (requestTimeout <= Clock::duration::zero()) {
    throw std::invalid_argument(
      "The request timeout must be strictly positive.");
  }
  // Resolve the host name.
  ::addrinfo addrInfoHint = {
    0, PF_INET, SOCK_DGRAM, IPPROTO_UDP, 0, nullptr, nullptr, nullptr};
  ::addrinfo *addrInfo = nullptr;
  int addrInfoStatus = ::getaddrinfo(
    hostName.c_str(), nullptr, &addrInfoHint, &addrInfo);
  if (addrInfoStatus) {
    throw std::system_error(
      addrInfoStatus,
      mrfGaiErrorCategory(),
      "Could not resolve " + hostName);
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
    throw std::system_error(
      errno,
      std::generic_category(),
      "Could not create UDP socket for communication with " + hostName);
  }
  if (::fcntl(this->socketDescriptor, F_SETFL, O_NONBLOCK) == -1) {
    int savedErrorNumber = errno;
    ::close(this->socketDescriptor);
    this->socketDescriptor = -1;
    throw std::system_error(
      savedErrorNumber,
      std::generic_category(),
      "Could not put socket into non-blocking mode");
  }
#ifdef __APPLE__
  // Unlike Linux, macOS may generate a SIGPIPE for a datagram socket, if it is
  // connected (Linux only does this for stream sockets). Such a signal would
  // kill the whole process, so we set a flag that keeps the operating system
  // from generating such a signal. The SO_NOSIGPIPE option is only supported
  // on macOS and some variants of BSD, so we cannot apply it everywhere.
  int socketOptNoSigPipe = 1;
  ::setsockopt(
    this->socketDescriptor,
    SOL_SOCKET,
    SO_NOSIGPIPE,
    &socketOptNoSigPipe,
    sizeof(socketOptNoSigPipe));
#endif // __APPLE__
  socketAddress.sin_port = htons(2000);
  if (::connect(this->socketDescriptor,
      reinterpret_cast<sockaddr *>(&socketAddress), sizeof(sockaddr_in))) {
    int savedErrorNumber = errno;
    close(this->socketDescriptor);
    this->socketDescriptor = -1;
    throw std::system_error(
      savedErrorNumber,
      std::generic_category(),
      "Could not connect UDP socket for communication with " + hostName);
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

MrfUdpIpClient::~MrfUdpIpClient() {
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

MrfUdpIpClient::Clock::time_point MrfUdpIpClient::checkSentRequests() {
  // This function must only be called while holding a lock on the mutex, so we
  // safely access all data structures but must not do anything that might
  // block in this function.
  auto now = Clock::now();
  Clock::time_point nextCheck;
  // We check whether the queue timeout for one of the queued requests has
  // expired. If this is the case, the request times out immediately without
  // ever being sent. This check is only made if the queue timeout is non-zero.
  // A timeout of zero means that requests shall never time out before being
  // sent for the first time.
  if (queueTimeout != Clock::duration::zero()) {
    while (!newRequests.empty()) {
      auto &request = newRequests.front();
      auto queueTimeoutTime = request->queueTime + queueTimeout;
      if (queueTimeoutTime <= now) {
        // If the request has timed out while waiting on the queue, we remove
        // it from the queue and notify the callback. The notification of the
        // callback has to happen after releasing the lock, so we delay that
        // action.
        timeoutCallbacks.emplace_back(std::move(request->callback));
        // Removing the request from the queue invalidates the reference, but
        // this is okay because we do not use it after this line.
        newRequests.pop_front();
      } else {
        // When we encounter the first request that has not timed out yet, we
        // are done, because all subsequent requests in the set must have a
        // queueTime greater than or equal to the one for this request.
        //
        // We set the time of the next check to the time when the next request
        // will timeout. nextCheck should not be set yet, but we rather keep
        // this slightly more complicated code in order to ensure that it will
        // still work correctly if we add any code that set nextCheck before
        // this code block.
        if (nextCheck.time_since_epoch() == Clock::duration::zero()) {
          nextCheck = queueTimeoutTime;
        } else {
          nextCheck = std::min(nextCheck, queueTimeoutTime);
        }
        break;
      }
    }
  }
  // If more than the user-specified time has passed since sending the request
  // for the first time, this is considered a timeout condition.
  // We remove the the request from the set of requests ordered by the time of
  // their first transmission and add it to the list of requests for which the
  // callback must be notified of the timeout. We cannot call the callback
  // here, because the callback must not be called while holding a lock on the
  // mutex.
  // We set the successOrTimeout flag on the request because it might still be
  // present in other lists (e.g. the list of requests which should be
  // retransmitted), so other sections of code that see this request should
  // know that it has timed out and should not be transmitted any longer.
  while (!requestsByFirstTransmission.empty()) {
    auto &request = *requestsByFirstTransmission.begin();
    auto requestTimeoutTime = request->firstSendTime + requestTimeout;
    if (requestTimeoutTime <= now) {
      // If the request has timed out, we do not remove it immediately.
      // Typically, there is still going to be a packet in flight, and if we
      // completely removed the request now, the number of packets in flight
      // would not match our expectations any longer, so we might send a packet
      // for another request too early.
      //
      // Instead, we notify the callback of the timeout, but keep the request
      // around until it would usually be retransmitted. At this point, we will
      // discard the request because we can assume that the packet associated
      // with it has been lost.
      //
      // We still remove the request from requestsByFirstTransmission, because
      // the timeout has been handled, and that is the only purpose of this
      // set.
      timeoutCallbacks.emplace_back(std::move(request->callback));
      request->callback = std::shared_ptr<RequestCallback>();
      request->successOrTimeout = true;
      // We have to increment the timeout counter, unless we have already
      // exceeded the limit. In the latter case, we do not increment it in
      // order to avoid an overflow situation when the peer is offline for a
      // very long time.
      if (consecutiveRequestTimeoutsCount <= maxRequestTimeoutsBeforeOffline) {
        consecutiveRequestTimeoutsCount += 1;
      } else {
        // When switching to offline mode, we reset a few variables, so that
        // we will use the settings that we would also use for an entirely new
        // connection when the peer comes back online.
        //
        // We do not reset the retransmissionTimeoutMultiplier. This multiplier
        // is reset when we receive a valid reply and thus leave offline mode.
        congestionWindow = initialCongestionWindow;
        congestionWindowIncrementCounter = 0;
        consecutiveLossesCount = 0;
        lastReceivedRoundTripTime = Clock::duration::zero();
        lastReceivedSendTime = Clock::time_point();
        reorderingWindowMultiplier = 0;
        reorderingWindowMultiplierResetCounter = 0;
        roundTripTimeVariation = Clock::duration::zero();
        slowStartThreshold = std::numeric_limits<
          decltype(slowStartThreshold)>::max();
        smoothedRoundTripTime = Clock::duration::zero();
      }
      // Removing the request from the set invalidates the reference, but this
      // is okay because we do not use it after this line.
      requestsByFirstTransmission.erase(requestsByFirstTransmission.begin());
    } else {
      // When we encounter the first request that has not timed out yet, we
      // are done, because all subsequent requests in the set must have a
      // firstSendTime greater than or equal to the one for this request.
      //
      // We set the time of the next check to the time when the next request
      // will timeout. nextCheck should not be set yet, but we rather keep this
      // slightly more complicated code in order to ensure that it will still
      // work correctly if we add any code that set nextCheck before this code
      // block.
      if (nextCheck.time_since_epoch() == Clock::duration::zero()) {
        nextCheck = requestTimeoutTime;
      } else {
        nextCheck = std::min(nextCheck, requestTimeoutTime);
      }
      break;
    }
  }
  // Next, we look for requests that need to be retransmitted because
  // the associated packet apparently has been lost. A packet is considered
  // lost in two cases:
  //
  // 1. The retransmission timer has expired.
  // 2. The response to a packet that has been transmitted later has been
  //    received and this cannot be explained with reordering according to the
  //    current reordering window.
  auto rto = retransmissionTimeout();
  bool rtoExpired = false;
  auto reorderingWindow =
    reorderingWindowMultiplier * smoothedRoundTripTime / 4;
  while (!requestsByLastTransmission.empty()) {
    // Check whether a request needs to be retransmitted according to either of
    // the two conditions.
    auto &request = *requestsByLastTransmission.begin();
    auto rtoExpires = request->lastSendTime + rto;
    auto earlyRetransmitExpires =
      request->lastSendTime + reorderingWindow + lastReceivedRoundTripTime;
    auto responseForLaterTransmissionReceived =
      lastReceivedSendTime.time_since_epoch() != Clock::duration::zero()
      && refGreaterThan(lastReceivedRef, request->lastRef);
    if (request->successOrTimeout) {
      // If the request has already succeeded or timed out, we only kept it in
      // case a late response arrives. As we now consider the last sent packet
      // lost, we can finally remove the request completely.
      //
      // Removing the request also invalidates the reference that we have, but
      // this is okay because it is not used after the next line of code.
      removeRequest(request);
      continue;
    }
    if (rtoExpires <= now) {
      // In order to ensure that the request is put into the correct position
      // in retransmitRequests, we have to set the retransmitTime. We give a
      // hint that the element should be inserted at the end of the set,
      // because typically there will be no requests that have a greater
      // retransmitTime. If there are, this simply has the effect that the
      // operation is no loger O(1) but O(log(n)).
      request->retransmitTime = rtoExpires;
      packetLossDetected(request);
      retransmitRequests.emplace_hint(
        retransmitRequests.end(), std::move(request));
      // Removing the request from the set invalidates the reference that we
      // have, but this is okay because it is not used after the next line of
      // code.
      requestsByLastTransmission.erase(requestsByLastTransmission.begin());
      // We record that the retransmission timeout expired, so that we can
      // update the multiplier after the loop. We do not update it inside the
      // loop because a bunch of requests expiring at the same time (due to
      // being sent at (almost) the same time) would quickly increase the
      // multiplier well beyond any reasonable value.
      rtoExpired = true;
    } else if (
        responseForLaterTransmissionReceived
        && (earlyRetransmitExpires <= now)) {
      // In order to ensure that the request is put into the correct position
      // in retransmitRequests, we have to set the retransmitTime. We give a
      // hint that the element should be inserted at the end of the set,
      // because typically there will be no requests that have a greater
      // retransmitTime. If there are, this simply has the effect that the
      // operation is no loger O(1) but O(log(n)).
      request->retransmitTime = earlyRetransmitExpires;
      packetLossDetected(request);
      retransmitRequests.emplace_hint(
        retransmitRequests.end(), std::move(request));
      // Removing the request from the set invalidates the reference that we
      // have, but this is okay because it is not used after the next line of
      // code.
      requestsByLastTransmission.erase(requestsByLastTransmission.begin());
    } else {
      // The requests in requests in requestsByLastTransmission are ordered, so
      // that all requests later in the queue must only be retransmitted after
      // requests earlier in the queue, so when we encounter the first request
      // that does not have to be retansmitted, we are done.
      //
      // We still have to determine when the request might become eligible for
      // retransmission, so that we can check again then.
      Clock::time_point nextRetransmissionCheck;
      if (
          responseForLaterTransmissionReceived
          && earlyRetransmitExpires < rtoExpires) {
        nextRetransmissionCheck = earlyRetransmitExpires;
      } else {
        nextRetransmissionCheck = rtoExpires;
      }
      if (nextCheck.time_since_epoch() == Clock::duration::zero()) {
        nextCheck = nextRetransmissionCheck;
      } else {
        nextCheck = std::min(nextCheck, nextRetransmissionCheck);
      }
      break;
    }
  }
  // If the retransmission timeout expired, we double it. It is reset to on
  // when a valid reply is received. This is in line with the approach
  // described in RFC 6298.
  //
  // At some point, the multiplier might overflow, so we only update it
  // if the new values is actually greater than the old one.
  if (rtoExpired) {
    retransmissionTimeoutMultiplier = std::max(
      retransmissionTimeoutMultiplier * 2, retransmissionTimeoutMultiplier);
  }
  return nextCheck;
}

bool MrfUdpIpClient::lastReceivedValid() const {
  // This function must only be called while holding a lock on the mutex, so we
  // safely access all data structures but must not do anything that might
  // block in this function.
  return lastReceivedSendTime.time_since_epoch() != Clock::duration::zero();
}

void MrfUdpIpClient::packetLossDetected(
    const std::shared_ptr<Request> &request) {
  // If we already reset the congestion window after sending the lost packet,
  // the loss of this packet should not be considered any longer. After all, we
  // already started from scratch, so this packet also being lost does not mean
  // that it makes sense to decrease the congestion window again.
  if (request->congestionWindowResetAfterSent) {
    return;
  }
  auto ref = request->packet.getRef();
  if (consecutiveLossesCount == 0) {
    // This is the first time a packet is lost, so we record its ref.
    consecutiveLossesFirstRef = ref;
    consecutiveLossesCount = 1;
  } else if (consecutiveLossesFirstRef + consecutiveLossesCount == ref) {
    // If the ref of the packet that was lost is one more than the ref of the
    // last packet that was lost, we increment the counter.
    //
    // We limit the counter the the consecutiveLossesLimit in order to avoid a
    // situation in which it might overflow.
    consecutiveLossesCount = std::min(
      consecutiveLossesCount + 1, consecutiveLossesLimit);
  } else {
    // If the ref of the packet that was lost is not adjacent to the ref of the
    // previous packet that was lost, the sequence of consecutive losses is
    // broken, so we start back at 1.
    //
    // We can use this very simple approach because loss of a packets with
    // adjacent refs can only be detected in order: The “early retransmission”
    // is only triggered when we already received a packet with a greater ref,
    // so that packet will definitely break the sequence (it cannot be detected
    // as lost, because we already received it). Packet that were sent after
    // the packet for which we received the response can only be considered
    // lost after all the preceding packets have already been received or
    // considered lost, so they cannot break the sequence either.
    //
    // For this reason, simply taking note of the first ref in the sequence and
    // the number of lost packets where the refs are consecutive is actually
    // sufficient.
    consecutiveLossesFirstRef = ref;
    consecutiveLossesCount = 1;
  }
  // Similar to the concept presented in RFC 5681, we reset the so-called
  // “slow-start threshold” based on the number of packets that were in flight
  // when the packet was sent (including the packet in question).
  //
  // RFC 5681 makes a distinction between whether the segment (in TCP, segments
  // are used for the logic instead of packets) has been retransmitted before
  // or not. There, this makes sense because there is a sequence of segments.
  // In our case however, there is no order, and thus it makes more sense to
  // treat a retransmitted request like a new request and always reset the
  // slow-start threshold.
  slowStartThreshold = std::max(
    request->packetsInFlightWhenSent / 2, static_cast<std::size_t>(2));
  // As long as less than consecutiveLossesLimit packets are lost, we simply
  // decrement the congestion window. When the limit is matched or exceeded, we
  // reset the congestion window to one.
  //
  // According to RFC 5681, implementations of TCP reset the congestion window
  // to one (segment), when the retransmission timeout expires. This does not
  // apply to fast restransmissions. Due to the different nature of TCP (it is
  // a stream protocol, where order matters), we use a different approach: We
  // reset the congestion window to one (packet) when we consecutively loose
  // three packets, regardless of whether these losses are detected due to a
  // timeout or due to the response to a packet that was sent later arriving.
  if (consecutiveLossesCount < consecutiveLossesLimit) {
    congestionWindow = std::max(
      congestionWindow - 1, static_cast<std::size_t>(1));
  } else {
    congestionWindow = 1;
    // When we reset the congestion window to one, other packets that have
    // already been sent and that might be lost should not cause the congestion
    // window to be updated. Threfore, we reset the consecutive losses count
    // and mark all requests that are still in flight.
    consecutiveLossesCount = 0;
    for (auto &requestInFlight : requestsByLastTransmission) {
      requestInFlight->congestionWindowResetAfterSent = true;
    }
  }
  // We reset the increment counter because succesful transmissions that
  // happened in the past should not count any longer after we have seen packet
  // loss.
  congestionWindowIncrementCounter = 0;
}

void MrfUdpIpClient::queueReadRequest(std::uint32_t address,
    const std::shared_ptr<RequestCallback> &callback) {
  // We have to hold a lock on the mutex while incrementing the counter and
  // modifying the request queue. We create the request object after acquiring
  // the lock because we have to ensure that the ordering of the queueTime
  // stored inside the request matches the order in which the requests are
  // stored in the list.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto request = std::make_shared<Request>(
      callback, 1, address, 0, true);
    newRequests.emplace_back(std::move(request));
    sendSelector.wakeUp();
  }
}

void MrfUdpIpClient::queueWriteRequest(std::uint32_t address,
    std::uint16_t data, const std::shared_ptr<RequestCallback> &callback) {
  // We have to hold a lock on the mutex while incrementing the counter and
  // modifying the request queue. We create the request object after acquiring
  // the lock because we have to ensure that the ordering of the queueTime
  // stored inside the request matches the order in which the requests are
  // stored in the list.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    auto request = std::make_shared<Request>(
      callback, 2, address, data, false);
    newRequests.emplace_back(std::move(request));
    sendSelector.wakeUp();
  }
}

void MrfUdpIpClient::removeRequest(
    const std::shared_ptr<Request> &request) {
  // This function must only be called while holding a lock on the mutex, so we
  // safely access all data structures but must not do anything that might
  // block in this function.
  //
  // For each of the refs that was used for the request, we have to remove it
  // from refToRequest.
  for (auto const &refAndSendTime : request->sendTime) {
    refToRequest.erase(refAndSendTime.first);
  }
  // The find method of std::multiset finds all requests that have the same
  // firstSendTime as the specified request, so we must still check whether
  // each of the found requests actually is the request that we want to remove.
  for (
      auto requestIterator = requestsByFirstTransmission.find(request);
      requestIterator != requestsByFirstTransmission.end();) {
    if (*requestIterator == request) {
      requestIterator = requestsByFirstTransmission.erase(requestIterator);
    } else {
      ++requestIterator;
    }
  }
  // For requestsByLastTransmission, the logic is the same as for
  // requestsByFirstTransmission, the onl difference being that the search
  // criterion is lastSendTime instead of firstSendTime.
  for (
      auto requestIterator = requestsByLastTransmission.find(request);
      requestIterator != requestsByLastTransmission.end();) {
    if (*requestIterator == request) {
      requestIterator = requestsByLastTransmission.erase(requestIterator);
    } else {
      ++requestIterator;
    }
  }
  // Finally, we do the same for retransmitRequests.
  for (
      auto requestIterator = retransmitRequests.find(request);
      requestIterator != retransmitRequests.end();) {
    if (*requestIterator == request) {
      requestIterator = retransmitRequests.erase(requestIterator);
    } else {
      ++requestIterator;
    }
  }
}

MrfUdpIpClient::Clock::duration MrfUdpIpClient::retransmissionTimeout() const {
  // This function must only be called while holding a lock on the mutex, so we
  // safely access all data structures but must not do anything that might
  // block in this function.
  //
  // We calculate the retransmission timeout in a similar way as described for
  // TCP in RFC 6298. In contrast to RFC 6298, we do not use a lower bound of
  // one second for the retransmission timeout. Instead, we use a tenth of the
  // user specified request timeout or 100 ms, whichever is greater.
  //
  // Like suggested by RFC 6298, we use an upper bound of 60 seconds.
  auto rtoLowerLimit = std::max(
    requestTimeout / 10, Clock::duration(std::chrono::milliseconds(100)));
  constexpr auto rtoUpperLimit = Clock::duration(std::chrono::seconds(60));
  Clock::duration rtoBase;
  if (smoothedRoundTripTime != Clock::duration::zero()) {
    // SRTT and RTTVAR are valid, so we can use it to calculate the RTO.
    rtoBase =
      smoothedRoundTripTime
      + std::max(Clock::duration(1), 4 * roundTripTimeVariation);
  } else {
    // We do not have a valid SRTT and RTTVAR yet, so we use the lower limit
    // for the RTO.
    rtoBase = rtoLowerLimit;
  }
  // As retransmissionTimeoutMultipler might grow very large, we have to check
  // for an overflow when multiplying rtoBase with it. As we know that neither
  // of the factors is negative, we do not have to worry about an underflow.
  if (
      rtoBase.count()
      > std::numeric_limits<Clock::duration::rep>::max()
      / retransmissionTimeoutMultiplier) {
    return rtoUpperLimit;
  }
  // The retransmission timeout is the base value multiplied by the multiplier
  // but contraint to be between the lower and upper limit.
  return std::max(
    std::min(rtoBase * retransmissionTimeoutMultiplier, rtoUpperLimit),
    rtoLowerLimit);
}

bool MrfUdpIpClient::trySendRequest(
    const std::shared_ptr<Request> &request) {
  request->lastRef = nextRef;
  request->packet.setRef(nextRef);
  try {
    request->packet.send(socketDescriptor, 0);
  } catch (std::system_error &e) {
    // An error code of EAGAIN is not considered an error. It just means
    // that the packet could not be sent immediately. Other errors are
    // considered permanent, so we let the exception bubble up.
    if (e.code() == std::error_code(EAGAIN, std::generic_category())) {
      return false;
    }
    throw;
  }
  // We need the current time in a few places, so we get it now.
  auto now = Clock::now();
  // We successfully send the request, so nextRef needs to be incremented. We
  // set request->lastRef before doing so, so that we still have the ref that
  // we just used.
  request->lastRef = nextRef;
  nextRef += 1;
  // If this was the first time the request was sent, we have to set its
  // firstSendTime and add it to requestsByFirstSendTime (in this order,
  // otherwise it won’t be added in the right position).
  if (request->sendTime.empty()) {
    request->firstSendTime = now;
    // We give a hint that the element should be inserted at the end of the
    // multiset. Usually, this hint is going to be correct and then the insert
    // operation will be O(1) instead of O(log(n)).
    requestsByFirstTransmission.insert(
      requestsByFirstTransmission.end(), request);
  }
  // We also have to update a few more fields in the request.
  request->congestionWindowResetAfterSent = false;
  request->lastSendTime = now;
  // The packet that we just sent counts towards the number of packets in
  // flight, but we have not aded the request to the map yet, so we have to use
  // the map size plus one.
  request->packetsInFlightWhenSent = requestsByLastTransmission.size() + 1;
  request->sendTime.emplace(std::make_pair(request->lastRef, now));
  // We also have to add the request and its ref to refToRequest.
  refToRequest.emplace(std::make_pair(request->lastRef, request));
  // Finally, we have to add the request to requestsByLastTransmission. This
  // has to be done after setting the lastSendTime field, so we do it last.
  requestsByLastTransmission.insert(requestsByLastTransmission.end(), request);
  // The packet for the request was successfully sent, so we have to indicate
  // this to the calling code.
  return true;
}

void MrfUdpIpClient::runReceiveThread() {
  int numberOfConsecutiveReadFailures = 0;
  while (!shutdown.load(std::memory_order_acquire)) {
    ::fd_set readFds;
    FD_ZERO(&readFds);
    FD_SET(socketDescriptor, &readFds);
    receiveSelector.select(&readFds, nullptr, nullptr, socketDescriptor,
        nullptr);
    MrfUdpPacket packet;
    try {
      packet.receive(socketDescriptor);
    } catch (std::range_error &e) {
      // Ignore packets with an odd size.
      continue;
    } catch (std::system_error &e) {
      // A EAGAIN error is not considered an error. The next select operation
      // should block until reading is possible again. We also ignore a
      // connection refused error because this simply means that the peer is
      // temporarily unavailable.
      if (e.code().value() != EAGAIN && e.code().value() != ECONNREFUSED) {
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
    // Remember the time at which we received the packet. We need this to
    // determine the round-trip time (RTT).
    auto receiveTime = std::chrono::steady_clock::now();
    // Reset the error counter.
    numberOfConsecutiveReadFailures = 0;
    // Find the request associated with the received packet, so that we can
    // call the callback.
    std::shared_ptr<RequestCallback> callback;
    {
      // We have to hold the mutex while accessing the refToRequest map,
      // updating the RTT estimate, and making other changes to shared
      // variables.
      std::lock_guard<std::recursive_mutex> lock(mutex);
      // We check whether the ref of the packet matches any of the refs for
      // which we expect a packet. If so, we process the contents of the
      // packet.
      auto ref = packet.getRef();
      auto refToRequestIterator = refToRequest.find(ref);
      if (refToRequestIterator != refToRequest.end()) {
        auto request = refToRequestIterator->second;
        // We received a valid reply for a pending request, so we have to reset
        // the counter which is counting consecutive timeouts.
        consecutiveRequestTimeoutsCount = 0;
        // When we receive a valid packet, the retransmission timeout should be
        // reset to the base value. This is in line with the approach described
        // in RFC 6298.
        retransmissionTimeoutMultiplier = 1;
        // Receiving a valid reply also means that we want to update the
        // congestion window, unless the congestion window was reset after
        // sending the request that caused this reply.
        if (!request->congestionWindowResetAfterSent) {
          // If we are in “slow-start” mode, we increment the congestion window
          // by one for each reply that we receive. This basically is the
          // approach suggested by RFC 5681.
          //
          // In “congestion avoidance” mode (when the congestion window is
          // greater than the slow-start threshold), we only increment the
          // congestion window after seeing multiple replies for requests that
          // used the existing congestion window when sending the respective
          // requests. Replies that we receive for requests that were sent when
          // fewer packets were in flight, cannot really serve as an indication
          // that we can increase the congestion window, because the fact that
          // they were successful could simply be due to the fact that the
          // request rate was low at the time when they were sent.
          //
          // RFC 5681 takes a different approach, that increments the
          // congestion window in congestion-avoidance mode about once per each
          // round-trip time, but this approach does not really fit in or case.
          // TCP is very different from our protocol because it is
          // stream-based and there is a receive window in addition to the
          // congestion window, so we cannot simply copy the approach used for
          // TCP.
          if (congestionWindow <= slowStartThreshold) {
            congestionWindow += 1;
          } else if (congestionWindow == request->packetsInFlightWhenSent) {
            // We increment the counter so that we know that we have seen a
            // packet that successfully used the full congestion window.
            congestionWindowIncrementCounter += 1;
            // After seeing two such packets, we increment the congestion
            // window by one. There is nothing special about the number two. We
            // simply found experimentally, that when immediately incrementing
            // the congestion window after seeing a single packet, it happens
            // rather frequently that subsequently three packets are lost in a
            // row, thus resulting in a hard reset of the congestion window.
            // And waiting for three packets instead of two does not seem to
            // significantly reduce the frequency of such events any longer, so
            // waiting for two packets seems to be the sweet spot.
            if (congestionWindowIncrementCounter == 2) {
              congestionWindow += 1;
              congestionWindowIncrementCounter = 0;
            }
          }
        }
        // We always want to remove the mapping for the current ref. This way,
        // if we receive a duplicate packet, we will simply ignore the second
        // packet.
        refToRequest.erase(refToRequestIterator);
        // Whenever we receive a packet that is expected, we have to wake up
        // the send thread because due to updating data structures here, the
        // send thread might have to do work. The send thread can only actually
        // do work when we release the lock, but we have to wake it up while
        // holding the lock, so we can do this right now, before we actually
        // update the data structures. It will still have the same effect as if
        // we did it later, just before releasing the lock.
        sendSelector.wakeUp();
        // We need the time when the packet was sent, so that we can calculate
        // the round-trip time.
        auto sendTimeIterator = request->sendTime.find(ref);
        if (sendTimeIterator != request->sendTime.end()) {
          auto sendTime = sendTimeIterator->second;
          auto roundTripTime = receiveTime - sendTime;
          this->updateRoundTripTime(roundTripTime);
          // If this is not the first packet that we receive, we want to run
          // some checks in order to detect packets that are received out of
          // order.
          if (lastReceivedValid()) {
            // If we see a packet that is sent in response to a request which
            // was sent before a request for which we already received the
            // response, reordering happened somewhere along the network path.
            if (refGreaterThan(lastReceivedRef, ref)) {
              // If the reordering window multiplier is zero, we set it to 1.
              // Otherwise, we want to keep the current value.
              reorderingWindowMultiplier = std::max(reorderingWindowMultiplier, 1);
            } else {
              // The request for the received packet was sent after the request
              // referenced by lastReceivedSendTime and lastReceivedRef, so we
              // update the associated variables.
              lastReceivedRef = ref;
              lastReceivedRoundTripTime = roundTripTime;
              lastReceivedSendTime = sendTime;
            }
          } else {
            // This is the first packet that we receive, so we set
            // lastReceivedRef, lastReceivedRoundTripTime, and
            // lastReceivedSendTime.
            lastReceivedRef = ref;
            lastReceivedRoundTripTime = roundTripTime;
            lastReceivedSendTime = sendTime;
          }
          // If the packet that we receive is not in reply to the last packet
          // that we sent for this request, we had a “spurious retransmission”,
          // meaning that we retransmitted a request even though the packet
          // that was sent earlier wasn’t lost but the response simply arrived
          // later than expected. If we have detected that reordering happens
          // for the connection, we increment the multiplier for the reordering
          // window. However, as described in RFC 8985, we never let the
          // reordering window grow greater than the smoothed round-trip time.
          // As unlike RFC 8985 we use the smoothed round-trip time as the
          // minimum RTT, we can simply achieve this by limiting the multiplier
          // to a maximum value of four.
          if (ref != request->lastRef&& reorderingWindowMultiplier) {
            reorderingWindowMultiplier = std::max(
              reorderingWindowMultiplier + 1, 4);
            reorderingWindowMultiplierResetCounter = 16;
          }
          // If we only incremented the multiplier, we could use a reordering
          // window that is larger than needed, unnecessarily delaying
          // retransmissions. Therefore, we reset the multiplier to one when we
          // consecutively had 16 successful retransmissions without having a
          // spurious retransmission in between. The new multiplier might be
          // too small and we might again get spurious retransmissions, but
          // this will only happen a few times before the multiplier is at its
          // larger value again. RFC 8985 takes a similar approach for TCP.
          if (
              ref == request->lastRef
              && request->firstSendTime != request->lastSendTime
              && reorderingWindowMultiplier > 1) {
            reorderingWindowMultiplierResetCounter -= 1;
            if (reorderingWindowMultiplierResetCounter == 0) {
              reorderingWindowMultiplier = 1;
            }
          }
          // If we receive another packet with the same ref, it must be a
          // duplicate and we want to ignore it. Therefore, we can remove the
          // send time for this ref from the request. The entry for this ref
          // has already been removed from refToRequest earlier in the code, so
          // we do not have to do this here.
          request->sendTime.erase(sendTimeIterator);
        }
        // If the request is idempotent or we received the response for the
        // packet that was sent last, the request was successful and we can
        // call the callback. If we received the response for the packet that
        // was sent last, we can also remove the request. Otherwise, we have to
        // keep it around because the more recently sent packet might still be
        // in flight.
        if (request->idempotent || ref == request->lastRef) {
          // We are done with the request, so we have to call the callback.
          callback.swap(request->callback);
          request->successOrTimeout = true;
          // We only remove the request if we do not expect any further packets
          // for it. Otherwise, we keep it, so that we can correctly keep track
          // of the number of packets “in flight”.
          if (ref == request->lastRef) {
            // Calling removeRequest invalidates the refToRequestIterator, but
            // this is okay because we do not use this iterator later in the
            // code.
            removeRequest(request);
          }
        }
      } else {
        // If we cannot find the request it probably timed out, so we simply
        // ignore the packet that we just received.
        continue;
      }
    }
    // We call the callback without holding the mutex in order to avoid a dead
    // lock.
    if (callback) {
      try {
        (*callback)(packet.getData(), packet.getStatus(), std::exception_ptr());
      } catch (...) {
        // We catch all errors so that an exception that is thrown by a callback
        // does not stop the receive thread.
      }
    }
  }
}

void MrfUdpIpClient::runSendThread() {
  while (!shutdown.load(std::memory_order_acquire)) {
    // There is some information that we have to carry from the block that
    // holds a lock on the mutex to the code that runs after releasing the
    // lock.
    //
    // Under some conditions (if a packet has successfully been sent or sending
    // a packet has failed for a different condition then the socket not being
    // ready), we want to run the next iteration of the loop without waiting.
    // This is indicated by setting this flag.
    bool continueImmediately = false;
    // In theory, accessing timeoutCallbacks without holding a lock on the
    // mutex should be safe, because we only access it from this thread.
    // However, if the code is changed for some reason in the future, it might
    // not be safe any longer. Therefore, we move the contents of this list
    // into an instance that is local to this function while holding the lock.
    // This way, the code is future proof and the cost for doing this is
    // minimal because we can use a swap operation.
    std::list<std::shared_ptr<RequestCallback>> localTimeoutCallbacks;
    // When making the select call, we might want to limit how long this call
    // blocks. If the following variable is non-zero, it defines the point in
    // time at which the select call should wake up. If it is zero, this
    // indicates that the select call may block indefinitely.
    Clock::time_point nextCheckTime;
    // If there is an error while trying to send the packet for a request, we
    // want to notify the callback, but we have to do this after releasing the
    // lock.
    std::exception_ptr sendException;
    std::shared_ptr<RequestCallback> sendExceptionCallback;
    // This flag indicates whether we would write to the socket if it was
    // ready. This information is used to decide whether we should wait for the
    // socket to become writable.
    bool waitForSocket = false;
    {
      // We have to hold a lock on the mutex while accessing the shared data
      // structures. We have to release the lock before we do anything that
      // might block (like waiting on the selector).
      std::lock_guard<std::recursive_mutex> lock(mutex);
      // First, we check requests that have already been sent for timeouts,
      // moving them to the appropriate queues if necessary.
      nextCheckTime = checkSentRequests();
      // If more than a certain number of requests consecutively experienced a
      // timeout, we consider the peer to be offline. That has the effect that
      // we fail new requests immediately, without even waiting for the
      // timeout to pass.
      bool offline =
        consecutiveRequestTimeoutsCount > maxRequestTimeoutsBeforeOffline;
      // Transfer the callbacks from timeoutCallbacks to the local instance.
      localTimeoutCallbacks.swap(timeoutCallbacks);
      // Now, we can send the packet for a request if the congestion window
      // allows for it. The congestion window specifies how many packets may
      // concurrently be “in flight”. A packet that is considered lost by our
      // algorithm is not considered in flight any longer. Therefore, we can
      // easily get the number of packets in flight by counting the number of
      // entries in requestsByLastTransmission.
      //
      // If we want to send a request, we do so regardless of whether the
      // socket is ready or not. If it is not ready, the operation will fail
      // with EAGAIN, and we know that we have to wait for the socket to become
      // writable again.
      //
      if (congestionWindow > requestsByLastTransmission.size()) {
        // We prioritize retransmitting earlier requests over sending entirely
        // new requests.
        if (!retransmitRequests.empty()) {
          auto nextRequestIterator = retransmitRequests.begin();
          auto nextRequest = *nextRequestIterator;
          // If the request succeeded or timed out while waiting for
          // retransmission, we do not retransmit it now. Instead, we remove it
          // (the callback has already been notified if successOrTimeout is
          // set).
          if (nextRequest->successOrTimeout) {
            removeRequest(nextRequest);
            // We did not try to send anything, so we need another iteration in
            // order to check whether another request might be eligible for
            // transmission.
            continueImmediately = true;
          } else {
            bool sent = false;
            try {
              sent = trySendRequest(nextRequest);
            } catch (...) {
              sendException = std::current_exception();
              sendExceptionCallback = nextRequest->callback;
            }
            // We remove the request if it was sent successfully or if sending
            // failed due to an error. We do not remove it if it has to be
            // delayed because the socket is not ready yet.
            if (sent || sendException) {
              retransmitRequests.erase(nextRequestIterator);
              // Another packet might be waiting for transmission, so we do not
              // want the thread to sleep before continuing with the next
              // iteration of the loop.
              continueImmediately = true;
            } else {
              // We couldn’t send the packet because the socket was not ready,
              // so we want to wait for it to become available.
              waitForSocket = true;
            }
          }
        } else if (!newRequests.empty()) {
          auto nextRequest = newRequests.front();
          bool sent = false;
          try {
            // We try to send a request, even if we are in offline mode. If we
            // (unexpectedly) get a reply, we know that the peer is no longer
            // offline. For write requests, there is a downside to this
            // approach: We communicate to the calling code that the request
            // has failed, but if the device comes online just as we send the
            // request and the calling code makes another write request, the
            // packet for the later request could be reordered before the
            // packet for the earlier request, thus indicating the wrong result
            // to the calling code. However, the chance of this happening is so
            // low that we accept it.
            //
            // This behavior is not much different from how we handle the
            // request timeout: After reporting such a timeout, the request
            // might still succeed and be reordered after a later request, but
            // this is a problem that we cannot avoid either, because the
            // protocol is designed in a way that there is no sequencing
            // between different requests.
            sent = trySendRequest(nextRequest);
          } catch (...) {
            sendException = std::current_exception();
            sendExceptionCallback = nextRequest->callback;
          }
          // We remove the request if the peer is offline (in this case, we do
          // not care whether it was sent), if it was sent successfully, or if
          // sending failed due to an error. We do not remove it if it has to
          // be delayed because the socket is not ready yet.
          if (offline | sent || sendException) {
            newRequests.pop_front();
            // Another packet might be waiting for transmission, so we do not
            // want the thread to sleep before continuing with the next
            // iteration of the loop.
            continueImmediately = true;
          } else {
            // We couldn’t send the packet because the socket was not ready, so
            // we want to wait for it to become available.
            waitForSocket = true;
          }
          // If the peer is offline, the request is very likely to result in a
          // timeout. Having each individual request time out can take very
          // long when there are many requests that are sent one after another.
          // In order to avoid a situation, where the queue of new requests
          // grows very big when the peer is offline, we immediately fail a
          // request when we have detected that we are in an offline situation.
          // We still want to occassionally sent requests (when we can do so
          // without waiting for the socket), so that we can detect when the
          // peer comes online again. This means that when the peer finally
          // comes online again, at least one additional request will be
          // failed, even though it will actually succeed, but this is an
          // acceptable trade-off.
          //
          // If there was an exception, we do not need any special handling
          // because the request fails immediately anyway.
          if (offline && !sendException) {
            sendException = std::make_exception_ptr(PeerOfflineException());
            // If the requst was sent successfully, we do not want the callback
            // to be called again later, when the request times out or actually
            // is successful, so we have to remove the callback. We achieve
            // this by swapping with the (empty) sendExceptionCallback instead
            // of assigning to it.
            sendExceptionCallback.swap(nextRequest->callback);
          }
        }
      } else if (offline && !newRequests.empty()) {
        // If we cannot send any more packets because of the congestion window,
        // we still want to fail requests if the peer is offline.
        auto nextRequest = newRequests.front();
        sendException = std::make_exception_ptr(PeerOfflineException());
        sendExceptionCallback = nextRequest->callback;
        newRequests.pop_front();
        // Another request might be waiting for processing, so we do not want
        // the thread to sleep before continuing with the next iteration of the
        // loop.
        continueImmediately = true;
      }
    }
    // Now we have left the block that acquired the mutex. This means that we
    // can perform blocking actions now, but it also means that we must not
    // access any shared data structures.
    //
    // If there was an error sending a packet, we call the associated callback
    // now.
    if (sendExceptionCallback) {
      try {
        (*sendExceptionCallback)(0, 0, sendException);
      } catch (...) {
        // We do not want an exception in the callback to stop the send thread,
        // so we ignore it.
      }
    }
    // We have to notify the callbacks of requests that timed out.
    for (const auto &callback : localTimeoutCallbacks) {
      if (callback) {
        try {
          (*callback)(0, 0, std::make_exception_ptr(TimeoutException()));
        } catch (...) {
          // We do not want an exception in the callback to stop the send
          // thread, so we ignore it.
        }
      }
    }
    // We clear the list, so that it is empty the next time it is swapped with
    // the other instance.
    localTimeoutCallbacks.clear();
    // If the continueImmediately flag is set, we continue with the next
    // iteration of the loop instead of waiting for an event or timeout.
    if (continueImmediately) {
      continue;
    }
    // Finally, we want the thread to sleep until there is more work to be
    // done.
    ::fd_set writeFds;
    ::fd_set *writeFdsPtr = nullptr;
    // If we want to wait for the socket to become writable, we have to add the
    // respective file descriptor to the set of write FDs.
    if (waitForSocket) {
      FD_ZERO(&writeFds);
      FD_SET(socketDescriptor, &writeFds);
      writeFdsPtr = &writeFds;
    }
    // When select is called without specifying a timeout, it may block
    // indefinitely. We use a time point of zero to indicate that this actually
    // is what we want. If it is non-zero, we want to use a timeout, so that
    // select returns at or after the specified point in time.
    ::timeval selectTimeout;
    ::timeval *selectTimeoutPtr = nullptr;
    if (nextCheckTime.time_since_epoch() != Clock::duration::zero()) {
      auto now = Clock::now();
      if (now >= nextCheckTime) {
        // If the desired point in time has already been reached, we do not
        // have to call select and can simply continue with the next iteration
        // of the loop.
        continue;
      }
      selectTimeout = toTimeval(nextCheckTime - now);
      selectTimeoutPtr = &selectTimeout;
    }
    // Call select so that this thread sleeps until the socket becomes writable
    // (if waitForSocket is set), the specified time has passed (if
    // nextCheckTime is non-zero), or sendSelector.wakeUp() is called.
    sendSelector.select(
      nullptr, writeFdsPtr, nullptr, socketDescriptor, selectTimeoutPtr);
  }
}

void MrfUdpIpClient::updateRoundTripTime(Clock::duration measurement) {
  // This function must only be called while holding a lock on the mutex, so we
  // safely access all data structures but must not do anything that might
  // block in this function.
  //
  // The calculation of the round-trip time estimate in this function follows
  // the method described in RFC 6298, using the additional corrections that
  // are recommended when updating the RTT estimate more than once per RTT
  // which are described in appendix G of RFC 7323.
  if (
      smoothedRoundTripTime == Clock::duration::zero()
      && roundTripTimeVariation == Clock::duration::zero()) {
    smoothedRoundTripTime = measurement;
    roundTripTimeVariation = measurement / 2;
  } else {
    double alpha =
      1.0 / (8.0 * std::max(
        (requestsByFirstTransmission.size()) / 2,
        static_cast<std::size_t>(1)));
    double beta = alpha * 2.0;
    Clock::duration diff;
    if (smoothedRoundTripTime > measurement) {
      diff = smoothedRoundTripTime - measurement;
    } else {
      diff = measurement - smoothedRoundTripTime;
    }
    roundTripTimeVariation = std::chrono::duration_cast<Clock::duration>(
      (1.0 - beta) * roundTripTimeVariation + beta * diff);
    smoothedRoundTripTime = std::chrono::duration_cast<Clock::duration>(
      (1.0 - alpha) * smoothedRoundTripTime + alpha * measurement);
  }
}

} // namespace mrf
} // namespace anka
