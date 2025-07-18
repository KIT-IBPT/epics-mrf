/*
 * Copyright 2025 aquenos GmbH.
 * Copyright 2025 Karlsruhe Institute of Technology.
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
#include <cstring>
#include <stdexcept>

#include <epicsStdio.h>
#include <epicsVersion.h>
#include <iocsh.h>

#include <MrfMemoryAccess.h>

#include "MrfDeviceRegistry.h"
#include "mrfEpicsError.h"

#include "mrfIocshWriteUInt16.h"

using namespace anka::mrf;
using namespace anka::mrf::epics;

extern "C" {

static int mrfIocshFuncInternal(const iocshArgBuf *args) noexcept {
  char *deviceId = args[0].sval;
  std::uint32_t address = static_cast<std::uint32_t>(args[1].ival);
  std::uint16_t value = static_cast<std::uint16_t>(args[2].ival);
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
    auto device = MrfDeviceRegistry::getInstance().getDevice(deviceId);
    if (!device) {
      errorPrintf("Could not find device with ID \"%s\".", deviceId);
      return 1;
    }
    value = device->writeUInt16(address, value);
    ::epicsStdoutPrintf(
      "0x%08" PRIx32 ": 0x%04" PRIx16 " (decimal value: %" PRIu16 ")\n",
      address, value, value);
  } catch (std::exception &e) {
    errorPrintf("Error while writing register: %s", e.what());
    return 1;
  } catch (...) {
    errorPrintf("Error while writing register: Unknown error.");
    return 1;
  }
  return 0;
}

static void mrfIocshFunc(const iocshArgBuf *args) noexcept {

#if EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshSetError(mrfIocshFuncInternal(args));
#else // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  mrfIocshFuncInternal(args);
#endif // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
}

// Data structures needed for the iocsh mrfReadUInt32 function.
static const iocshArg mrfIocshArg0 = { "device ID", iocshArgString };
static const iocshArg mrfIocshArg1 = { "memory address", iocshArgInt };
static const iocshArg mrfIocshArg2 = { "value", iocshArgInt };
static const iocshArg * const mrfIocshArgs[] = {
  &mrfIocshArg0, &mrfIocshArg1, &mrfIocshArg2 };
static const iocshFuncDef mrfIocshFuncDef = {
  "mrfWriteUInt16",
  3,
  mrfIocshArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Write to a uint16 register in the MRF device memory.\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};

} // extern "C"

namespace anka {
namespace mrf {
namespace epics {

void registerIocshMrfWriteUInt16() {
  ::iocshRegister(&mrfIocshFuncDef, mrfIocshFunc);
}

} // namespace epics
} // namespace mrf
} // namespace anka
