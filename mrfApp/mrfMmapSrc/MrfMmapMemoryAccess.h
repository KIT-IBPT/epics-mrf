/*
 * Copyright 2016-2026 aquenos GmbH.
 * Copyright 2016-2026 Karlsruhe Institute of Technology.
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

#ifndef ANKA_MRF_MMAP_MEMORY_ACCESS_H
#define ANKA_MRF_MMAP_MEMORY_ACCESS_H

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <MrfFdSelector.h>
#include <MrfMemoryAccess.h>

namespace anka {
namespace mrf {

/**
 * Provides access to an MRF device through memory-mapped I/O. This
 * implementation is aimed at working with the MRF kernel driver for Linux,
 * using the device nodes created by that driver for communication with the
 * hardware.
 */
class MrfMmapMemoryAccess: public MrfMemoryAccess {

public:

  /**
   * Creates a memory-access object for an MRF device that is accessed by using
   * mmap(...) on a device node. The specified path must point to the device
   * node that provides access to the device's FPGA memory. Typically, this is
   * the fourth minor device provided by the MRF kernel module (e.g. "/dev/era3"
   * or "/dev/egb3").
   *
   * The specified memory size represents the number of bytes that can be
   * accessed in the device's memory and depends on the exact device type.
   * Specifying a value that is too large might result in an error when
   * accessing the device. Specifying a value that is too small will result in
   * parts of the device's memory not being accessible.
   *
   * The constructor creates a background thread that takes
   * care of communicating with the device. Throws an exception if the
   * background thread cannot be created.
   */
  MrfMmapMemoryAccess(const std::string &devicePath,
      const std::uint32_t memorySize);

  /**
   * Destructor. Closes the connection to the device and terminates the
   * background thread.
   */
  virtual ~MrfMmapMemoryAccess();

  /**
   * Registers a signal handler that handles SIGBUS events. Such events occur
   * when there is an error while reading from or writing to the devices memory.
   * This signal handler is not strictly necessary for operation. However, when
   * it is registered, it can gracefully recover from I/O errors instead of the
   * whole application being killed. This method should be called once before
   * creating any threads. If another signal handler is already registered for
   * SIGBUS, the newly installed signal handler will try to delegate events
   * which are caused by other code to that handler.
   */
  static void registerSignalHandler();

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
  virtual void writeUInt16(
    std::uint32_t address,
    std::uint16_t value,
    std::shared_ptr<CallbackUInt16>,
    ReadbackMode readbackMode = ReadbackMode::must
  );

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
  virtual void writeUInt32(
    std::uint32_t address,
    std::uint32_t value,
    std::shared_ptr<CallbackUInt32>,
    ReadbackMode readbackMode = ReadbackMode::must
  );

  // We want the methods from the base class to participate in overload
  // resolution.
  using MrfMemoryAccess::readUInt16;
  using MrfMemoryAccess::readUInt32;
  using MrfMemoryAccess::writeUInt16;
  using MrfMemoryAccess::writeUInt32;

  /**
   * Tells whether this memory access supports interrupts. The mmap memory
   * access always supports interrupts, so this method always returns
   * {@code true}.
   */
  virtual bool supportsInterrupts() const;

  /**
   * Adds the specified listener to the list of listeners that are notified when
   * the device generates an interrupt. If the specified listener has already
   * been registered with this memory access, calling this method has no effect.
   *
   * The listeners are internally kept using weak pointers. This means that a
   * listener will be destroyed if no other references to it are being hold.
   */
  virtual void addInterruptListener(
      std::shared_ptr<InterruptListener> interruptListener);

  /**
   * Removes the specified listener from the list of listeners that are notified
   * when the device generates an interrupt. If the specified listener has
   * already been removed (or has never been added), calling this method has no
   * effect.
   */
  virtual void removeInterruptListener(
      std::shared_ptr<InterruptListener> interruptListener);

private:

  /**
   * Type of a queued request.
   */
  enum class MrfIoRequestType {
    notSpecified,
    readUInt16,
    writeUInt16,
    writeReadUInt16,
    readUInt32,
    writeUInt32,
    writeReadUInt32
  };

  /**
   * Base class for requests to read or write data. These requests can be
   * queued for processing by the I/O thread. We use separate fields for 16-bit
   * and 32-bit requests instead of using unions. If we used a union of shared
   * pointers, we would need to handle the construction and destruction
   * explicitly and this seems like an awful lot of work (with a high risk for
   * nasty bugs) just to save a few bytes of memory.
   */
  struct MrfIoRequest {

    MrfIoRequestType type;
    std::uint32_t address;
    std::uint16_t value16;
    std::uint32_t value32;
    std::shared_ptr<CallbackUInt16> callback16;
    std::shared_ptr<CallbackUInt32> callback32;

    MrfIoRequest() :
        type(MrfIoRequestType::notSpecified), address(0), value16(0), value32(0) {
    }

    MrfIoRequest(MrfIoRequestType type, std::uint32_t address,
        std::uint16_t value, std::shared_ptr<CallbackUInt16> callback) :
        type(type), address(address), value16(value), value32(0), callback16(
            callback), callback32(nullptr) {
    }

    MrfIoRequest(MrfIoRequestType type, std::uint32_t address,
        std::uint32_t value, std::shared_ptr<CallbackUInt32> callback) :
        type(type), address(address), value16(0), value32(value), callback16(
            nullptr), callback32(callback) {
    }

    void fail(ErrorCode errorCode, const std::string& details);

  };

  // We do not want to allow copy or move construction or assignment.
  MrfMmapMemoryAccess(const MrfMmapMemoryAccess &) = delete;
  MrfMmapMemoryAccess(MrfMmapMemoryAccess &&) = delete;
  MrfMmapMemoryAccess &operator=(const MrfMmapMemoryAccess &) = delete;
  MrfMmapMemoryAccess &operator=(MrfMmapMemoryAccess &&) = delete;

  const std::string devicePath;
  const std::uint32_t memorySize;
  bool shutdown = false;
  std::mutex mutex;
  std::list<MrfIoRequest> ioQueue;
  std::thread ioThread;
  MrfFdSelector ioThreadFdSelector;
  std::vector<std::weak_ptr<InterruptListener>> interruptListeners;

  /**
   * Adds an I/O request to the queue. This method takes care of waking up the
   * I/O thread if necessary. The added request fails immediately if this device
   * has been shutdown. This method uses move semantics in order to avoid
   * copying when adding the request. This means that the specified request
   * object will possibly be invalid after calling this method.
   */
  void queueIoRequest(MrfIoRequest &&request);

  /**
   * Main function of the I/O thread.
   */
  void runIoThread();

};

}
}

#endif // ANKA_MRF_MMAP_MEMORY_ACCESS_H
