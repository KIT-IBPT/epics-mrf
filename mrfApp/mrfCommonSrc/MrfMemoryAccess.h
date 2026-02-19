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

#ifndef ANKA_MRF_MEMORY_ACCESS_H
#define ANKA_MRF_MEMORY_ACCESS_H

#include <cstdint>
#include <memory>
#include <string>

namespace anka {
namespace mrf {

/**
 * Base class for accessing the memory of an MRF device. This class hides the
 * details of how the memory is accessed (e.g. memory-mapped I/O, network
 * sockets, etc.). Those details are implemented by the child classes derived
 * from this class.
 */
class MrfMemoryAccess {

public:

  /**
   * Error codes for a failed operation.
   */
  enum class ErrorCode {

    /**
     * The cause of the error is unknown.
     */
    unknown,

    /**
     * The read or write address was invalid.
     */
    invalidAddress,

    /**
     * The FPGA did not reply in time.
     */
    fpgaTimeout,

    /**
     * Not network response was received in time.
     */
    networkTimeout,

    /**
     * The specified command was invalid (should never happen).
     */
    invalidCommand
  };

  /**
   * Readback behavior when writing to a register.
   */
  enum class ReadbackMode {

    /**
     * The write operation may but does not have to read back the register that
     * was just written. This mode is always supported.
     */
    may,

    /**
     * The write operation must read the back the register that was just
     * written and provide the value to the calling code. This mode is always
     * supported.
     */
    must,

    /**
     * The write operation must not read back the register that was just
     * written. This mode is not supported when accessing a device through
     * MrfUdpIpMemoryAccessV1.
     */
    mustNot
  };

  /**
   * Interface for a memory-access callback. Callbacks allow memory access in an
   * asynchronous way, so that a register can be read or written without having
   * to wait until the operation finishes.
   */
  template<typename T>
  class Callback {

  public:

    /**
     * Called when a read or write operation succeeds. The address passed is the
     * address specified in the read or write request. The value passed is the
     * value read from the device memory (unless a write operation specifies
     * ReadbackMode::may or ReadbackMode::mustNot).
     */
    virtual void success(std::uint32_t address, T value) = 0;

    /**
     * Called when a read or write operation fails finally. The address passed
     * is the address specified in the read or write request. The error code
     * gives information about the cause of the failure. The optional string
     * may give additional information about the cause of the error (e.g. the
     * system call that failed), but it may also be empty.
     */
    virtual void failure(std::uint32_t address, ErrorCode errorCode,
        const std::string &details) =0;

    /**
     * Default constructor.
     */
    Callback() {
    }

    /**
     * Destructor. Virtual classes should have a virtual destructor.
     */
    virtual ~Callback() {
    }

    // We do not want to allow copy or move construction or assignment.
    Callback(const Callback &) = delete;
    Callback(Callback &&) = delete;
    Callback &operator=(const Callback &) = delete;
    Callback &operator=(Callback &&) = delete;

  };

  /**
   * Listener that is notified when a device generates an interrupt. Such a
   * listener can be registered with an {@link MrfMemoryAccess} that supports
   * interrupts.
   */
  class InterruptListener {

  public:

    /**
     * Notifies the listener that the device has generated an interrupt. The
     * state of the interrupt flag register at the time of receiving the
     * interrupt is passed to this method. Only those bits that are set in the
     * interrupt flag register and have interrupts enabled are set in the passed
     * value. The interrupt status is reset by the calling code, so the listener
     * does not have to take care of this.
     */
    virtual void operator()(std::uint32_t interruptFlags) =0;

    /**
     * Default constructor.
     */
    InterruptListener() {
    }

    /**
     * Destructor. Virtual classes should have a virtual destructor.
     */
    virtual ~InterruptListener() {
    }

    // We do not want to allow copy or move construction or assignment.
    InterruptListener(const InterruptListener &) = delete;
    InterruptListener(InterruptListener &&) = delete;
    InterruptListener &operator=(const InterruptListener &) = delete;
    InterruptListener &operator=(InterruptListener &&) = delete;

  };

  /**
   * Callback for reading from or writing to an unsigned 16-bit register.
   */
  using CallbackUInt16 = Callback<std::uint16_t>;

  /**
   * Callback for reading from or writing to an unsigned 32-bit register.
   */
  using CallbackUInt32 = Callback<std::uint32_t>;

  /**
   * Default constructor.
   */
  MrfMemoryAccess();

  /**
   * Reads from an unsigned 32-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On success,
   * the value read from the specified memory address is returned. On failure,
   * an exception is thrown.
   */
  virtual std::uint16_t readUInt16(std::uint32_t address);

  /**
   * Writes to an unsigned 16-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On
   * success, the value read from the memory (after writing to it) may be
   * returned (depending on the specified readback mode). On failure, an
   * exception is thrown.
   */
  virtual std::uint16_t writeUInt16(
    std::uint32_t address,
    std::uint16_t value,
    ReadbackMode readbackMode = ReadbackMode::must
  );

