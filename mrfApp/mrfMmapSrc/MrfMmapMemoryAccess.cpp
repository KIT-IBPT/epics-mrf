/*
 * Copyright 2016-2025 aquenos GmbH.
 * Copyright 2016-2025 Karlsruhe Institute of Technology.
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

#include <atomic>
#include <cerrno>
#include <mutex>
#include <stdexcept>
#include <system_error>

extern "C" {
#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
}

#include "MrfMmapMemoryAccess.h"

namespace anka {
namespace mrf {

MrfMmapMemoryAccess::MrfMmapMemoryAccess(const std::string &devicePath,
    std::uint32_t memorySize) :
    devicePath(devicePath), memorySize(memorySize), shutdown(false) {
  // Create the background thread.
  this->ioThread = std::thread([this]() {runIoThread();});
}

MrfMmapMemoryAccess::~MrfMmapMemoryAccess() {
  // We want to terminate the background thread. We do this by setting the
  // shutdown flag and then waiting for the thread to finish.
  try {
    {
      // We have to hold the mutex when setting the shutdown flag.
      std::lock_guard<std::mutex> lock(mutex);
      shutdown = true;
    }
    // We have to wake up the I/O thread if it is sleeping.
    ioThreadFdSelector.wakeUp();
    if (ioThread.joinable()) {
      ioThread.join();
    }
  } catch (...) {
    // A destructor should never throw.
  }
}

static bool verifyAddress16(std::uint32_t address, std::uint32_t memorySize,
    std::shared_ptr<MrfMemoryAccess::CallbackUInt16> callback) {
  // The address must be within the accessible memory and we also make sure that
  // it is aligned to the size of the element that is accessed.
  if (memorySize < 2 || address > memorySize - 2 || address % 2 != 0) {
    callback->failure(address, MrfMemoryAccess::ErrorCode::invalidAddress,
        std::string());
    return false;
  } else {
    return true;
  }
}

static bool verifyAddress32(std::uint32_t address, std::uint32_t memorySize,
    std::shared_ptr<MrfMemoryAccess::CallbackUInt32> callback) {
  // The address must be within the accessible memory and we also make sure that
  // it is aligned to the size of the element that is accessed.
  if (memorySize < 4 || address > memorySize - 4 || address % 4 != 0) {
    callback->failure(address, MrfMemoryAccess::ErrorCode::invalidAddress,
        std::string());
    return false;
  } else {
    return true;
  }
}

void MrfMmapMemoryAccess::readUInt16(std::uint32_t address,
    std::shared_ptr<CallbackUInt16> callback) {
  if (!verifyAddress16(address, memorySize, callback)) {
    return;
  }
  MrfIoRequest request(MrfIoRequestType::readUInt16, address, 0, callback);
  queueIoRequest(std::move(request));
}

void MrfMmapMemoryAccess::writeUInt16(std::uint32_t address,
    std::uint16_t value, std::shared_ptr<CallbackUInt16> callback) {
  if (!verifyAddress16(address, memorySize, callback)) {
    return;
  }
  MrfIoRequest request(MrfIoRequestType::writeUInt16, address, value, callback);
  queueIoRequest(std::move(request));
}

void MrfMmapMemoryAccess::readUInt32(std::uint32_t address,
    std::shared_ptr<CallbackUInt32> callback) {
  if (!verifyAddress32(address, memorySize, callback)) {
    return;
  }
  MrfIoRequest request(MrfIoRequestType::readUInt32, address, 0, callback);
  queueIoRequest(std::move(request));
}

void MrfMmapMemoryAccess::writeUInt32(std::uint32_t address,
    std::uint32_t value, std::shared_ptr<CallbackUInt32> callback) {
  if (!verifyAddress32(address, memorySize, callback)) {
    return;
  }
  MrfIoRequest request(MrfIoRequestType::writeUInt32, address, value, callback);
  queueIoRequest(std::move(request));
}

bool MrfMmapMemoryAccess::supportsInterrupts() const {
  return true;
}

void MrfMmapMemoryAccess::addInterruptListener(
    std::shared_ptr<InterruptListener> interruptListener) {
  // We have to hold the mutex while accessing the list of listeners.
  std::lock_guard<std::mutex> lock(mutex);
  // Before trying to add a listener, we iterator over all existing listeners
  // and remove those that have become invalid. This ensures that our list does
  // not grow without bounds when listeners are added but never removed.
  // We do not increment the iterator in the header of the for loop because we
  // should not increment it when we replace it after deleting an element.
  bool listenerMissing = true;
  for (auto listenerIterator = interruptListeners.begin();
      listenerIterator != interruptListeners.end();) {
    if (listenerIterator->expired()) {
      listenerIterator = interruptListeners.erase(listenerIterator);
    } else {
      if (listenerIterator->lock() == interruptListener) {
        listenerMissing = false;
      }
      ++listenerIterator;
    }
  }
  if (listenerMissing) {
    interruptListeners.emplace_back(interruptListener);
  }
}

void MrfMmapMemoryAccess::removeInterruptListener(
    std::shared_ptr<InterruptListener> interruptListener) {
  // We have to hold the mutex while accessing the list of listeners.
  std::lock_guard<std::mutex> lock(mutex);
  // We do not increment the iterator in the header of the for loop because we
  // should not increment it when we replace it after deleting an element.
  for (auto listenerIterator = interruptListeners.begin();
      listenerIterator != interruptListeners.end();) {
    std::shared_ptr<InterruptListener> foundListener = listenerIterator->lock();
    if (!foundListener) {
      listenerIterator = interruptListeners.erase(listenerIterator);
    } else {
      if (foundListener == interruptListener) {
        listenerIterator = interruptListeners.erase(listenerIterator);
      } else {
        ++listenerIterator;
      }
    }
  }
}

// We need a helper class and a few static variables and functions for handling
// error's (in the form of SIGBUS signals) that might occur while reading from
// or writing to mmaped memory. We place this data structures in an anonymous
// namespace because we only use it in this compilation unit.
namespace {

struct MrfIoInfo {
  constexpr MrfIoInfo() :
      active(false), address(nullptr), jumpBuffer { } {
  }
  bool active;
  void *address;
  ::sigjmp_buf jumpBuffer;
};

// Strictly speaking, accessing a thread_local variable from a signal handler
// is not safe, because thread_local could be implemented using a global map
// that is protected by a lock. However, we now that for our target platform
// thread_local is implemented in a lock-free way, so we can use it.
// The alternative would be using POSIX thread-local storage
// (pthread_create_key(...), etc.), but is would also depend on the platform
// whether this would be async signal safe. For glibc, it would be async
// signal safe (see https://www.gnu.org/software/libc/manual/html_node/
// Thread_002dspecific-Data.html).
thread_local MrfIoInfo threadLocalIoInfo;

struct MrfOldSignalHandlerInfo {
  constexpr MrfOldSignalHandlerInfo() noexcept :
  action(nullptr), handler(nullptr) {
  }
  void (*action)(int, ::siginfo_t *, void *);
  void (*handler)(int);
};

// We store a pointer to the data structure instead of the data structure
// itself. On most platforms, an atomic containing the data structure cannot be
// lock free. We only set this atomic once, so memory management is not an
// issue.
std::atomic<MrfOldSignalHandlerInfo *> oldSignalHandlerInfo;

// The signal handler must have C linkage.
extern "C" {

void signalHandler(int signalNumber, ::siginfo_t *signalInfo, void *context) {
  if (signalInfo->si_signo != SIGBUS || !threadLocalIoInfo.active
      || signalInfo->si_addr != threadLocalIoInfo.address) {
    // If the signal has not been caused by our code, we delegate to a
    // previously registered signal handler, if there is any. If there is not,
    // we take the default action (terminate the program).
    MrfOldSignalHandlerInfo *oldSignalHandlerInfoTemp;
    // We can only load the old signal handler if the atomic is lock free. If it
    // is not lock free, accessing it is not async signal safe.
    if (oldSignalHandlerInfo.is_lock_free()) {
      oldSignalHandlerInfoTemp = oldSignalHandlerInfo.load(
          std::memory_order_acquire);
    }
    if (oldSignalHandlerInfoTemp == nullptr) {
      // If we do not have an old signal handler we have to restore the default
      // signal handler. This will lead to the signal being triggered again and
      // being handled by the default signal handler (which typically terminates
      // the process after generating a core dump). If we cannot restore the
      // default signal handler for some reason, we have to kill the process the
      // hard way. Simply returning from this function is not a solution because
      // it would just be called again.
      struct ::sigaction newAction;
      newAction.sa_handler = SIG_DFL;
      newAction.sa_flags = 0;
      sigemptyset(&newAction.sa_mask);
      if (::sigaction(signalNumber, &newAction, nullptr)) {
        // The exit(...) function is not async signal safe, so we have to use
        // _exit(...) which is. The return code passed is the same one that is
        // used by the default signal handler.
        _exit(128 + signalNumber);
      }
    } else if (oldSignalHandlerInfoTemp->action != nullptr) {
      oldSignalHandlerInfoTemp->action(signalNumber, signalInfo, context);
    } else if (oldSignalHandlerInfoTemp->handler != nullptr) {
      oldSignalHandlerInfoTemp->handler(signalNumber);
    }
  } else {
    // We restore the state before trying the I/O operation. This means that the
    // corresponding call to sigsetjmp will return the specified number and we
    // will thus know that the I/O operation failed.
    ::siglongjmp(threadLocalIoInfo.jumpBuffer, 1);
  }
}

} // extern "C"

bool signalHandlerRegistered = false;
std::mutex signalHandlerRegistrationMutex;

} // anonymous namespace

void MrfMmapMemoryAccess::registerSignalHandler() {
  std::lock_guard<std::mutex> lock(signalHandlerRegistrationMutex);
  // The signal handler should only be registered once.
  if (signalHandlerRegistered) {
    return;
  }
  struct ::sigaction oldAction, newAction;
  newAction.sa_sigaction = &signalHandler;
  newAction.sa_flags = SA_SIGINFO;
  sigemptyset(&newAction.sa_mask);
  if (::sigaction(SIGBUS, &newAction, &oldAction) == 0) {
    signalHandlerRegistered = true;
  }
  if ((oldAction.sa_flags & SA_SIGINFO) && oldAction.sa_sigaction != nullptr) {
    MrfOldSignalHandlerInfo *oldSignalHandlerInfoTemp =
        new MrfOldSignalHandlerInfo();
    oldSignalHandlerInfoTemp->action = oldAction.sa_sigaction;
    oldSignalHandlerInfo.store(oldSignalHandlerInfoTemp,
        std::memory_order_release);
  } else if (oldAction.sa_handler != nullptr) {
    MrfOldSignalHandlerInfo *oldSignalHandlerInfoTemp =
        new MrfOldSignalHandlerInfo();
    oldSignalHandlerInfoTemp->handler = oldAction.sa_handler;
    oldSignalHandlerInfo.store(oldSignalHandlerInfoTemp,
        std::memory_order_release);
  }

}

void MrfMmapMemoryAccess::MrfIoRequest::fail(ErrorCode errorCode,
    const std::string &details) {
  // This method assumes that the object has been initialized properly. This is
  // okay because we only use it internally and the calling code ensures this.
  switch (type) {
  case MrfIoRequestType::notSpecified:
    throw std::logic_error(
        "MrfIoRequest::fail has been called on an uninitialized object.");
  case MrfIoRequestType::readUInt16:
  case MrfIoRequestType::writeUInt16:
    try {
      if (callback16) {
        callback16->failure(address, errorCode, details);
      }
    } catch (...) {
      // We do not want an exception in a callback to bubble up into the calling
      // code.
    }
    break;
  case MrfIoRequestType::readUInt32:
  case MrfIoRequestType::writeUInt32:
    try {
      if (callback32) {
        callback32->failure(address, errorCode, details);
      }
    } catch (...) {
      // We do not want an exception in a callback to bubble up into the calling
      // code.
    }
    break;
  }

} // anonymous namespace

void MrfMmapMemoryAccess::queueIoRequest(MrfIoRequest &&request) {
  try {
    {
      // We have to hold the mutex while accessing the queue.
      std::lock_guard<std::mutex> lock(mutex);
      if (shutdown) {
        throw std::runtime_error("This device has been shutdown.");
      }
      ioQueue.emplace_back(request);
    }
  } catch (std::exception &e) {
    // This block is only triggered when the emplace (or code before the
    // emplace) failed, so our request should still be valid.
    request.fail(ErrorCode::unknown,
        std::string("The request could not be queued: ") + e.what());
  } catch (...) {
    // This block is only triggered when the emplace (or code before the
    // emplace) failed, so our request should still be valid.
    request.fail(ErrorCode::unknown,
        std::string("The request could not be queued."));
    return;
  }
  // The I/O thread might be sleeping, waiting for a new request, so we have
  // to wake it up now.
  try {
    ioThreadFdSelector.wakeUp();
  } catch (...) {
    // When the request has been queued, we cannot fail it any longer because it
    // is going to be processed eventually. For this reason, we ignore an error
    // that occurs while trying to wake up the I/O thread.
  }
}

// We use preprocessor macros for the ioctl requests instead of defining
// constants. The type of an ioctl request depends on the platform and using the
// _IO macro directly ensures that we will always use the correct type.
#define ANKA_MRF_IOCTL_IRQ_ENABLE _IO(220, 1)
// The enable macro is not used because it is a no-op in the kernel driver.
// However, it is still here in case it becomes useful in a future version of
// the kernel driver.
#define ANKA_MRF_IOCTL_IRQ_DISABLE _IO(220, 2)

static void prepareInterrupt(int fileDescriptor) {
  // We set this thread as the file owner. This ensures that SIGIO signals are
  // delivered to the I/O thread and not to the whole process.
  struct ::f_owner_ex owner;
  owner.type = F_OWNER_TID;
  owner.pid = ::syscall(SYS_gettid);
  if (::fcntl(fileDescriptor, F_SETOWN_EX, &owner) == -1) {
    throw std::system_error(
      errno, std::generic_category(), "fcntl(..., F_SETOWN_EX, ...) failed");
  }
  // If we do not set the desired signal to SIGIO explicitly, the extended
  // signal information (e.g. the file descriptor) is not included.
  if (::fcntl(fileDescriptor, F_SETSIG, SIGIO) == -1) {
    throw std::system_error(
      errno, std::generic_category(), "fcntl(..., F_SETSIG, SIGIO) failed");
  }
  int flags = ::fcntl(fileDescriptor, F_GETFL);
  if (flags == -1) {
    throw std::system_error(
      errno, std::generic_category(), "fcntl(..., F_GETFL) failed");
  }
  flags |= O_ASYNC;
  if (::fcntl(fileDescriptor, F_SETFL, flags | O_ASYNC) == -1) {
    throw std::system_error(
      errno, std::generic_category(), "fcntl(..., F_SETFL, ...) failed");
  }
}

static void enableInterrupt(int fileDescriptor) {
  if (::ioctl(fileDescriptor, ANKA_MRF_IOCTL_IRQ_ENABLE) == -1) {
    throw std::system_error(
        errno,
        std::generic_category(),
        "ioctl(...) for enabling interrupt failed");
  }
}

inline static void prepareIo(void *targetAddress) noexcept {
  // When the I/O operation fails with a SIGBUS, our signal handler ensures
  // that the execution jumps back to the point where we called sigsetjmp and
  // that this function returns a non-zero value.
  threadLocalIoInfo.address = targetAddress;
  // We have to use two fences: One before setting active and one after. The
  // first one is so that active is not going to be set before initializing
  // address and jumpBuffer. This ensures that when the signal handler is
  // executed and sees active is set, the two other variables have also been
  // initialized. The second one ensures that the signal handler sees that
  // active is set. If we only had the second one, it could happen that the
  // signal handler is triggered by an external signal (not a signal that we
  // caused) before the other fields are initialized and this would cause
  // undefined behavior.
  std::atomic_signal_fence(std::memory_order_release);
  threadLocalIoInfo.active = true;
  std::atomic_signal_fence(std::memory_order_release);
  // We also add a fence with acquire semantics so that the access to the
  // hardware does not get reordered before setting the flag. For write
  // operations, this would not be necessary because writes after a release
  // fence may not be reordered before writes that happened before that
  // release fences, but reads could be reordered. In any case, this extra
  // fence should not hurt us.
  std::atomic_signal_fence(std::memory_order_acquire);
}

inline static void finishIo() noexcept {
  // Doing both a release and acquire before resetting the active flag should
  // ensure that the hardware access never gets reordered after resetting the
  // flag.
  std::atomic_signal_fence(std::memory_order_release);
  std::atomic_signal_fence(std::memory_order_acquire);
  threadLocalIoInfo.active = false;
  // Having a release memory fence after resetting the active flag should
  // ensure that future invocations of the signal handler see that the active
  // flag is not set.
  std::atomic_signal_fence(std::memory_order_release);
}

inline static bool ioReadUInt16(void *targetAddress, std::uint16_t &value)
    noexcept {
  // If sigsetjmp returns a non-zero value, siglongjmp was called by the signal
  // handler which means that an error occurred.
  if (::sigsetjmp(threadLocalIoInfo.jumpBuffer, 1)) {
    finishIo();
    return false;
  }
  prepareIo(targetAddress);
  // The MRF devices use big endian internally, so we have to convert when we
  // are running on a little endian system. htonl, htons, ntohl, and ntohs can
  // be preprocessor macros, so we cannot qualify them explicitly with "::".
  value = ntohs(*(reinterpret_cast<volatile std::uint16_t *>(targetAddress)));
  finishIo();
  return true;
}

inline static bool ioReadUInt32(void *targetAddress, std::uint32_t &value)
    noexcept {
  // If sigsetjmp returns a non-zero value, siglongjmp was called by the signal
  // handler which means that an error occurred.
  if (::sigsetjmp(threadLocalIoInfo.jumpBuffer, 1)) {
    finishIo();
    return false;
  }
  prepareIo(targetAddress);
  // The MRF devices use big endian internally, so we have to convert when we
  // are running on a little endian system. htonl, htons, ntohl, and ntohs can
  // be preprocessor macros, so we cannot qualify them explicitly with "::".
  value = ntohl(*(reinterpret_cast<volatile std::uint32_t *>(targetAddress)));
  finishIo();
  return true;
}

inline static bool ioReadWriteBackUInt32(void *targetAddress, std::uint32_t &value)
    noexcept {
  // If sigsetjmp returns a non-zero value, siglongjmp was called by the signal
  // handler which means that an error occurred.
  if (::sigsetjmp(threadLocalIoInfo.jumpBuffer, 1)) {
    finishIo();
    return false;
  }
  prepareIo(targetAddress);
  // The MRF devices use big endian internally, so we have to convert when we
  // are running on a little endian system. htonl, htons, ntohl, and ntohs can
  // be preprocessor macros, so we cannot qualify them explicitly with "::".
  value = ntohl(*(reinterpret_cast<volatile std::uint32_t *>(targetAddress)));
  *(reinterpret_cast<volatile std::uint32_t *>(targetAddress)) = htonl(value);
  finishIo();
  return true;
}

inline static bool ioWriteReadUInt16(void *targetAddress, std::uint16_t &value)
    noexcept {
  // If sigsetjmp returns a non-zero value, siglongjmp was called by the signal
  // handler which means that an error occurred.
  if (::sigsetjmp(threadLocalIoInfo.jumpBuffer, 1)) {
    finishIo();
    return false;
  }
  prepareIo(targetAddress);
  // The MRF devices use big endian internally, so we have to convert when we
  // are running on a little endian system. htonl, htons, ntohl, and ntohs can
  // be preprocessor macros, so we cannot qualify them explicitly with "::".
  *(reinterpret_cast<volatile std::uint16_t *>(targetAddress)) = htons(value);
  value = ntohs(*(reinterpret_cast<volatile std::uint16_t *>(targetAddress)));
  finishIo();
  return true;
}

inline static bool ioWriteReadUInt32(void *targetAddress, std::uint32_t &value)
    noexcept {
  // If sigsetjmp returns a non-zero value, siglongjmp was called by the signal
  // handler which means that an error occurred.
  if (::sigsetjmp(threadLocalIoInfo.jumpBuffer, 1)) {
    finishIo();
    return false;
  }
  prepareIo(targetAddress);
  // The MRF devices use big endian internally, so we have to convert when we
  // are running on a little endian system. htonl, htons, ntohl, and ntohs can
  // be preprocessor macros, so we cannot qualify them explicitly with "::".
  *(reinterpret_cast<volatile std::uint32_t *>(targetAddress)) = htonl(value);
  value = ntohl(*(reinterpret_cast<volatile std::uint32_t *>(targetAddress)));
  finishIo();
  return true;
}

void MrfMmapMemoryAccess::runIoThread() {
  // We block the SIGIO signal for this thread. We want to read this signal from
  // our signal file descriptor and so we do not want a signal handler (if there
  // is one) to intercept it.
  {
    ::sigset_t blockedSignalSet;
    sigemptyset(&blockedSignalSet);
    sigaddset(&blockedSignalSet, SIGIO);
    // phtread_sigmask only fails for invalid arguments, so we do not have to
    // check the return code.
    ::pthread_sigmask(SIG_BLOCK, &blockedSignalSet, nullptr);
  }
  // A read operation for a signal info could be split over more than one
  // iteration, so we have to store this information outside the loop. The same
  // applies to the file descriptors and the memory pointer.
  ::signalfd_siginfo signalInfo;
  std::size_t signalInfoBytesRead = 0;
  int signalFd = -1;
  int deviceFd = -1;
  void *deviceMemory = nullptr;
  // We do not check the shutdown flag in the loop condition because we have to
  // acquire the mutex when checking the flag.
  while (true) {
    std::string deviceErrorDetails;
    // We create a signal file-descriptor (if we do not have one already) so
    // that we can wait for a signal using select(...).
    if (signalFd == -1) {
      ::sigset_t acceptedSignalSet;
      sigemptyset(&acceptedSignalSet);
      sigaddset(&acceptedSignalSet, SIGIO);
      signalFd = ::signalfd(-1, &acceptedSignalSet, SFD_NONBLOCK | SFD_CLOEXEC);
      if (signalFd == -1) {
        // This kind of error is not directly related to a request, but this is
        // the only reasonable way how we can communicate the error to the user.
        deviceErrorDetails = std::string(
          "signalfd(-1, { SIGIO }, SDF_NON_BLOCK | SFD_CLOEXEC) failed: ")
          + std::generic_category().message(errno);
      }
    }
    // If we have not opened the device yet, we try to do this now. We do this
    // before getting the request from the queue because this way we can avoid
    // an unnecessary delay when processing the first request.
    if (deviceMemory == nullptr && signalFd != -1) {
      deviceFd = ::open(devicePath.c_str(), O_RDWR);
      if (deviceFd == -1) {
        deviceErrorDetails =
          std::string("Could not open device ")
          + devicePath
          + ": "
          + std::generic_category().message(errno);
      } else {
        deviceMemory = ::mmap(0, memorySize, PROT_READ | PROT_WRITE, MAP_SHARED,
            deviceFd, 0);
        if (deviceMemory != MAP_FAILED) {
          try {
            prepareInterrupt(deviceFd);
            enableInterrupt(deviceFd);
          } catch (std::exception &e) {
            ::munmap(deviceMemory, memorySize);
            deviceMemory = nullptr;
            ::close(deviceFd);
            deviceFd = -1;
            deviceErrorDetails = std::string("Could not prepare device ")
                + devicePath + " for generating interrupts: " + e.what();
          }
        } else {
          deviceErrorDetails =
            std::string("Could not mmap device ")
            + devicePath
            + ": "
            + std::generic_category().message(errno);
          deviceMemory = nullptr;
          ::close(deviceFd);
          deviceFd = -1;
        }
      }
    }
    bool haveInterrupt = false;
    if (signalFd != -1) {
      ::ssize_t bytesRead = ::read(signalFd,
          reinterpret_cast<void *>(reinterpret_cast<char *>(&signalInfo)
              + signalInfoBytesRead), sizeof(signalInfo) - signalInfoBytesRead);
      if (bytesRead == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
          // This is most likely a permanent error. In any case, the position of
          // the file descriptor is undefined and so we cannot use it any
          // longer. This kind of error is not directly related to a request,
          // but this is the only reasonable way how we can communicate the
          // error to the user.
          deviceErrorDetails =
            std::string("read(...) failed for signal file-descriptor: ")
            + std::generic_category().message(errno);
          // We close the signal file-descriptor so that it is created again at
          // next iteration.
          ::close(signalFd);
          signalFd = -1;
        }
      } else {
        signalInfoBytesRead += bytesRead;
        if (signalInfoBytesRead == sizeof(signalInfo)) {
          signalInfoBytesRead = 0;
          if (signalInfo.ssi_signo == SIGIO) {
            // We have an interrupt if the device FD matches and when we have an
            // open device. If we do not have an open device, we cannot handle
            // the signal, but we still have to consume it so that select(...)
            // can actually sleep.
            haveInterrupt = (signalInfo.ssi_fd == deviceFd)
                && (deviceMemory != nullptr);
          }
        }
      }
    }
    MrfIoRequest request;
    bool haveRequest = false;
    // If we have an interrupt, we handle this interrupt before trying to get
    // the next request. However, we still check the shutdown flag so that the
    // loop quits even if interrupts happen very frequently.
    if (haveInterrupt) {
      // We have to hold the mutex while accessing the shutdown flag.
      std::unique_lock<std::mutex> lock(mutex);
      // If the device is being shutdown, we exit the loop.
      if (shutdown) {
        break;
      }
    } else {
      // We have to hold the mutex while accessing the queue and the shutdown
      // flag.
      std::unique_lock<std::mutex> lock(mutex);
      // If the device is being shutdown, we exit the loop.
      if (shutdown) {
        break;
      }
      // If the queue is empty, we later wait for an element to be queued and
      // then try again. The thread might wake up spuriously, so we cannot
      // expect that the queue will always have an element when we wake up.
      if (!ioQueue.empty()) {
        request = std::move(ioQueue.front());
        haveRequest = true;
        ioQueue.pop_front();
      } else {
        haveRequest = false;
      }
    }
    // We do not need the mutex for the rest of the operations and in fact we
    // should not hold it because we might sleep when calling select(...).
    bool ioSuccessful = true;
    if (haveRequest) {
      // If we could not open and mmap the device sucessfully, we have to report
      // an error.
      if (deviceMemory == nullptr) {
        request.fail(ErrorCode::unknown, deviceErrorDetails);
        continue;
      }
      void *targetAddress =
          reinterpret_cast<void *>(reinterpret_cast<char*>(deviceMemory)
              + request.address);
      switch (request.type) {
      case MrfIoRequestType::notSpecified:
        // This should never happen. We cannot fail the request because most
        // likely it has not been initialized with a callback. We throw an
        // exception which should quit the program forcibly.
        throw std::logic_error(
            "The I/O request queue contained an uninitialized request.");
      case MrfIoRequestType::readUInt16:
        ioSuccessful = ioReadUInt16(targetAddress, request.value16);
        break;
      case MrfIoRequestType::writeUInt16:
        ioSuccessful = ioWriteReadUInt16(targetAddress, request.value16);
        break;
      case MrfIoRequestType::readUInt32:
        ioSuccessful = ioReadUInt32(targetAddress, request.value32);
        break;
      case MrfIoRequestType::writeUInt32:
        ioSuccessful = ioWriteReadUInt32(targetAddress, request.value32);
        break;
      }
      // We have to notify the callback of the result of the operation.
      if (ioSuccessful) {
        switch (request.type) {
        case MrfIoRequestType::notSpecified:
          // Now we can throw an exception because the state has been restored
          // correctly.
          throw std::logic_error(
              "MrfIoRequest::fail has been called on an uninitialized object.");
        case MrfIoRequestType::readUInt16:
        case MrfIoRequestType::writeUInt16:
          try {
            if (request.callback16) {
              request.callback16->success(request.address, request.value16);
            }
          } catch (...) {
            // We do not want an exception in a callback to bubble up into the calling
            // code.
          }
          break;
        case MrfIoRequestType::readUInt32:
        case MrfIoRequestType::writeUInt32:
          try {
            if (request.callback32) {
              request.callback32->success(request.address, request.value32);
            }
          } catch (...) {
            // We do not want an exception in a callback to bubble up into the calling
            // code.
          }
          break;
        }
      } else {
        request.fail(ErrorCode::unknown,
            std::string("Received a SIGBUS while trying to access the device ")
                + devicePath + ". This indicates an I/O error.");
      }
    } else if (haveInterrupt) {
      // If we cannot access the hardware, there is no way how we can handle the
      // interrupt, so we simply ignore it. Interrupts will be enabled again
      // when we can successfully open the device.
      if (deviceMemory == nullptr) {
        continue;
      }
      // The interrupt flag register is stored at address 0x08.
      void *interruptFlagRegisterAddress =
          reinterpret_cast<void *>(reinterpret_cast<char*>(deviceMemory) + 0x08);
      // The interrupt enable register is stored at address 0x0c.
      void *interruptEnableRegisterAddress =
          reinterpret_cast<void *>(reinterpret_cast<char*>(deviceMemory) + 0x0c);
      // Interrupt flags are reset by writing one to them. We simply write the
      // value that we just read. This way, there is no risk of missing an
      // interrupt that occurs between reading and writing.
      std::uint32_t interruptFlagRegister;
      std::uint32_t interruptEnableRegister;
      // We get the value of the interrupt enable register, save the value of
      // the interrupt flag register, and reset the flags so that we can
      // re-enable interrupts without getting an interrupt right again.
      ioSuccessful = ioReadUInt32(interruptEnableRegisterAddress,
          interruptEnableRegister)
          && ioReadWriteBackUInt32(interruptFlagRegisterAddress,
              interruptFlagRegister);
      if (ioSuccessful) {
        // We only want to use those bits of the interrupt flag register for
        // which interrupts are actually enabled. The might be other bits in the
        // interrupt flag register, but those cannot have triggered the
        // interrupt.
        // Luckily, the interrupt enable register has the same layout as the
        // interrupt flag register (apart from a few extra bits for globally
        // enabling interrupts). For this reason, we can simply mask the
        // interrupt flags with the interrupt enabled bits.
        interruptFlagRegister &= interruptEnableRegister;
        // An interrupt might be triggered spuriously. For this reason, we only
        // call the interrupt listeners when the interrupt flag register has at
        // least one interrupt flag set.
        if (interruptFlagRegister != 0) {
          // Notify the interrupt listeners.
          std::vector<std::shared_ptr<InterruptListener>> foundListeners;
          {
            // We have to hold the mutex while accessing the list of listeners.
            std::lock_guard<std::mutex> lock(mutex);
            for (auto listenerIterator = interruptListeners.begin();
                listenerIterator != interruptListeners.end();) {
              std::shared_ptr<InterruptListener> foundListener =
                  listenerIterator->lock();
              if (!foundListener) {
                listenerIterator = interruptListeners.erase(listenerIterator);
              } else {
                foundListeners.push_back(std::move(foundListener));
                ++listenerIterator;
              }
            }
          }
          // We notify the listeners after releasing the mutex. This ensures that a
          // listener cannot cause a dead lock and also means that we do not need a
          // recursive mutex.
          for (auto listenerIterator = foundListeners.begin();
              listenerIterator != foundListeners.end(); ++listenerIterator) {
            try {
              (**listenerIterator)(interruptFlagRegister);
            } catch (...) {
              // We do not want an exception caused by a listener to bubble up into
              // the calling code.
            }
          }
        }
        // After handling an interrupt we have to reenable interrupts by using the
        // respective ioctl() call.
        try {
          enableInterrupt(deviceFd);
        } catch (...) {
          // If we cannot re-enable interrupts our best option is to close the
          // device and hope that it will work the next time.
          ioSuccessful = false;
        }
      }
    } else {
      // If we neither have an interrupt nor a request, we sleep waiting for an
      // interrupt to occur or a request to be queued.
      try {
        ::fd_set readFds;
        FD_ZERO(&readFds);
        if (signalFd != -1) {
          FD_SET(signalFd, &readFds);
        }
        // We wait for a limited amount of time so that we will try reopening
        // the device if it is not open, even when there are no I/O requests.
        // This makes sense because even if there are no I/O requests, we might
        // be interested in interrupts.
        struct ::timeval waitTime;
        waitTime.tv_sec = 5;
        waitTime.tv_usec = 0;
        ioThreadFdSelector.select(&readFds, nullptr, nullptr, signalFd,
            &waitTime);
        // After waking up, the event will be handled in the next iteration.
      } catch (std::system_error &e) {
        if (e.code().value() == EINTR) {
          // If the select call failed because it was interrupted by a signal,
          // our best option is to simply try again.
        } else {
          // If the select call failed because of a different reason, trying
          // again does not make much sense because the same error will most
          // likely happen again. The best we can do is closing all file
          // descriptors and hoping that the error will not happen again after
          // reopening the files.
          ioSuccessful = false;
          // In addition to closing the files, we add a small delay. This
          // ensures that we will not occupy a full CPU core when the error
          // keeps happening again and again.
          struct ::timespec sleepTime;
          sleepTime.tv_sec = 0;
          sleepTime.tv_nsec = 100000000;
          ::nanosleep(&sleepTime, nullptr);
        }
      } catch (...) {
        // When the exception is not of type std::system_error, we cannot know
        // the reason for the error, so we have to expect the worst.
        ioSuccessful = false;
        // In addition to closing the files, we add a small delay. This
        // ensures that we will not occupy a full CPU core when the error
        // keeps happening again and again.
        struct ::timespec sleepTime;
        sleepTime.tv_sec = 0;
        sleepTime.tv_nsec = 100000000;
        ::nanosleep(&sleepTime, nullptr);
      }
    }
    if (!ioSuccessful) {
      // We close the device so that we get a chance to reopen it for the next
      // request when it was temporarily removed.
      if (deviceMemory != nullptr) {
        ::munmap(deviceMemory, memorySize);
        deviceMemory = nullptr;
      }
      if (deviceFd != -1) {
        ::close(deviceFd);
        deviceFd = -1;
      }
    }
  }

  // We want to close the connection to the device when we do not use it any
  // longer. We do not check the status of the munmap(...) and close(...)
  // operations because there is no reasonable way how we could handle an error.
  if (deviceMemory != nullptr) {
    ::munmap(deviceMemory, memorySize);
    deviceMemory = nullptr;
  }
  if (deviceFd != -1) {
    ::close(deviceFd);
    deviceFd = -1;
  }
  if (signalFd != -1) {
    ::close(signalFd);
    signalFd = -1;
  }
  // When we are here, we can access the queue without acquiring the mutex
  // because no requests are added after setting the shutdown flag and this is
  // the only thread that processes the queue.
  for (MrfIoRequest &request : ioQueue) {
    request.fail(ErrorCode::unknown,
        "The device has been shutdown before the request could be processed.");
  }
}

} // namespace mrf
} // namespace anka
