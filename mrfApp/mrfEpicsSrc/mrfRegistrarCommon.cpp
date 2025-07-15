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

#include <climits>
#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <dbScan.h>
#include <epicsExport.h>
#include <epicsVersion.h>
#include <iocsh.h>

#include <MrfMemoryAccess.h>

#include "MrfDeviceRegistry.h"
#include "mrfEpicsError.h"
#include "mrfIocshReadUInt16.h"
#include "mrfIocshReadUInt32.h"
#include "mrfIocshWriteUInt16.h"
#include "mrfIocshWriteUInt32.h"

using namespace anka::mrf;
using namespace anka::mrf::epics;

// We use an anonymous namespace for the functions and data structures that we
// only use internally. This way, we can avoid accidental name collisions.
namespace {

/**
 * Interrupt listener that posts an EPICS event.
 */
class InterruptListenerImpl: public MrfMemoryAccess::InterruptListener {

public:

  InterruptListenerImpl(int eventNumber, std::uint32_t interruptFlagsMask) :
      eventNumber(eventNumber), interruptFlagsMask(interruptFlagsMask) {
  }

  void operator()(std::uint32_t interruptFlags) {
    interruptFlags &= interruptFlagsMask;
    if (interruptFlags != 0) {
      ::post_event(eventNumber);
    }
  }

private:

  const int eventNumber;
  const std::uint32_t interruptFlagsMask;

};

// We need a vector where we can store the interrupt listeners. If we did not,
// the listeners would be destroyed because the device only keeps weak
// references. As std::vector is not thread safe, we have to protect it with a
// mutex.
std::mutex mutex;
std::vector<std::shared_ptr<InterruptListenerImpl>> interruptListeners;

void mapInterruptToEvent(const std::string &deviceId, int eventNumber,
    std::uint32_t interruptFlagsMask) {
  std::shared_ptr<MrfMemoryAccess> device =
      MrfDeviceRegistry::getInstance().getDevice(deviceId);
  if (!device) {
    throw std::runtime_error(
        std::string("Could not find device ") + deviceId + ".");
  }
  if (!device->supportsInterrupts()) {
    throw std::runtime_error(
        std::string("The device ") + deviceId
            + " does not support interrupts.");
  }
  std::shared_ptr<InterruptListenerImpl> interruptListener = std::make_shared<
      InterruptListenerImpl>(eventNumber, interruptFlagsMask);
  device->addInterruptListener(interruptListener);
  {
    // Access to the vector is protected by the mutex.
    std::lock_guard<std::mutex> lock(mutex);
    interruptListeners.push_back(interruptListener);
  }
}

}

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

// Data structures needed for the iocsh mrfMapInterruptToEvent function.
static const iocshArg iocshMrfMapInterruptToEventArg0 = { "device ID",
    iocshArgString };
static const iocshArg iocshMrfMapInterruptToEventArg1 = { "event number",
    iocshArgInt };
static const iocshArg iocshMrfMapInterruptToEventArg2 = {
    "interrupt flags mask", iocshArgString };
static const iocshArg * const iocshMrfMapInterruptToEventArgs[] = {
    &iocshMrfMapInterruptToEventArg0, &iocshMrfMapInterruptToEventArg1,
    &iocshMrfMapInterruptToEventArg2 };
