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

#include "MrfConsistentAsynchronousMemoryAccess.h"

namespace anka {
namespace mrf {

MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::Impl(
    MrfMemoryAccess &delegate) :
    delegate(delegate) {
}

MrfConsistentAsynchronousMemoryAccess::MrfConsistentAsynchronousMemoryAccess::Impl::Impl(
    std::shared_ptr<MrfMemoryAccess> delegate) :
    delegate(*delegate), delegatePtr(delegate) {
}

void MrfConsistentAsynchronousMemoryAccess::Impl::writeUInt16(
  std::uint32_t address,
  std::uint16_t value,
  std::shared_ptr<CallbackUInt16> callback,
  ReadbackMode readbackMode
) {
  bool canRun;
  OperationInfo info;
  info.type = OperationType::writeUInt16;
  info.address = address;
  // We have to hold the mutex while operating on the internal data structures.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    info.id = nextId;
    ++nextId;
    std::shared_ptr<WriteCallback<std::uint16_t>> wrappingCallback =
        std::make_shared<WriteCallback<std::uint16_t>>();
    wrappingCallback->operationInfo = info;
    wrappingCallback->impl = shared_from_this();
    wrappingCallback->delegate = callback;
    writeUInt16Args.insert(
      std::make_pair(
        info.id,
        std::make_tuple(wrappingCallback, value, readbackMode)
      )
    );
    canRun = canRunOperation(info);
    if (canRun) {
      markRunOperation(info);
    } else {
      insertOperationInfo(info);
    }
  }
  // We do not want to hold the mutex when processing the operations because we
  // want to avoid possible dead locks.
  if (canRun) {
    runOperation(info);
  }
}

void MrfConsistentAsynchronousMemoryAccess::Impl::writeUInt32(
  std::uint32_t address,
  std::uint32_t value,
  std::shared_ptr<CallbackUInt32> callback,
  ReadbackMode readbackMode
) {
  bool canRun;
  OperationInfo info;
  info.type = OperationType::writeUInt32;
  info.address = address;
  // We have to hold the mutex while operating on the internal data structures.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    info.id = nextId;
    ++nextId;
    std::shared_ptr<WriteCallback<std::uint32_t>> wrappingCallback =
        std::make_shared<WriteCallback<std::uint32_t>>();
    wrappingCallback->operationInfo = info;
    wrappingCallback->impl = shared_from_this();
    wrappingCallback->delegate = callback;
    writeUInt32Args.insert(
      std::make_pair(
        info.id, std::make_tuple(wrappingCallback, value, readbackMode)
      )
    );
    canRun = canRunOperation(info);
    if (canRun) {
      markRunOperation(info);
    } else {
      insertOperationInfo(info);
    }
  }
  // We do not want to hold the mutex when processing the operations because we
  // want to avoid possible dead locks.
  if (canRun) {
    runOperation(info);
  }
}

void MrfConsistentAsynchronousMemoryAccess::Impl::updateUInt16(
    std::uint32_t address, std::shared_ptr<UpdatingCallbackUInt16> callback) {
  bool canRun;
  OperationInfo info;
  info.type = OperationType::updateUInt16;
  info.address = address;
  // We have to hold the mutex while operating on the internal data structures.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    info.id = nextId;
    ++nextId;
    std::shared_ptr<UpdateCallback<std::uint16_t>> wrappingCallback =
        std::make_shared<UpdateCallback<std::uint16_t>>();
    wrappingCallback->operationInfo = info;
    wrappingCallback->impl = shared_from_this();
    wrappingCallback->delegate = callback;
    updateUInt16Callbacks.insert(std::make_pair(info.id, wrappingCallback));
    canRun = canRunOperation(info);
    if (canRun) {
      markRunOperation(info);
    } else {
      insertOperationInfo(info);
    }
  }
  // We do not want to hold the mutex when processing the operations because we
  // want to avoid possible dead locks.
  if (canRun) {
    runOperation(info);
  }
}

