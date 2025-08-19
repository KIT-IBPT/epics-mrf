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

#ifndef ANKA_MRF_UDP_IP_CLIENT_H
#define ANKA_MRF_UDP_IP_CLIENT_H

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>

#include <MrfFdSelector.h>
#include "MrfUdpPacket.h"

namespace anka {
namespace mrf {

/**
 * Client for the MRF UDP/IP protocol.
 *
 * This implements the network communication for the MrfUdpIpMemoryAccess.
 */
class MrfUdpIpClient {

public:

  /**
   * Exception that is passed to the RequestCallback when a request fails
   * because the peer is considered offline.
   */
  struct PeerOfflineException : public std::runtime_error {
    PeerOfflineException() : std::runtime_error("Peer is offline") {
    };
  };

  /**
   * Data structure that is used for the request callbacks.
   */
  struct RequestCallback {
    virtual ~RequestCallback() {}

    virtual void operator()(
        std::uint16_t receivedData,
        std::int8_t receivedStatus,
        std::exception_ptr exception) = 0;
  };

  /**
   * Exception that is passed to the RequestCallback when a request fails
   * because of a timeout.
   */
  struct TimeoutException : public std::runtime_error {
    TimeoutException() : std::runtime_error("Request timeout") {
    };
  };

  /**
   * Creates a UDP/IP client for an MRF device.
   *
   * The specified host name can either be a DNS name or an IP address.
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
   * The constructor creates two background threads that take care of
   * communicating with the device.
   *
   * Throws an exception if the socket cannot be initialized, the background
   * threads cannot be created, or if one of the parameters is invalid.
   */
  template<typename Rep1, typename Period1, typename Rep2, typename Period2>
  MrfUdpIpClient(
      const std::string &hostName,
      const std::chrono::duration<Rep1, Period1> &queueTimeout,
      const std::chrono::duration<Rep2, Period2> &requestTimeout) :
      MrfUdpIpClient(
        hostName,
        std::chrono::duration_cast<Clock::duration>(queueTimeout),
        std::chrono::duration_cast<Clock::duration>(requestTimeout)) {
  }

  /**
   * Destructor. Closes the connection to the device and terminates the
   * background threads.
   */
  virtual ~MrfUdpIpClient();

  /**
   * Queues a request for reading a word from a memory address.
   */
  void queueReadRequest(
      std::uint32_t address,
      const std::shared_ptr<RequestCallback> &callback);

  /**
   * Queues a request for writing a word to a memory address.
   */
  void queueWriteRequest(
      std::uint32_t address,
      std::uint16_t data,
      const std::shared_ptr<RequestCallback> &callback);

private:

  /**
   * Clock that is internally used for time keeping.
   *
   * We use std::chrono::steady_clock because it is important that the clock is
   * monotonic, while we do not care about wall-clock time.
   */
  using Clock = std::chrono::steady_clock;

  /**
   * Data structure storing all data associated with the request for sending a
   * UDP packet.
   */
  struct Request {
    Request(
        const std::shared_ptr<RequestCallback> &callback,
        std::uint8_t accessType,
        std::uint32_t address,
        std::uint16_t data,
        bool idempotent) :
        callback(callback),
        idempotent(idempotent),
        packet(accessType, address, data, 0, 0),
        queueTime(Clock::now()),
        successOrTimeout(false) {
    }

    std::shared_ptr<RequestCallback> callback;
    bool congestionWindowResetAfterSent;
    Clock::time_point firstSendTime;
    bool idempotent;
    std::uint32_t lastRef;
    Clock::time_point lastSendTime;
    MrfUdpPacket packet;
    std::size_t packetsInFlightWhenSent;
    Clock::time_point queueTime;
    Clock::time_point retransmitTime;
    std::unordered_map<std::uint32_t, Clock::time_point> sendTime;
    bool successOrTimeout;
  };

  /**
   * Comparator used for the requestsByFirstTransmission set.
   */
  struct RequestFirstSendTimeComparator {
    bool operator()(
        const std::shared_ptr<Request> &request1,
        const std::shared_ptr<Request> &request2) const {
      return
        (request1 && request2)
        ? (request1->firstSendTime < request2->firstSendTime)
        : !request1;
    }
  };

  /**
   * Comparator used for the requestsByLastTransmission set.
   */
  struct RequestLastSendTimeComparator {
    bool operator()(
        const std::shared_ptr<Request> &request1,
        const std::shared_ptr<Request> &request2) const {
      return
        (request1 && request2)
        ? (request1->lastSendTime < request2->lastSendTime)
        : !request1;
    }
  };

  /**
   * Comparator used for the retransmitRequests set.
   */
  struct RequestRetransmitTimeComparator {
    bool operator()(
        const std::shared_ptr<Request> &request1,
        const std::shared_ptr<Request> &request2) const {
      return
        (request1 && request2)
        ? (request1->retransmitTime < request2->retransmitTime)
        : !request1;
    }
  };