static const iocshFuncDef iocshMrfMapInterruptToEventFuncDef = {
  "mrfMapInterruptToEvent",
  3,
  iocshMrfMapInterruptToEventArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Map a device interrupt to an EPICS event.\n\n"
  "This only works when the device actually supports interrupts (e.g. not for\n"
  "UDP/IP devices). The event is only triggered when one of the bits that is "
  "set in\nthe mask is also set in the interrupt flags register when the "
  "interrupt happens.\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};

static int iocshMrfMapInterruptToEventFuncInternal(const iocshArgBuf *args) noexcept {
  char *deviceId = args[0].sval;
  int eventNumber = args[1].ival;
  const char *interruptFlagsMaskCString = args[2].sval;
  // Verify and convert the parameters.
  if (!deviceId) {
    errorPrintf(
        "Could not create the event mapping: Device ID must be specified.");
    return 1;
  }
  if (!std::strlen(deviceId)) {
    errorPrintf(
        "Could not create the event mapping: Device ID must not be empty.");
    return 1;
  }
  if (eventNumber < 0) {
    errorPrintf(
        "Could not create the event mapping: The event number must not be negative.");
    return 1;
  }
  // We do the conversion of the interruptFlagsMask ourselves instead of using
  // an int parameter. This way, we can ensure that only unsigned numbers are
  // used.
  std::string interruptFlagsMaskString;
  if (!interruptFlagsMaskCString || !std::strlen(interruptFlagsMaskCString)) {
    interruptFlagsMaskString = std::string("0xffffffff");
  } else {
    interruptFlagsMaskString = std::string(interruptFlagsMaskCString);
  }
  unsigned long interruptFlagsMask;
  // We use the strtoul function for converting. This only works, if the
  // unsigned long data-type is large enough (which it should be on
  // virtually all platforms).
  static_assert(sizeof(unsigned long) >= sizeof(std::uint32_t), "unsigned long data-type is not large enough to hold a uint32_t");
  std::size_t numberLength;
  try {
    interruptFlagsMask = std::stoul(interruptFlagsMaskString, &numberLength, 0);
  } catch (std::invalid_argument&) {
    errorPrintf(
        "Could not create the event mapping: Invalid interrupt flags mask: %s",
        interruptFlagsMaskString.c_str());
    return 1;
  } catch (std::out_of_range&) {
    errorPrintf(
        "Could not create the event mapping: Invalid interrupt flags mask: %s",
        interruptFlagsMaskString.c_str());
    return 1;
  }
  if (numberLength != interruptFlagsMaskString.size()) {
    errorPrintf(
        "Could not create the event mapping: Invalid interrupt flags mask: %s",
        interruptFlagsMaskString.c_str());
    return 1;
  }
  if (interruptFlagsMask > UINT32_MAX || interruptFlagsMask == 0) {
    errorPrintf(
        "Could not create the event mapping: Invalid interrupt flags mask: %s. The event mask must be greater than zero and less than or equal to 0xffffffff.",
        interruptFlagsMaskString.c_str());
    return 1;
  }
  try {
    mapInterruptToEvent(deviceId, eventNumber,
        static_cast<std::uint32_t>(interruptFlagsMask));
  } catch (std::exception &e) {
    errorPrintf("Could not create the event mapping: %s", e.what());
    return 1;
  } catch (...) {
    errorPrintf("Could not create the event mapping: Unknown error.");
    return 1;
  }
  return 0;
}

/**
 * Implementation of the iocsh mrfMapInterruptToEvent function. This function
 * maps the interrupt for a specific device to an EPICS event. The interrupt
 * flag mask defines for which kind of interrupts an event is triggered. An
 * event is only triggered when at least one of the bits in the specified mask
 * is also set in the interrupt flags of the interrupt event.
 */
static void iocshMrfMapInterruptToEventFunc(const iocshArgBuf *args) noexcept {
#if EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshSetError(iocshMrfMapInterruptToEventFuncInternal(args));
#else // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshMrfMapInterruptToEventFuncInternal(args);
#endif // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
}

/**
 * Registrar that registers the iocsh commands.
 */
static void mrfRegistrarCommon() {
  ::iocshRegister(&iocshMrfDumpCacheFuncDef, iocshMrfDumpCacheFunc);
  ::iocshRegister(&iocshMrfMapInterruptToEventFuncDef,
      iocshMrfMapInterruptToEventFunc);
  registerIocshMrfReadUInt16();
  registerIocshMrfReadUInt32();
  registerIocshMrfWriteUInt16();
  registerIocshMrfWriteUInt32();
}

epicsExportRegistrar(mrfRegistrarCommon);

} // extern "C"