void MrfConsistentAsynchronousMemoryAccess::Impl::updateUInt32(
    std::uint32_t address, std::shared_ptr<UpdatingCallbackUInt32> callback) {
  bool canRun;
  OperationInfo info;
  info.type = OperationType::updateUInt32;
  info.address = address;
  // We have to hold the mutex while operating on the internal data structures.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    info.id = nextId;
    ++nextId;
    std::shared_ptr<UpdateCallback<std::uint32_t>> wrappingCallback =
        std::make_shared<UpdateCallback<std::uint32_t>>();
    wrappingCallback->operationInfo = info;
    wrappingCallback->impl = shared_from_this();
    wrappingCallback->delegate = callback;
    updateUInt32Callbacks.insert(std::make_pair(info.id, wrappingCallback));
    canRun = canRunOperation(info);
    if (canRun) {
      markRunOperation(info);
    } else {
      insertOperationInfo(info);
    }
  }
  // We do not want to hold the mutex when processing the operations because we
  // want to avoid possible dead locks.
  if (canRun) {
    runOperation(info);
  }
}

void MrfConsistentAsynchronousMemoryAccess::Impl::insertOperationInfo(
    const OperationInfo &operationInfo) {
  std::uint32_t address = operationInfo.address;
  std::uint32_t width = operationInfo.width();
  for (std::uint32_t byteIndex = 0; byteIndex < width; ++byteIndex) {
    pendingOperations.insert(
        std::make_pair(address + byteIndex, operationInfo));
  }
}

void MrfConsistentAsynchronousMemoryAccess::Impl::removeOperationInfo(
    const OperationInfo &operationInfo) {
  std::uint32_t address = operationInfo.address;
  std::uint32_t width = operationInfo.width();
  for (std::uint32_t byteIndex = 0; byteIndex < width; ++byteIndex) {
    auto range = pendingOperations.equal_range(address + byteIndex);
    for (auto iterator = range.first; iterator != range.second; iterator++) {
      if (iterator->second.id == operationInfo.id) {
        pendingOperations.erase(iterator);
        // We have found the operation info that we were looking for, so there
        // is no need to keep looking at the current position.
        break;
      }
    }
  }
}

std::forward_list<MrfConsistentAsynchronousMemoryAccess::Impl::OperationInfo> MrfConsistentAsynchronousMemoryAccess::Impl::prepareNextOperations(
    const OperationInfo &operationInfo) {
  std::forward_list<OperationInfo> runnableOperations;
  std::uint32_t address = operationInfo.address;
  std::uint32_t width = operationInfo.width();
  for (std::uint32_t byteIndex = 0; byteIndex < width; ++byteIndex) {
    auto range = pendingOperations.equal_range(address + byteIndex);
    for (auto operationIterator = range.first;
        operationIterator != range.second; ++operationIterator) {
      OperationInfo &pendingOperationInfo = operationIterator->second;
      if (canRunOperation(pendingOperationInfo)) {
        markRunOperation(pendingOperationInfo);
        runnableOperations.push_front(pendingOperationInfo);
        removeOperationInfo(pendingOperationInfo);
        // We can stop looking for pending operations at the current address
        // because we know that the current address is now in use. Due to the
        // remove operation, the operationIterator has become invalid anyway.
        break;
      }
    }
  }
  return runnableOperations;
}

