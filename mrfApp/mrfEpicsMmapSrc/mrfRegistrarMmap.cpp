/*
 * Copyright 2016-2021 aquenos GmbH.
 * Copyright 2016-2021 Karlsruhe Institute of Technology.
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

/*
 * This file contains the functions and data structures needed for initializing
 * a device that is accessed through a device node using mmap(...). These
 * declarations and definitions are placed in a separate because this part of
 * the device support does not work on all platforms.
 */

#include <cstring>
#include <string>

#include <epicsExport.h>
#include <epicsVersion.h>
#include <iocsh.h>

#include <MrfConsistentAsynchronousMemoryAccess.h>
#include <MrfDeviceRegistry.h>
#include <MrfMmapMemoryAccess.h>
#include <mrfEpicsError.h>

using namespace anka::mrf;
using namespace anka::mrf::epics;

extern "C" {

// Data structures shared by all iocsh mrfMmapXxxDevice functions.
static const iocshArg iocshMrfMmapDeviceArg0 = { "device ID", iocshArgString };
static const iocshArg iocshMrfMmapDeviceArg1 = { "device path", iocshArgString };
static const iocshArg * const iocshMrfMmapDeviceArgs[] = {
    &iocshMrfMmapDeviceArg0, &iocshMrfMmapDeviceArg1 };

// The size of the memory area depends on the device type, so that we need a
// separate function for each device type. Technically speaking, there are only
// three different sizes at the moment, but having a separate function for each
// device allows for changes in the future without having to change iocsh
// scripts.
static const iocshFuncDef iocshMrfMmapCpciEvg220DeviceFuncDef = {
  "mrfMmapCpciEvg220Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a cPCI-EVG-220 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/ega3, /dev/egb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapCpciEvg230DeviceFuncDef = {
  "mrfMmapCpciEvg230Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a cPCI-EVG-230 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/ega3, /dev/egb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapCpciEvg300DeviceFuncDef = {
  "mrfMmapCpciEvg300Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a cPCI-EVG-300 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/ega3, /dev/egb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapPxieEvg300DeviceFuncDef = {
  "mrfMmapPxieEvg300Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a PXIe-EVG-300 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/ega3, /dev/egb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapCpciEvr220DeviceFuncDef = {
  "mrfMmapCpciEvr220Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a cPCI-EVR-220 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapCpciEvr230DeviceFuncDef = {
  "mrfMmapCpciEvr230Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a cPCI-EVR-230 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapCpciEvr300DeviceFuncDef = {
  "mrfMmapCpciEvr300Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a cPCI-EVR-300 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapCpciEvrtg300DeviceFuncDef = {
  "mrfMmapCpciEvrtg300Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a cPCI-EVRTG-300 using the MRF kernel device driver."
  "\n\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapMtcaEvr300DeviceFuncDef = {
  "mrfMmapMtcaEvr300Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a mTCA-EVR-300 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapPcieEvr300DeviceFuncDef = {
  "mrfMmapPcieEvr300Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a PCIe-EVR-300 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapPmcEvr230DeviceFuncDef = {
  "mrfMmapPmcEvr230Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a PMC-EVR-230 using the MRF kernel device driver.\n\n"
  "The device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};
static const iocshFuncDef iocshMrfMmapPxieEvr300DeviceFuncDef = {
  "mrfMmapPxieEvr300Device",
  2,
  iocshMrfMmapDeviceArgs,
#ifdef IOCSHFUNCDEF_HAS_USAGE
  "Define a connection to a PXIe-EVR-300 using the MRF kernel device driver.\n"
  "\nThe device path is the path to the device node providing access to the "
  "device\nregisters (e.g. /dev/era3, /dev/erb3, etc.).\n",
#endif // IOCSHFUNCDEF_HAS_USAGE
};

/**
 * Implementation that is shared by all the the iocsh mrfMmapXxxDevice
 * functions.
 */
static int iocshMrfMmapDeviceFunc(const iocshArgBuf *args,
    std::uint32_t memorySize) noexcept {
  char *deviceId = args[0].sval;
  char *devicePath = args[1].sval;
  // Verify and convert the parameters.
  if (!deviceId) {
    errorPrintf("Could not create device: Device ID must be specified.");
    return 1;
  }
  if (!std::strlen(deviceId)) {
    errorPrintf("Could not create device: Device ID must not be empty.");
    return 1;
  }
  if (!devicePath) {
    errorPrintf("Could not create device: Device path must be specified.");
    return 1;
  }
  if (!std::strlen(devicePath)) {
    errorPrintf("Could not create device: Device path must not be empty.");
    return 1;
  }
  // Until here our code does not throw. We put the rest of the function into a
  // try-catch statement, so that we handle all other exceptions.
  try {
    std::shared_ptr<MrfMmapMemoryAccess> rawDevice = std::make_shared<
        MrfMmapMemoryAccess>(std::string(devicePath), memorySize);
    std::shared_ptr<MrfConsistentAsynchronousMemoryAccess> consistentDevice =
        std::make_shared<MrfConsistentAsynchronousMemoryAccess>(rawDevice);
    MrfDeviceRegistry::getInstance().registerDevice(std::string(deviceId),
        consistentDevice);
  } catch (std::exception &e) {
    anka::mrf::epics::errorPrintf("Could not create device %s: %s", deviceId,
        e.what());
    return 1;
  } catch (...) {
    anka::mrf::epics::errorPrintf("Could not create device %s: Unknown error.",
        deviceId);
    return 1;
  }
  return 0;
}

/**
 * Implementation of the iocsh function used for most EVG devices
 * (regular memory size).
 */
static void iocshMrfMmapRegularEvgDeviceFunc(const iocshArgBuf *args) noexcept {
#if EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshSetError(iocshMrfMmapDeviceFunc(args, 0x00010000));
#else // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshMrfMmapDeviceFunc(args, 0x00010000);
#endif // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
}

/**
 * Implementation of the iocsh function used for most EVR devices
 * (regular memory size).
 */
static void iocshMrfMmapRegularEvrDeviceFunc(const iocshArgBuf *args) noexcept {
#if EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshSetError(iocshMrfMmapDeviceFunc(args, 0x00008400));
#else // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshMrfMmapDeviceFunc(args, 0x00008400);
#endif // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
}

/**
 * Implementation of the iocsh function used for EVRs with GTX outputs
 * (cPCI-EVRTG-300, MTCA-EVR-300). This maps a larger address space because the
 * pattern memory for the GTX outputs is added at the end. We always map the
 * address space for up to eight GTX outputs, because mapping for devices with
 * fewer GTX outputs, mapping the memory does not really hurt (it only consumes
 * a few more kilobytes of RAM than absolutely necessary).
 */
static void iocshMrfMmapEvrWithGtxDeviceFunc(const iocshArgBuf *args)
    noexcept {
#if EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshSetError(iocshMrfMmapDeviceFunc(args, 0x00040000));
#else // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
  iocshMrfMmapDeviceFunc(args, 0x00040000);
#endif // EPICS_VERSION_INT >= VERSION_INT(7,0,3,1)
}

/*
 * Registrar that registers the iocsh commands.
 */
static void mrfRegistrarMmap() {
  iocshRegister(&iocshMrfMmapCpciEvg220DeviceFuncDef,
      iocshMrfMmapRegularEvgDeviceFunc);
  iocshRegister(&iocshMrfMmapCpciEvg230DeviceFuncDef,
      iocshMrfMmapRegularEvgDeviceFunc);
  iocshRegister(&iocshMrfMmapCpciEvg300DeviceFuncDef,
      iocshMrfMmapRegularEvgDeviceFunc);
  iocshRegister(&iocshMrfMmapPxieEvg300DeviceFuncDef,
      iocshMrfMmapRegularEvgDeviceFunc);
  iocshRegister(&iocshMrfMmapCpciEvr220DeviceFuncDef,
      iocshMrfMmapRegularEvrDeviceFunc);
  iocshRegister(&iocshMrfMmapCpciEvr230DeviceFuncDef,
      iocshMrfMmapRegularEvrDeviceFunc);
  iocshRegister(&iocshMrfMmapCpciEvr300DeviceFuncDef,
      iocshMrfMmapRegularEvrDeviceFunc);
  iocshRegister(&iocshMrfMmapCpciEvrtg300DeviceFuncDef,
      iocshMrfMmapEvrWithGtxDeviceFunc);
  iocshRegister(&iocshMrfMmapMtcaEvr300DeviceFuncDef,
      iocshMrfMmapEvrWithGtxDeviceFunc);
  iocshRegister(&iocshMrfMmapPcieEvr300DeviceFuncDef,
      iocshMrfMmapRegularEvrDeviceFunc);
  iocshRegister(&iocshMrfMmapPmcEvr230DeviceFuncDef,
      iocshMrfMmapRegularEvrDeviceFunc);
  iocshRegister(&iocshMrfMmapPxieEvr300DeviceFuncDef,
      iocshMrfMmapRegularEvrDeviceFunc);
  // We have to register the SIGBUS signal handler that is used to catch I/O
  // errors that can happen when accessing devices. We do this here, because the
  // chances that this code is called before creating any threads are quite
  // good.
  MrfMmapMemoryAccess::registerSignalHandler();
}

epicsExportRegistrar(mrfRegistrarMmap);

}
