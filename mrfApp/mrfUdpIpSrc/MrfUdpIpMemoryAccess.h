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

#ifndef ANKA_MRF_UDP_IP_MEMORY_ACCESS_H
#define ANKA_MRF_UDP_IP_MEMORY_ACCESS_H

#include <atomic>
#include <list>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

extern "C" {
#include <sys/select.h>
}

#include <MrfFdSelector.h>
#include <MrfMemoryAccess.h>
#include <MrfTime.h>

namespace anka {
namespace mrf {

/**
 * MRF memory access implementation that provides access to an MRF device
 * through the UDP/IP protocol. At the time of writing, only the VME-based
 * devices (e.g. VME-EVG-230) support this kind of access.
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
   * Creates a memory-access object for an MRF device that can be controlled
   * via UDP/IP. The specified host name can either be a DNS name or an IP
   * address. The base address is added to the address specified in a read or
   * write request. This is useful if different devices with the same register
   * layout but different base addresses shall be accessed.
   * The constructor creates two background threads that take care of
   * communicating with the device. The minimum delay between sending two
   * packets is initialized with a default of 400 microseconds, the timeout is
   * initialized with a default value of 5 milliseconds, and the maximum number
   * of retries is initialized with a default value of 5. Throws an exception if
   * the socket cannot be initialized or the background threads cannot be
   * created.
   */
  MrfUdpIpMemoryAccess(const std::string &hostName, std::uint32_t baseAddress);

  /**
   * Creates a memory-access object for an MRF device that can be controlled
   * via UDP/IP. The specified host name can either be a DNS name or an IP
   * address. The base address is added to the address specified in a read or
   * write request. This is useful if different devices with the same register
   * layout but different base addresses shall be accessed.
   * The constructor creates two background threads that take care of
   * communicating with the device. The delay between packets is the minimum
   * time that is spent between sending too packets. Thus this setting
   * effectively limits the maximum data rate. The UDP timeout is the maximum
   * time between sending a request and receiving the response. If the response
   * is not received within this interval, the request is sent again. This
   * parameter should typically be about five times the round-trip time for the
   * network. The maximum number of tries specified how often a request that
   * timed out is sent again before failing permanently. Throws an exception if
   * the socket cannot be initialized, the background threads cannot be
   * created, or if one of the parameters is invalid.
   */
  MrfUdpIpMemoryAccess(const std::string &hostName, std::uint32_t baseAddress,
      const MrfTime &delayBetweenPackets, const MrfTime &udpTimeout,
      int maximumNumberOfTries);

  /**
   * Destructor. Closes the connection to the device and terminates the
   * background thread.
   */
  virtual ~MrfUdpIpMemoryAccess();

  /**
   * Reads from an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void readUInt16(std::uint32_t address,
      std::shared_ptr<CallbackUInt16> callback);

  /**
   * Writes to an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void writeUInt16(std::uint32_t address, std::uint16_t value,
      std::shared_ptr<CallbackUInt16>);

  /**
   * Reads from an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void readUInt32(std::uint32_t address,
      std::shared_ptr<CallbackUInt32> callback);

  /**
   * Writes to an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.
   */
  virtual void writeUInt32(std::uint32_t address, std::uint32_t value,
      std::shared_ptr<CallbackUInt32>);

  // We want the methods from the base class to participate in overload
  // resolution.
  using MrfMemoryAccess::readUInt16;
  using MrfMemoryAccess::readUInt32;
  using MrfMemoryAccess::writeUInt16;
  using MrfMemoryAccess::writeUInt32;

private:

  /**
   * Data structure for a UDP packet sent to or received from the MRF VME
   * modules.
   */
// We have to pack the structure so that it matches the network representation.
#pragma pack(push, 1)
  struct MrfUdpPacket {
    std::uint8_t accessType;
    std::int8_t status;
    std::uint16_t data;
    std::uint32_t address;
    std::uint32_t ref;
  };
#pragma pack(pop)

  /**
   * Data structure that is used for the internal request callbacks.
   */
  struct MrfRequestCallback {
    virtual ~MrfRequestCallback() {}

    virtual void operator()(std::uint16_t receivedData, std::int8_t status,
        bool timeout) = 0;
  };

  /**
   * Internal callback for a uint16 read or write request.
   */
  struct UInt16Callback: MrfRequestCallback {
    std::uint32_t address;
    std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback;

    UInt16Callback(std::uint32_t address,
        std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback);

    void operator()(std::uint16_t receivedData, std::int8_t status,
        bool timeout);
  };