void MrfConsistentAsynchronousMemoryAccess::Impl::runOperation(
    const OperationInfo &operationInfo) {
  switch (operationInfo.type) {
  case OperationType::writeUInt16: {
    std::shared_ptr<CallbackUInt16> callback;
    std::uint16_t value;
    ReadbackMode readbackMode;
    std::tie(callback, value, readbackMode) = (
      writeUInt16Args.at(operationInfo.id)
    );
    // We have to catch exceptions and call the failure callback to make sure
    // that things get cleaned up.
    try {
      delegate.writeUInt16(
        operationInfo.address, value, callback, readbackMode
      );
    } catch (std::exception &e) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The write operation failed: ") + e.what());
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    } catch (...) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The write operation failed."));
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    }
    break;
  }
  case OperationType::writeUInt32: {
    std::shared_ptr<CallbackUInt32> callback;
    std::uint32_t value;
    ReadbackMode readbackMode;
    std::tie(callback, value, readbackMode) = (
      writeUInt32Args.at(operationInfo.id)
    );
    // We have to catch exceptions and call the failure callback to make sure
    // that things get cleaned up.
    try {
      delegate.writeUInt32(
        operationInfo.address, value, callback, readbackMode
      );
    } catch (std::exception &e) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The write operation failed: ") + e.what());
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    } catch (...) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The write operation failed."));
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    }
    break;
  }
  case OperationType::updateUInt16: {
    std::shared_ptr<CallbackUInt16> callback = updateUInt16Callbacks.at(
        operationInfo.id);
    // We have to catch exceptions and call the failure callback to make sure
    // that things get cleaned up.
    try {
      delegate.readUInt16(operationInfo.address, callback);
    } catch (std::exception &e) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The read operation failed: ") + e.what());
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    } catch (...) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The read operation failed."));
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    }
    break;
  }
  case OperationType::updateUInt32: {
    std::shared_ptr<CallbackUInt32> callback = updateUInt32Callbacks.at(
        operationInfo.id);
    // We have to catch exceptions and call the failure callback to make sure
    // that things get cleaned up.
    try {
      delegate.readUInt32(operationInfo.address, callback);
    } catch (std::exception &e) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The read operation failed: ") + e.what());
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    } catch (...) {
      try {
        callback->failure(operationInfo.address, ErrorCode::unknown,
            std::string("The read operation failed."));
      } catch (...) {
        // The callback itself might also throw. We simply ignore such an
        // exception
      }
    }
    break;
  }
  }
}

bool MrfConsistentAsynchronousMemoryAccess::Impl::canRunOperation(
    const OperationInfo &operationInfo) {
  std::uint32_t address = operationInfo.address;
  std::uint32_t width = operationInfo.width();
  for (std::uint32_t byteIndex = 0; byteIndex < width; ++byteIndex) {
    if (operationRunning.count(address + byteIndex)) {
      // There is another operation running that overlaps with the
      // requested operation.
      return false;
    }
  }
  return true;
}

void MrfConsistentAsynchronousMemoryAccess::Impl::markRunOperation(
    const OperationInfo &operationInfo) {
  std::uint32_t address = operationInfo.address;
  std::uint32_t width = operationInfo.width();
  for (std::uint32_t byteIndex = 0; byteIndex < width; ++byteIndex) {
    operationRunning.insert(address + byteIndex);
  }
}

void MrfConsistentAsynchronousMemoryAccess::Impl::unmarkRunOperation(
    const OperationInfo &operationInfo) {
  std::uint32_t address = operationInfo.address;
  std::uint32_t width = operationInfo.width();
  for (std::uint32_t byteIndex = 0; byteIndex < width; ++byteIndex) {
    operationRunning.erase(address + byteIndex);
  }
}

void MrfConsistentAsynchronousMemoryAccess::Impl::operationFinished(
    const OperationInfo &operationInfo) {
  std::forward_list<OperationInfo> runnableOperations;
// We have to hold the mutex while operating on the internal data structures.
  {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    unmarkRunOperation(operationInfo);
    switch (operationInfo.type) {
    case OperationType::writeUInt16:
      writeUInt16Args.erase(operationInfo.id);
      break;
    case OperationType::writeUInt32:
      writeUInt32Args.erase(operationInfo.id);
      break;
    case OperationType::updateUInt16:
      updateUInt16Callbacks.erase(operationInfo.id);
      break;
    case OperationType::updateUInt32:
      updateUInt32Callbacks.erase(operationInfo.id);
      break;
    }
    runnableOperations = prepareNextOperations(operationInfo);
  }
// We do not want to hold the mutex when processing the operations because we
// want to avoid possible dead locks.
  for (auto iterator = runnableOperations.begin();
      iterator != runnableOperations.end(); ++iterator) {
    runOperation(*iterator);
  }
}

}
}
