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

#ifndef ANKA_MRF_CONSISTENT_ASYNCHRONOUS_MEMORY_ACCESS_H
#define ANKA_MRF_CONSISTENT_ASYNCHRONOUS_MEMORY_ACCESS_H

#include <forward_list>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "MrfConsistentMemoryAccess.h"

namespace anka {
namespace mrf {

/**
 * Consistent memory-access for asynchronous memory-access implementations.
 * This implementation delegates the read and write operations to a memory
 * access that is passed to the constructor. Write and update operations are
 * queued so that the internal mutex is only hold for a short amount of time.
 * Therefore, this wrapper can be used without asynchronous memory-access
 * implementations where a write operation might block. It can also be used with
 * synchronous memory-access implementations, but a different implementation
 * might be more efficient.
 */
class MrfConsistentAsynchronousMemoryAccess: public MrfConsistentMemoryAccess {

public:

  /**
   * Creates a consistent memory-access wrapping the specified (asynchronous)
   * memory-access. The wrapped memory access must be kept alive until all
   * operations queued in this memory access have been finished.
   */
  explicit MrfConsistentAsynchronousMemoryAccess(MrfMemoryAccess &delegate) :
      impl(std::make_shared<Impl>(delegate)) {
  }

  /**
   * Creates a consistent memory-access wrapping the specified (asynchronous)
   * memory-access. The shared pointer to the wrapped memory-access is kept
   * alive until it is not needed any longer. This can be longer than the
   * lifetime of the object created because callbacks that depend on the memory
   * access might still be queued when this object is destructed.
   */
  explicit MrfConsistentAsynchronousMemoryAccess(
      std::shared_ptr<MrfMemoryAccess> delegate) :
      impl(std::make_shared<Impl>(delegate)) {
  }

  /**
   * Reads from an unsigned 16-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On success,
   * the value read from the specified memory address is returned. On failure,
   * an exception is thrown. This method delegates the read operation to the
   * memory access which has been passed to the constructor.
   */
  inline std::uint16_t readUInt16(std::uint32_t address) {
    return impl->delegate.readUInt16(address);
  }

  /**
   * Writes to an unsigned 16-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On
   * success, the value read from the memory (after writing to it) may be
   * returned (depending on the specified readback mode). On failure, an
   * exception is thrown. This method delegates the write operation to the
   * memory access which has been passed to the constructor. However, the
   * operation is delayed automatically, if a concurrent update operation to
   * the same register is in progress.
   */
  inline std::uint16_t writeUInt16(
    std::uint32_t address,
    std::uint16_t value,
    ReadbackMode readbackMode = ReadbackMode::must
  ) {
    // We delegate to the implementation in the super class. This will
    // indirectly call our asynchronous implementation.
    return MrfMemoryAccess::writeUInt16(address, value, readbackMode);
  }

  /**
   * Reads from an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called.  This method delegates the read
   * operation to the memory access which has been passed to the constructor.
   * Implementations that can read without blocking might call the callback
   * directly in the calling thread.
   */
  inline void readUInt16(std::uint32_t address,
      std::shared_ptr<CallbackUInt16> callback) {
    impl->delegate.readUInt16(address, callback);
  }

  /**
   * Writes to an unsigned 16-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called. This method delegates the write
   * operation to the memory access which has been passed to the constructor.
   * However, the operation is delayed automatically, if a concurrent update
   * operation to the same register is in progress. Implementations that can
   * write without blocking might call the callback directly in the calling
   * thread.
   */
  inline void writeUInt16(
    std::uint32_t address,
    std::uint16_t value,
    std::shared_ptr<CallbackUInt16> callback,
    ReadbackMode readbackMode = ReadbackMode::must
  ) {
    impl->writeUInt16(address, value, callback, readbackMode);
  }