  /**
   * Private constructor that is called after converting the durations to a
   * type that is compatible with the system clock.
   */
  MrfUdpIpClient(
      const std::string &hostName,
      const Clock::duration &queueTimeout,
      const Clock::duration &requestTimeout);

  // We do not want to allow copy or move construction or assignment.
  MrfUdpIpClient(const MrfUdpIpClient &) = delete;
  MrfUdpIpClient(MrfUdpIpClient &&) = delete;
  MrfUdpIpClient &operator=(const MrfUdpIpClient &) = delete;
  MrfUdpIpClient &operator=(MrfUdpIpClient &&) = delete;

  /**
   * Checks whether any sent requests should be retransmitted or have timed out
   * completely and moves them to the appropriate queue.
   *
   * Returns the time when (according to current conditions) the next check
   * should be performed (zero if according to current conditions, no further
   * check is needed).
   *
   * This function must only be called while holding a lock on the mutex.
   */
  Clock::time_point checkSentRequests();

  /**
   * Tells whether the information about the last received packet is valid.
   *
   * This function must only be called while holding a lock on the mutex.
   */
  bool lastReceivedValid() const;

  /**
   * Takes the actions that are necessary when packet loss is detected.
   *
   * This function must only be called while holding a lock on the mutex.
   */
  void packetLossDetected(const std::shared_ptr<Request> &request);

  /**
   * Removes a request from most of the internal maps and lists.
   *
   * The request is removed from refToRequest, requestsByFirstTransmission,
   * requestsByLastTransmission, and retransmitRequests. It is not removed from
   * newRequests.
   *
   * Please note that using this function potentially invalidates iterators for
   * the data structures mentioned in the preceding paragraph.
   *
   * This function must only be called while holding a lock on the mutex.
   */
  void removeRequest(const std::shared_ptr<Request> &request);

  /**
   * Calculates and returns the current retransmission timeout.
   *
   * This is not the timeout that is used to decide that a request has failed
   * (that timeout is user defined and typically much longer) and it also isn’t
   * the timeout that decides whether a packet is retransmitted early. Rather,
   * it is the timeout that decides when a packet is retransmitted if it is not
   * eligible for early retransmissions (yet).
   *
   * This function must only be called while holding a lock on the mutex.
   */
  Clock::duration retransmissionTimeout() const;

  /**
   * Main function of the receive thread.
   */
  void runReceiveThread();

  /**
   * Main function of the send thread.
   */
  void runSendThread();

  /**
   * Try to send a request (in a non-blocking way).
   *
   * Returns true if the request was sent, false if it wasn’t send because the
   * operation failed with EAGAIN. If it failed with another error code, a
   * std::system_error is thrown.
   *
   * This function must only be called while holding a lock on the mutex.
   */
  bool trySendRequest(const std::shared_ptr<Request> &request);

  /**
   * Update the current estimate of the round-trip time with a measurement.
   *
   * This function must only be called while holding a lock on the mutex.
   */
  void updateRoundTripTime(Clock::duration measurement);

  /**
   * The number of consecutive losses at which the congestion window is not
   * only decremented but reset to one.
   */
  const static std::uint32_t consecutiveLossesLimit = 3;

  /**
   * Initial window for the congestion window. This value is used when a new
   * connction is established or when the peer has been offline for a while.
   */
  const static std::uint32_t initialCongestionWindow = 4;

  /**
   * The limit of how many requests may consecutively timeout, before the peer
   * is considered offline.
   *
   * If this number of timeouts is exceeded, further requests are failed
   * immediately, without even waiting for a reply from the peer. This way, we
   * can avoid a long backlog of requests, which are most likely notgoing to
   * succeed anyway, and can drastically reduce the time needed for
   * initialization during startup when one of the devices is offline.
   */
  const static int maxRequestTimeoutsBeforeOffline = 2;

  /**
   * Number of packets that may concurrently be “in flight”. This is
   * dynamically decreased when congestion is detected and increases if
   * everything seems to be working fine.
   */
  std::size_t congestionWindow = initialCongestionWindow;

  /**
   * Counter for the number of reply packets that are received for requests
   * that were made when the congestion window was fully used.
   *
   * This is used when operating in “congestion avoidance” mode to decide when
   * the congestion window should be incremented.
   */
  int congestionWindowIncrementCounter;

  /**
   * Number of packets that got lost consecutively
   *
   * A loss is considered consecutive if all the lost packets where sent
   * adjacent to each other, without any packet that was sent in between having
   * resulted in a response that was actually received.
   */
  std::uint32_t consecutiveLossesCount = 0;

  /**
   * Ref of the packet that caused consecutiveLossesCount to be incremented
   * from zero to one. This is used to determine whether a loss is considered
   * consecutive.
   */
  std::uint32_t consecutiveLossesFirstRef;

  /**
   * Number of consecutive request timeouts. Timeouts are considered
   * consecutive when no request at all succeeds between the timeouts.
   */
  int consecutiveRequestTimeoutsCount = 0;

  /**
   * Host name of IP address of the peer.
   */
  std::string hostName;

