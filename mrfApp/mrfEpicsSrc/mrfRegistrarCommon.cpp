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


#include <epicsExport.h>

#include "mrfIocshDumpCache.h"
#include "mrfIocshMapInterruptToEvent.h"
#include "mrfIocshReadUInt16.h"
#include "mrfIocshReadUInt32.h"
#include "mrfIocshWriteUInt16.h"
#include "mrfIocshWriteUInt32.h"

using namespace anka::mrf;
using namespace anka::mrf::epics;

extern "C" {

/**
 * Registrar that registers the iocsh commands.
 */
static void mrfRegistrarCommon() {
  registerIocshMrfDumpCache();
  registerIocshMrfMapInterruptToEvent();
  registerIocshMrfReadUInt16();
  registerIocshMrfReadUInt32();
  registerIocshMrfWriteUInt16();
  registerIocshMrfWriteUInt32();
}

epicsExportRegistrar(mrfRegistrarCommon);

} // extern "C"