  /**
   * Reads from an unsigned 32-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On success,
   * the value read from the specified memory address is returned. On failure,
   * an exception is thrown.  This method delegates the read operation to the
   * memory access which has been passed to the constructor.
   */
  inline std::uint32_t readUInt32(std::uint32_t address) {
    return impl->delegate.readUInt32(address);
  }

  /**
   * Writes to an unsigned 32-bit register. The method blocks until the
   * operation has finished (either successfully or unsuccessfully). On
   * success, the value read from the memory (after writing to it) may be
   * returned (depending on the specified readback mode). On failure, an
   * exception is thrown. This method delegates the write operation to the
   * memory access which has been passed to the constructor. However, the
   * operation is delayed automatically, if a concurrent update operation to
   * the same register is in progress.
   */
  inline std::uint32_t writeUInt32(
    std::uint32_t address,
    std::uint32_t value,
    ReadbackMode readbackMode = ReadbackMode::must
  ) {
    // We delegate to the implementation in the super class. This will
    // indirectly call our asynchronous implementation.
    return MrfMemoryAccess::writeUInt32(address, value, readbackMode);
  }

  /**
   * Reads from an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called. This method delegates the read
   * operation to the memory access which has been passed to the constructor.
   * Implementations that can read without blocking might call the callback
   * directly in the calling thread.
   */
  inline void readUInt32(std::uint32_t address,
      std::shared_ptr<CallbackUInt32> callback) {
    impl->delegate.readUInt32(address, callback);
  }

  /**
   * Writes to an unsigned 32-bit register. This method does not block. The
   * operation is queued and executed asynchronously. When the operation
   * finishes, the specified callback is called. This method delegates the write
   * operation to the memory access which has been passed to the constructor.
   * However, the operation is delayed automatically, if a concurrent update
   * operation to the same register is in progress. Implementations that can
   * write without blocking might call the callback directly in the calling
   * thread.
   */
  inline void writeUInt32(
    std::uint32_t address,
    std::uint32_t value,
    std::shared_ptr<CallbackUInt32> callback,
    ReadbackMode readbackMode = ReadbackMode::must
  ) {
    return impl->writeUInt32(address, value, callback, readbackMode);
  }

  /**
   * Updates an unsigned 16-bit register in a consistent way. The register's
   * value is read, then the callback's update method is called, and finally
   * the updated value is written to the register. Other write or update
   * operations to the same register are blocked until this process has
   * finished. This method does not block.
   */
  inline void updateUInt16(std::uint32_t address,
      std::shared_ptr<UpdatingCallbackUInt16> callback) {
    return impl->updateUInt16(address, callback);
  }

  /**
   * Updates an unsigned 32-bit register in a consistent way. The register's
   * value is read, then the callback's update method is called, and finally
   * the updated value is written to the register. Other write or update
   * operations to the same register are blocked until this process has
   * finished. This method does not block.
   */
  inline void updateUInt32(std::uint32_t address,
      std::shared_ptr<UpdatingCallbackUInt32> callback) {
    return impl->updateUInt32(address, callback);
  }

  // We want the methods from the base class to participate in overload
  // resolution.
  using MrfConsistentMemoryAccess::writeUInt16;
  using MrfConsistentMemoryAccess::writeUInt32;

  /**
   * Tells whether this memory access supports interrupts. If the memory access
   * is able to intercept interrupts generated by the device, this method
   * returns {@code true}, otherwise it returns {@code false}.
   *
   * This memory access supports interrupts if (and only if) the backing memory
   * access supports interrupts.
   */
  inline bool supportsInterrupts() const {
    return impl->delegate.supportsInterrupts();
  }

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
   */
  inline void addInterruptListener(
      std::shared_ptr<InterruptListener> interruptListener) {
    return impl->delegate.addInterruptListener(interruptListener);
  }

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
   */
  void removeInterruptListener(
      std::shared_ptr<InterruptListener> interruptListener) {
    return impl->delegate.removeInterruptListener(interruptListener);
  }

private:

