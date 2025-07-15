#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <epicsVersion.h>
#include <iocsh.h>

#include <MrfMemoryAccess.h>

#include "MrfDeviceRegistry.h"
#include "mrfEpicsError.h"

#include "mrfIocshWriteUInt32.h"

using namespace anka::mrf;
using namespace anka::mrf::epics;

extern "C" {

static int mrfIocshFuncInternal(const iocshArgBuf *args) noexcept {
  char *deviceId = args[0].sval;
  std::uint32_t address = static_cast<std::uint32_t>(args[1].ival);
  std::uint32_t value = static_cast<std::uint32_t>(args[2].ival);
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
    value = device->writeUInt32(address, value);
    std::printf(
      "0x%08" PRIx32 ": 0x%08" PRIx32 " (decimal value: %" PRIu32 ")\n",
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
  "mrfWriteUInt32",
  3,
  mrfIocshArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Write to a uint32 register in the MRF device memory.\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};

} // extern "C"

namespace anka {
namespace mrf {
namespace epics {

void registerIocshMrfWriteUInt32() {
  ::iocshRegister(&mrfIocshFuncDef, mrfIocshFunc);
}

} // namespace epics
} // namespace mrf
} // namespace anka
