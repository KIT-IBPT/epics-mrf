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

#include <condition_variable>
#include <mutex>
#include <stdexcept>

#include "MrfConsistentMemoryAccess.h"

namespace anka {
namespace mrf {

namespace {

template<typename T>
class UpdatingCallbackImpl: public MrfConsistentMemoryAccess::UpdatingCallback<T> {
private:
  T value;
  T mask;
  std::shared_ptr<MrfMemoryAccess::Callback<T>> notifyCallback;

public:
  UpdatingCallbackImpl(T value, T mask,
      std::shared_ptr<MrfMemoryAccess::Callback<T>> notifyCallback) :
      value(value), mask(mask), notifyCallback(notifyCallback) {
  }

  void success(std::uint32_t address, T value) {
    if (notifyCallback) {
      notifyCallback->success(address, value);
    }
  }

  void failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
      const std::string &details) {
    if (notifyCallback) {
      notifyCallback->failure(address, errorCode, details);
    }
  }

  T update(std::uint32_t, T oldValue) {
    return (oldValue & ~mask) | (value & mask);
  }
};

template<typename T>
class SynchronousCallbackImpl: public MrfMemoryAccess::Callback<T> {
private:
  std::mutex mutex;
  std::condition_variable cv;
  bool finished = false;
  T value;
  bool successful = false;
  std::uint32_t address;
  MrfMemoryAccess::ErrorCode errorCode;
  std::string details;

public:
  SynchronousCallbackImpl() :
      address(0), errorCode(MrfMemoryAccess::ErrorCode::unknown) {
  }

  void success(std::uint32_t, T value) {
    std::unique_lock<std::mutex> lock(mutex);
    this->finished = true;
    this->value = value;
    this->successful = true;
    cv.notify_all();
  }

  void failure(std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
      const std::string &details) {
    std::unique_lock<std::mutex> lock(mutex);
    this->finished = true;
    this->successful = false;
    this->address = address;
    this->errorCode = errorCode;
    this->details = details;
    cv.notify_all();
  }

  T getResult() {
    std::unique_lock<std::mutex> lock(mutex);
    while (!finished) {
      cv.wait(lock);
    }
    if (!successful) {
      throw std::runtime_error(
          std::string("Memory access operation for address ")
              + mrfMemoryAddressToString(address) + " failed: "
              + (details.empty() ? mrfErrorCodeToString(errorCode) : details));
    }
    return value;
  }

};
}

void MrfConsistentMemoryAccess::writeUInt16(std::uint32_t address,
    std::uint16_t value, std::uint16_t mask,
    std::shared_ptr<CallbackUInt16> callback) {
  std::shared_ptr<UpdatingCallbackImpl<std::uint16_t>> internalCallback =
      std::make_shared<UpdatingCallbackImpl<std::uint16_t>>(value, mask,
          callback);
  updateUInt16(address, internalCallback);
}

void MrfConsistentMemoryAccess::writeUInt32(std::uint32_t address,
    std::uint32_t value, std::uint32_t mask,
    std::shared_ptr<CallbackUInt32> callback) {
  std::shared_ptr<UpdatingCallbackImpl<std::uint32_t>> internalCallback =
      std::make_shared<UpdatingCallbackImpl<std::uint32_t>>(value, mask,
          callback);
  updateUInt32(address, internalCallback);
}

std::uint16_t MrfConsistentMemoryAccess::writeUInt16(std::uint32_t address,
    std::uint16_t value, std::uint16_t mask) {
  auto callback = std::make_shared<SynchronousCallbackImpl<std::uint16_t>>();
  writeUInt16(address, value, mask, callback);
  return callback->getResult();
}

std::uint32_t MrfConsistentMemoryAccess::writeUInt32(std::uint32_t address,
    std::uint32_t value, std::uint32_t mask) {
  auto callback = std::make_shared<SynchronousCallbackImpl<std::uint32_t>>();
  writeUInt32(address, value, mask, callback);
  return callback->getResult();
}

}
}
