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

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <epicsVersion.h>
#include <iocsh.h>

#include <MrfMemoryAccess.h>

#include "MrfDeviceRegistry.h"
#include "mrfEpicsError.h"

#include "mrfIocshDumpCache.h"

using namespace anka::mrf;
using namespace anka::mrf::epics;

extern "C" {

// Data structures needed for the iocsh mrfDumpCache function.
static const iocshArg iocshMrfDumpCacheArg0 = { "device ID", iocshArgString };
static const iocshArg * const iocshMrfDumpCacheArgs[] = {
  &iocshMrfDumpCacheArg0 };
static const iocshFuncDef iocshMrfDumpCacheFuncDef = {
  "mrfDumpCache",
  1,
  iocshMrfDumpCacheArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Dump the memory cache for a device.\n\n"
  "The memory cache is only used for initializing output records during IOC "
  "startup\nand thus will only contain entries for memory locations referenced "
  "by such\nrecords.\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};

static int iocshMrfDumpCacheFuncInternal(const iocshArgBuf *args) noexcept {
  char *deviceId = args[0].sval;
  // Verify and convert the parameters.
  if (!deviceId) {
    errorPrintf(
        "Device ID must be specified.");
    return 1;
  }
  if (!std::strlen(deviceId)) {
    errorPrintf(
        "Device ID must not be empty.");
    return 1;
  }
  try {
    auto cache = MrfDeviceRegistry::getInstance().getDeviceCache(deviceId);
    if (!cache) {
      errorPrintf("Could not find cache for device with ID \"%s\".", deviceId);
      return 1;
    }
    std::printf("uint16 registers:\n\n");
    for (auto &addressAndValue : cache->getCacheUInt16()) {
      std::printf(
        "0x%08" PRIx32 ": 0x%04" PRIx16 "\n",
        addressAndValue.first,
        addressAndValue.second);
    }
    std::printf("\n\nuint32 registers:\n\n");
    for (auto &addressAndValue : cache->getCacheUInt32()) {
      std::printf(
        "0x%08" PRIx32 ": 0x%08" PRIx32 "\n",
        addressAndValue.first,
        addressAndValue.second);
    }
  } catch (std::exception &e) {
    errorPrintf("Error while accessing device cache: %s", e.what());
    return 1;
  } catch (...) {
    errorPrintf("Error while accessing device cache: Unknown error.");
    return 1;
  }
  return 0;
}

/**
 * Implementation of the iocsh mrfDumpCache function. This function prints the
 * contents of the memory cache for a device. It is mainly intended for
 * diagnostics when developing this device support. In particular, it is used to
 * get a list of addresses for which the cache is used.
 */
static void iocshMrfDumpCacheFunc(const iocshArgBuf *args) noexcept {
#if EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshSetError(iocshMrfDumpCacheFuncInternal(args));
#else // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshMrfDumpCacheFuncInternal(args);
#endif // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
}

} // extern "C"

namespace anka {
namespace mrf {
namespace epics {

void registerIocshMrfDumpCache() {
  ::iocshRegister(&iocshMrfDumpCacheFuncDef, iocshMrfDumpCacheFunc);
}

} // namespace epics
} // namespace mrf
} // namespace anka
