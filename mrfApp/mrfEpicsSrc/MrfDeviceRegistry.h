/*
 * Copyright 2015-2016 aquenos GmbH.
 * Copyright 2015-2016 Karlsruhe Institute of Technology.
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

#ifndef ANKA_MRF_EPICS_DEVICE_REGISTRY_H
#define ANKA_MRF_EPICS_DEVICE_REGISTRY_H

#include <memory>
#include <mutex>
#include <unordered_map>

#include <MrfConsistentMemoryAccess.h>

#include "MrfMemoryCache.h"

namespace anka {
namespace mrf {
namespace epics {

/**
 * Registry holding MRF devices. Devices are registered with the registry
 * during initialization and can then be retrieved for use by different records.
 * This class implements the singleton pattern and the only instance is returned
 * by the {@link #getInstance()} function.
 */
class MrfDeviceRegistry {

public:

  /**
   * Returns the only instance of this class.
   */
  inline static MrfDeviceRegistry &getInstance() {
    return instance;
  }

  /**
   * Returns the device with the specified ID. If no device with the ID has been
   * registered, a pointer to null is returned.
   */
  std::shared_ptr<MrfConsistentMemoryAccess> getDevice(
      const std::string &deviceId);

  /**
   * Returns the cache for the device with the specified ID. If no device with
   * the ID has been registered, a pointer to null is returned.
   */
  std::shared_ptr<MrfMemoryCache> getDeviceCache(const std::string &deviceId);

  /**
   * Registers a device under the specified name. This method can be used to
   * register a device instance that cannot be created by the device registry
   * directly. In particular, it is used for mmap-based devices, because those
   * are not supported on all platforms and thus the registry does not implement
   * a method to create them. Throws an exception if the device cannot be
   * registered because the specified name is already in use.
   */
  void registerDevice(const std::string &deviceId,
      std::shared_ptr<MrfConsistentMemoryAccess> device);

private:

  // We do not want to allow copy or move construction or assignment.
  MrfDeviceRegistry(const MrfDeviceRegistry &) = delete;
  MrfDeviceRegistry(MrfDeviceRegistry &&) = delete;
  MrfDeviceRegistry &operator=(const MrfDeviceRegistry &) = delete;
  MrfDeviceRegistry &operator=(MrfDeviceRegistry &&) = delete;

  static MrfDeviceRegistry instance;

  std::unordered_map<std::string, std::shared_ptr<MrfConsistentMemoryAccess>> devices;
  std::unordered_map<std::string, std::shared_ptr<MrfMemoryCache>> caches;
  std::recursive_mutex mutex;

  MrfDeviceRegistry();

};

}
}
}

#endif // ANKA_MRF_EPICS_DEVICE_REGISTRY_H