  /**
   * This is the actual implementation. It is in a separate, private class
   * because it might need to be kept alive beyond the lifetime of the
   * surrounding object. It can only be destroyed if the surrounding object has
   * been destroyed and no callbacks are queued any longer.
   */
  class Impl: public std::enable_shared_from_this<Impl> {

  public:

    Impl(MrfMemoryAccess &delegate);
    Impl(std::shared_ptr<MrfMemoryAccess> delegate);

    MrfMemoryAccess &delegate;
    std::shared_ptr<MrfMemoryAccess> delegatePtr;

    void writeUInt16(
      std::uint32_t address,
      std::uint16_t value,
      std::shared_ptr<CallbackUInt16> callback,
      ReadbackMode readbackMode
    );

    void writeUInt32(
      std::uint32_t address,
      std::uint32_t value,
      std::shared_ptr<CallbackUInt32> callback,
      ReadbackMode readbackMode
    );

    void updateUInt16(std::uint32_t address,
        std::shared_ptr<UpdatingCallbackUInt16> callback);

    void updateUInt32(std::uint32_t address,
        std::shared_ptr<UpdatingCallbackUInt32> callback);

  private:

    /**
     * Type of a queued operation. Read operations are not queued because they
     * do not interfere with update operations.
     */
    enum class OperationType {
      writeUInt16, writeUInt32, updateUInt16, updateUInt32
    };

    /**
     * Structure holding information about an operation. This structure
     * (combined with the callback and write value that is stored separately)
     * stores all the information that is needed to process the operation at a
     * later point in time.
     */
    struct OperationInfo {
      unsigned long id;
      OperationType type;
      std::uint32_t address;

      inline std::uint32_t width() const {
        switch (type) {
        case OperationType::writeUInt16:
        case OperationType::updateUInt16:
          return 2;
        case OperationType::writeUInt32:
        case OperationType::updateUInt32:
          return 4;
        default:
          // This should never happen as we handle all operation types.
          return 0;
        }
      }
    };

    /**
     * Internal callback for update operations. It is used for both stages of
     * the update operation (read and write).
     */
    template<typename T>
    class UpdateCallback: public MrfMemoryAccess::Callback<T>,
        public std::enable_shared_from_this<UpdateCallback<T>> {
    public:
      OperationInfo operationInfo;
      bool readFinished = false;
      std::shared_ptr<Impl> impl;
      std::shared_ptr<MrfConsistentMemoryAccess::UpdatingCallback<T>> delegate;

      void success(std::uint32_t address, T value);
      void failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
          const std::string &details);

    private:
      void write(T newValue);
    };

    /**
     * Internal callback for write operations.
     */
    template<typename T>
    struct WriteCallback: MrfMemoryAccess::Callback<T> {
      OperationInfo operationInfo;
      std::shared_ptr<Impl> impl;
      std::shared_ptr<MrfMemoryAccess::Callback<T>> delegate;

      void success(std::uint32_t address, T value);
      void failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
          const std::string &details);
    };

    using WriteUInt16Args = std::tuple<
      std::shared_ptr<CallbackUInt16>, std::uint16_t, ReadbackMode
    >;

    using WriteUInt32Args = std::tuple<
      std::shared_ptr<CallbackUInt32>, std::uint32_t, ReadbackMode
    >;

    std::recursive_mutex mutex;
    unsigned long nextId = 0;
    std::unordered_multimap<std::uint32_t, OperationInfo> pendingOperations;
    std::unordered_set<std::uint32_t> operationRunning;
    std::unordered_map<unsigned long, WriteUInt16Args> writeUInt16Args;
    std::unordered_map<unsigned long, WriteUInt32Args> writeUInt32Args;
    std::unordered_map<unsigned long, std::shared_ptr<CallbackUInt16>> updateUInt16Callbacks;
    std::unordered_map<unsigned long, std::shared_ptr<CallbackUInt32>> updateUInt32Callbacks;