  /**
   * Reads from an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called. Implementations that can read
   * without blocking might call the callback directly in the calling thread.
   */
  virtual void readUInt16(std::uint32_t address,
      std::shared_ptr<CallbackUInt16> callback) = 0;

  /**
   * Writes to an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called. Implementations that can write
   * without blocking might call the callback directly in the calling thread.
   */
  virtual void writeUInt16(
    std::uint32_t address,
    std::uint16_t value,
    std::shared_ptr<CallbackUInt16> callback,
    ReadbackMode readbackMode = ReadbackMode::must
  ) = 0;

  /**
   * Reads from an unsigned 32-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On success,
   * the value read from the specified memory address is returned. On failure,
   * an exception is thrown.
   */
  virtual std::uint32_t readUInt32(std::uint32_t address);

  /**
   * Writes to an unsigned 32-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On
   * success, the value read from the memory (after writing to it) may be
   * returned (depending on the specified readback mode). On failure, an
   * exception is thrown.
   */
  virtual std::uint32_t writeUInt32(
    std::uint32_t address,
    std::uint32_t value,
    ReadbackMode readbackMode = ReadbackMode::must
  );

  /**
   * Reads from an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called. Implementations that can read
   * without blocking might call the callback directly in the calling thread.
   */
  virtual void readUInt32(std::uint32_t address,
      std::shared_ptr<CallbackUInt32> callback) = 0;

  /**
   * Writes to an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called. Implementations that can write
   * without blocking might call the callback directly in the calling thread.
   */
  virtual void writeUInt32(
    std::uint32_t address,
    std::uint32_t value,
    std::shared_ptr<CallbackUInt32> callback,
    ReadbackMode readbackMode = ReadbackMode::must
  ) = 0;

  /**
   * Tells whether this memory access supports interrupts. If the memory access
   * is able to intercept interrupts generated by the device, this method
   * returns {@code true}, otherwise it returns {@code false}.
   *
   * Subclasses that support interrupts must override this method along with the
   * {@link addInterruptListener(std::shared_ptr<InterruptListener>
   * interruptListener)} and {@link removeInterruptListener(
   * std::shared_ptr<InterruptListener> interruptListener)} methods.
   */
  virtual bool supportsInterrupts() const;

  /**
   * Adds the specified listener to the list of listeners that are notified when
   * the device generates an interrupt. If the specified listener has already
   * been registered with this memory access, calling this method has no effect.
   *
   * The listeners are internally kept using weak pointers. This means that a
   * listener will be destroyed if no other references to it are being hold.
   *
   * This method may only be called if this memory access supports interrupts
   * ({@link supportsInterrupts()} returns {@code true}). Calling this method
   * on a memory access that does not support interrupts results in an exception
   * being thrown.
   *
   * Subclasses that support interrupts must override this method along with the
   * {@link supportsInterrupts()} and {@link removeInterruptListener(
   * std::shared_ptr<InterruptListener> interruptListener)} methods.
   */
  virtual void addInterruptListener(
      std::shared_ptr<InterruptListener> interruptListener);

  /**
   * Removes the specified listener from the list of listeners that are notified
   * when the device generates an interrupt. If the specified listener has
   * already been removed (or has never been added), calling this method has no
   * effect.
   *
   * This method may only be called if this memory access supports interrupts
   * ({@link supportsInterrupts()} returns {@code true}). Calling this method
   * on a memory access that does not support interrupts results in an exception
   * being thrown.
   *
   * Subclasses that support interrupts must override this method along with the
   * {@link supportsInterrupts()} and {@link addInterruptListener(
   * std::shared_ptr<InterruptListener> interruptListener)} methods.
   */
  virtual void removeInterruptListener(
      std::shared_ptr<InterruptListener> interruptListener);

protected:

  /**
   * The destructor is protected so that an instance cannot be destroyed through
   * a pointer to the base class. Virtual classes should always have a virtual
   * destructor.
   */
  virtual ~MrfMemoryAccess();

private:

  // We do not want to allow copy or move construction or assignment.
  MrfMemoryAccess(const MrfMemoryAccess &) = delete;
  MrfMemoryAccess(MrfMemoryAccess &&) = delete;
  MrfMemoryAccess &operator=(const MrfMemoryAccess &) = delete;
  MrfMemoryAccess &operator=(MrfMemoryAccess &&) = delete;

};

/**
 * Converts a memory address to its hexadecimal string representation.
 */
std::string mrfMemoryAddressToString(std::uint32_t address);

/**
 * Converts an error code to a human readable string.
 */
std::string mrfErrorCodeToString(MrfMemoryAccess::ErrorCode errorCode);

}
}

#endif // ANKA_MRF_MEMORY_ACCESS_H