  /**
   * Data structure that is shared by the callbacks for a uint32 read request.
   */
  struct UInt32ReadShared: std::enable_shared_from_this<UInt32ReadShared> {
    MrfUdpIpMemoryAccess &memoryAccess;
    std::mutex mutex;
    std::uint32_t address;
    std::uint32_t data = 0;
    bool failed = false;
    bool gotLow = false;
    bool gotHigh = false;
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback;

    UInt32ReadShared(MrfUdpIpMemoryAccess &memoryAccess, std::uint32_t address,
        std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback);

    void receivedLow(std::uint16_t data);
    void receivedHigh(std::uint16_t data);
    void failure(MrfMemoryAccess::ErrorCode errorCode,
        const std::string &details);
  };

  /**
   * Internal callback for reading the low word of a uint32 register.
   */
  struct UInt32ReadLowCallback: MrfRequestCallback {
    std::shared_ptr<UInt32ReadShared> sharedData;

    UInt32ReadLowCallback(std::shared_ptr<UInt32ReadShared> sharedData);

    void operator()(std::uint16_t receivedData, std::int8_t status,
        bool timeout);
  };

  /**
   * Internal callback for reading the high word of a uint32 register.
   */
  struct UInt32ReadHighCallback: MrfRequestCallback {
    std::shared_ptr<UInt32ReadShared> sharedData;

    UInt32ReadHighCallback(std::shared_ptr<UInt32ReadShared> sharedData);

    void operator()(std::uint16_t receivedData, std::int8_t status,
        bool timeout);
  };

  /**
   * Internal callback for writing the low word of a uint32 register.
   */
  struct UInt32WriteLowCallback: MrfRequestCallback {
    std::uint32_t address;
    std::uint16_t highData;
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback;

    UInt32WriteLowCallback(std::uint32_t address, std::uint16_t highData,
        std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback);

    void operator()(std::uint16_t receivedData, std::int8_t status,
        bool timeout);
  };

  /**
   * Internal callback for writing the high word of a uint32 register.
   */
  struct UInt32WriteHighCallback: MrfRequestCallback {
    MrfUdpIpMemoryAccess &memoryAccess;
    std::uint32_t address;
    std::uint16_t lowData;
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback;

    UInt32WriteHighCallback(MrfUdpIpMemoryAccess &memoryAccess,
        std::uint32_t address, std::uint16_t lowData,
        std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback);

    void operator()(std::uint16_t receivedData, std::int8_t status,
        bool timeout);
  };

  /**
   * Data structure storing all data associated with the request for sending a
   * UDP packet.
   */
  struct MrfRequest {
    MrfUdpPacket packet;
    std::shared_ptr<MrfRequestCallback> callback;
    int numberOfTries;
    MrfTime timeout;
  };

  // We do not want to allow copy or move construction or assignment.
  MrfUdpIpMemoryAccess(const MrfUdpIpMemoryAccess &) = delete;
  MrfUdpIpMemoryAccess(MrfUdpIpMemoryAccess &&) = delete;
  MrfUdpIpMemoryAccess &operator=(const MrfUdpIpMemoryAccess &) = delete;
  MrfUdpIpMemoryAccess &operator=(MrfUdpIpMemoryAccess &&) = delete;

  std::string hostName;
  std::uint32_t baseAddress;
  std::recursive_mutex mutex;
  std::thread receiveThread;
  std::thread sendThread;
  std::atomic<bool> shutdown;

  // Experiments have shown that the MRF VME-EVG can process packets at a rate
  // of roughly one packet every 400 microseconds without losing packets.
  MrfTime delayBetweenPackets;

  // Experiments have shown that for a direct connection the round-trip time of
  // a request is about 750 microseconds and for a connection over a switch it
  // is about 770 microseconds. Therefore, a default timeout of 5 ms is
  // reasonable for the typical setup (MRF EVG or EVR is in the same network
  // segment as the controlling computer and there are only a few switches in
  // between).
  MrfTime udpTimeout;

  int maximumNumberOfTries;

  int socketDescriptor = -1;
  MrfFdSelector receiveSelector;
  MrfFdSelector sendSelector;

  std::list<MrfRequest> requestQueue;
  std::unordered_map<std::uint32_t, MrfRequest> pendingRequests;
  std::uint32_t nextRequestCounter = 0;

  /**
   * Queues a request for reading a word from a memory address.
   */
  void queueReadRequest(std::uint32_t address,
      std::shared_ptr<MrfRequestCallback> callback);

  /**
   * Queues a request for writing a word to a memory address.
   */
  void queueWriteRequest(std::uint32_t address, std::uint16_t data,
      std::shared_ptr<MrfRequestCallback> callback);

  /**
   * Main function of the receive thread.
   */
  void runReceiveThread();

  /**
   * Main function of the send thread.
   */
  void runSendThread();

};

}
}

#endif // ANKA_MRF_UDP_IP_MEMORY_ACCESS_H