    void insertOperationInfo(const OperationInfo &operationInfo);
    void removeOperationInfo(const OperationInfo &operationInfo);
    std::forward_list<OperationInfo> prepareNextOperations(
        const OperationInfo &operationInfo);
    void runOperation(const OperationInfo &operationInfo);
    bool canRunOperation(const OperationInfo &operationInfo);
    void markRunOperation(const OperationInfo &operationInfo);
    void unmarkRunOperation(const OperationInfo &operationInfo);
    void operationFinished(const OperationInfo &operationInfo);

  };

  // We do not want to allow copy or move construction or assignment.
  MrfConsistentAsynchronousMemoryAccess(
      const MrfConsistentAsynchronousMemoryAccess &) = delete;
  MrfConsistentAsynchronousMemoryAccess(
      MrfConsistentAsynchronousMemoryAccess &&) = delete;
  MrfConsistentAsynchronousMemoryAccess &operator=(
      const MrfConsistentAsynchronousMemoryAccess &) = delete;
  MrfConsistentAsynchronousMemoryAccess &operator=(
      MrfConsistentAsynchronousMemoryAccess &&) = delete;

  std::shared_ptr<Impl> impl;

};

template<typename T>
void MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::UpdateCallback<
    T>::success(std::uint32_t address, T value) {
  if (readFinished) {
    try {
      impl->operationFinished(operationInfo);
    } catch (...) {
      // The code should not throw, but if it does, we still want to call the
      // delegate's method. We do not rethrow the exception because it would be
      // discarded by the calling code anyway.
    }
    delegate->success(address, value);
  } else {
    readFinished = true;
    try {
      T newValue = delegate->update(address, value);
      write(newValue);
    } catch (std::exception &e) {
      // If the update or the write method throws an exception, we have to make
      // sure that all resources get cleaned up.
      failure(address, ErrorCode::unknown,
          std::string("The callback's update method threw an exception: ")
              + e.what());
      return;
    } catch (...) {
      // If the update or the write method throws an exception, we have to make
      // sure that all resources get cleaned up.
      failure(address, ErrorCode::unknown,
          std::string("The callback's update method threw an exception."));
      return;
    }
  }
}

template<typename T>
void MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::UpdateCallback<
    T>::failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
    const std::string &details) {
  try {
    impl->operationFinished(operationInfo);
  } catch (...) {
    // The code should not throw, but if it does, we still want to call the
    // delegate's method. We do not rethrow the exception because it would be
    // discarded by the calling code anyway.
  }
  delegate->failure(address, errorCode, details);
}

template<>
inline void MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::UpdateCallback<
    std::uint16_t>::write(std::uint16_t newValue) {
  impl->delegate.writeUInt16(operationInfo.address, newValue,
      shared_from_this());
}

template<>
inline void MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::UpdateCallback<
    std::uint32_t>::write(std::uint32_t newValue) {
  impl->delegate.writeUInt32(operationInfo.address, newValue,
      shared_from_this());
}

template<typename T>
void MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::WriteCallback<
    T>::success(std::uint32_t address, T value) {
  try {
    impl->operationFinished(operationInfo);
  } catch (...) {
    // The code should not throw, but if it does, we still want to call the
    // delegate's method. We do not rethrow the exception because it would be
    // discarded by the calling code anyway.
  }
  if (delegate) {
    delegate->success(address, value);
  }
}

template<typename T>
void MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::WriteCallback<
    T>::failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
    const std::string &details) {
  try {
    impl->operationFinished(operationInfo);
  } catch (...) {
    // The code should not throw, but if it does, we still want to call the
    // delegate's method. We do not rethrow the exception because it would be
    // discarded by the calling code anyway.
  }
  if (delegate) {
    delegate->failure(address, errorCode, details);
  }
}

} // namespace mrf
} // namespace anka

#endif // ANKA_MRF_CONSISTENT_ASYNCHRONOUS_MEMORY_ACCESS_H