  /**
   * Ref of the most recently sent packet for which we received a reply.
   *
   * If no reordering happened, this also is the ref of the most recently
   * received packet, but if reordering happens, another packet that
   * referenced a request that was sent earlier might have been received later.
   */
  std::uint32_t lastReceivedRef;

  /**
   * Round-trip time associated with the packet referenced by lastReveivedRef.
   */
  Clock::duration lastReceivedRoundTripTime;

  /**
   * Send time of the packet associated with lastReceivedRef.
   */
  Clock::time_point lastReceivedSendTime;

  /**
   * Mutex that protects access to shared variables.
   */
  std::recursive_mutex mutex;

  /**
   * New requests that have not been sent yet.
   */
  std::list<std::shared_ptr<Request>> newRequests;

  /**
   * Ref that is going to be used for the next packet that is sent.
   */
  std::uint32_t nextRef = 0;

  /**
   * Timeout determining how long a request can stay in the queue before being
   * sent.
   *
   * If this timeout expires before a request can be sent for the first time,
   * the request times out without ever being sent and the callback is
   * notified.
   */
  Clock::duration queueTimeout;

  /**
   * Selector that is used by the receive thread in order to wait for I/O or
   * other events.
   */
  MrfFdSelector receiveSelector;

  /**
   * Thread that processes packets that are received from the peer.
   */
  std::thread receiveThread;

  /**
   * Map from refs to requests. This map is used to identify the request for a
   * certain ref when receiving a packet.
   */
  std::unordered_map<std::uint32_t, std::shared_ptr<Request>> refToRequest;

  /**
   * Multiplier used when calculating the reordering window.
   *
   * This is inspired by RFC 8985. The multiplier is incremented every time it
   * is detected that a packet was retransmitted too early because it arrived
   * late because of reordering. However, the multiplier has an upper limit.
   */
  int reorderingWindowMultiplier = 0;

  /**
   * Counter that counts how many successful retransmission (without a spurious
   * retransmission in between) must be performed before the reordering window
   * multiplier is reset to one.
   */
  int reorderingWindowMultiplierResetCounter = 0;

  /**
   * Set containing all the sent requests in order a packet was *first* sent.
   *
   * This is used to detect when the timeout for a request has been reached.
   * This set contains a request from the time it was first sent (and thus
   * moved from newRequests to this set) until it is successful or finally
   * times out.
   */
  std::multiset<
    std::shared_ptr<Request>,
    RequestFirstSendTimeComparator> requestsByFirstTransmission;

  /**
   * Set containing the sent requests in the order a packet was *last* sent.
   *
   * This is used to detect when a request shall be retransmitted. This set
   * contains a request from the time when it is sent until it is successful,
   * it is detected that it needs to be retransmitted, or it finally times out.
   */
  std::multiset<
    std::shared_ptr<Request>,
    RequestLastSendTimeComparator> requestsByLastTransmission;

  /**
   * Timeout determines how long a request is retried after being sent for
   * the first time.
   *
   * If no response is received within this time (regardless of
   * retransmissions), the request is considered to have failed and the
   * callback is notified.
   */
  Clock::duration requestTimeout;

  /**
   * Multiplier used when calculating the retransmission timeout.
   *
   * This is inspired by RFC 6298. The multiplier is doubled each time the
   * retransmission timeout expires and reset to one when the reply for a
   * transmission is received.
   */
  int retransmissionTimeoutMultiplier = 1;

  /**
   * Set containg requests that shall be retransmitted in the order of the time
   * when they became eligible for retransmission.
   */
  std::multiset<
    std::shared_ptr<Request>,
    RequestRetransmitTimeComparator> retransmitRequests;

  /**
   * Variation of the round-trip time.
   *
   * This is caculated according to the approach described in RC 6298.
   */
  Clock::duration roundTripTimeVariation = Clock::duration::zero();

  /**
   * Selector that is used by the send thread in order to wait for I/O or
   * other events.
   */
  MrfFdSelector sendSelector;

  /**
   * Thread that sents packets to the peer.
   */
  std::thread sendThread;

  /**
   * Indicates whether the receive and send threads shall be shutdown.
   */
  std::atomic<bool> shutdown;

  /**
   * Threshold for the congestion window at which the algorithm that increases
   * the congestion window switches from “slow start” to “congestion avoidance
   * mode”. The terms are inspired by the respective terms used in RFC 5681.
   */
  std::size_t slowStartThreshold = std::numeric_limits<std::size_t>::max();

  /**
   * Smoothed version of the measured round-trip time.
   *
   * This is caculated according to the approach described in RC 6298.
   */
  Clock::duration smoothedRoundTripTime = Clock::duration::zero();

  /**
   * File descriptor for the UDP socket.
   */
  int socketDescriptor = -1;

  /**
   * Callbacks for requests that have timed out and where the callback still
   * needs to be notified.
   */
  std::list<std::shared_ptr<RequestCallback>> timeoutCallbacks;

};

} // namespace mrf
} // namespace anka

#endif // ANKA_MRF_UDP_IP_CLIENT_H
