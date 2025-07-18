/*
 * Copyright 2015-2025 aquenos GmbH.
 * Copyright 2015-2025 Karlsruhe Institute of Technology.
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

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

extern "C" {
#include <stdio.h>
#include <unistd.h>
} // extern "C"

#include <epicsStdio.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsVersion.h>
#include <errlog.h>

#include "mrfEpicsError.h"

namespace anka {
namespace mrf {
namespace epics {

namespace {

char *bufferOffset(char *buffer, std::size_t offset) noexcept {
  return buffer ? (buffer + offset) : nullptr;
}

std::size_t bufferRemaining(
    std::size_t bufferSize, std::size_t offset) noexcept {
  return (bufferSize && bufferSize > offset) ? (bufferSize - offset) : 0;
}

bool prepareErrorMessage(
    char *buffer,
    std::size_t bufferSize,
    std::size_t *bytesWritten,
    const char *format,
    const char *timeString,
    const char *threadString,
    bool useAnsiEscapes,
    std::va_list varArgs) noexcept {
  std::size_t offset = 0;
  int result;
  if (useAnsiEscapes) {
    // Set format to bold, red.
    result = std::snprintf(
      bufferOffset(buffer, offset),
      bufferRemaining(bufferSize, offset),
      "\x1b[1;31m");
    if (result < 0) {
      return false;
    }
    offset += result;
  }
  if (timeString) {
    result = std::snprintf(
      bufferOffset(buffer, offset),
      bufferRemaining(bufferSize, offset),
      "%s ",
      timeString);
    if (result < 0) {
      return false;
    }
    offset += result;
  }
  if (threadString) {
    result = std::snprintf(
      bufferOffset(buffer, offset),
      bufferRemaining(bufferSize, offset),
      "%s ",
      threadString);
    if (result < 0) {
      return false;
    }
    offset += result;
  }
  // Typically, this function is called multiple times (at least twice), so we
  // cannot consume the variable arguments and have to make a copy instead.
  std::va_list varArgsCopy;
  va_copy(varArgsCopy, varArgs);
  result = std::vsnprintf(
    bufferOffset(buffer, offset),
    bufferRemaining(bufferSize, offset),
    format,
    varArgsCopy);
  va_end(varArgsCopy);
  if (result < 0) {
    return false;
  }
  offset += result;
  if (useAnsiEscapes) {
    // Reset format.
    result = std::snprintf(
      bufferOffset(buffer, offset),
      bufferRemaining(bufferSize, offset),
      "\x1b[0m");
    if (result < 0) {
      return false;
    }
    offset += result;
  }
  result = std::snprintf(
    bufferOffset(buffer, offset),
    bufferRemaining(bufferSize, offset),
    "\n");
  if (result < 0) {
    return false;
  }
  offset += result;
  if (bytesWritten) {
    *bytesWritten = offset;
  }
  return true;
}

void errorPrintInternal(const char *format, const char *timeString,
    const char *threadString, std::va_list varArgs) noexcept {
  // In order to avoid the message being interlaced with the message written by
  // another thread, we first prepare a buffer with the whole message and then
  // write it in a single go. This should significantly reduce the risk of
  // different messages getting mingled.
  //
  // First, we have to determine how long the whole message is going to be. In
  // order to not make the code more complex than it needs to be, we always
  // reserve the size that is needed when including the ANSI escape sequences,
  // even if we are not going to use them in the end.
  std::size_t bufferSize;
  if (!prepareErrorMessage(
      nullptr,
      0,
      &bufferSize,
      format,
      timeString,
      threadString,
      true,
      varArgs)) {
    // This is very unlikely, but we still have to handle it.
    ::errlogPrintf(
      "Error: Could not determine buffer size for error message.\n");
    return;
  }
  // We also want to reserve space for the terminating null byte.
  bufferSize += 1;
  // Now, we can allocate a buffer.
  char *buffer = static_cast<char *>(std::malloc(bufferSize));
  if (!buffer) {
    // It is very unlikely that the allocation fails, but we still have to
    // handle such a case. We could use a more elegant alternative like
    // reverting to writing the message without a dynamically allocated buffer,
    // risking that it might get interlaced with another message. However, an
    // allocation failure looks sufficiently unlikly that it is simply not
    // worth the added complexity in the code.
    ::errlogPrintf("Error: Could not allocate buffer for error message.\n");
    return;
  }
  // Starting with EPICS Base 7.0.7, errlogPrintf automatically removes ANSI
  // sequences when not writing to a terminal, so we can simply use that
  // function. For earlier versions, we handle output to stderr ourselves (only
  // using ANSI escapes when it is a terminal) and pass the message without
  // escapes to errlogPrintfNoConsole.
#if EPICS_VERSION_INT >= VERSION_INT(7,0,7,0)
  if (prepareErrorMessage(
      buffer,
      bufferSize,
      nullptr,
      format,
      timeString,
      threadString,
      true,
      varArgs)) {
    ::errlogPrintf("%s", buffer);
  } else {
    ::errlogPrintf("Error: Could not prepare error message.\n");
  }
#else // EPICS_VERSION_INT >= VERSION_INT(7,0,7,0)
std::FILE *output = ::epicsGetStderr();
bool useAnsiSequences = ::isatty(::fileno(output));
  if (prepareErrorMessage(
      buffer,
      bufferSize,
      nullptr,
      format,
      timeString,
      threadString,
      useAnsiSequences,
      varArgs)) {
    std::fprintf(output, "%s", buffer);
    std::fflush(output);
    // We also write the message to the errlog, but without again writing it to
    // the console and omitting the ANSI escape sequences.
    if (!useAnsiSequences || prepareErrorMessage(
        buffer,
        bufferSize,
        nullptr,
        format,
        timeString,
        threadString,
        false,
        varArgs)) {
      ::errlogPrintfNoConsole("%s", buffer);
    }
  } else {
    ::errlogPrintf("Error: Could not prepare error message.\n");
  }
#endif // EPICS_VERSION_INT >= VERSION_INT(7,0,7,0)
  // Free the dynamically allocated buffer.
  std::free(buffer);
}

} // anonymous namespace

void errorPrintf(const char *format, ...) noexcept {
  std::va_list varArgs;
  va_start(varArgs, format);
  errorPrintInternal(format, nullptr, nullptr, varArgs);
  va_end(varArgs);
}

void errorExtendedPrintf(const char *format, ...) noexcept {
  constexpr int bufferSize = 1024;
  char buffer[bufferSize];
  const char *timeString;
  try {
    ::epicsTime currentTime = ::epicsTime::getCurrent();
    if (currentTime.strftime(buffer, bufferSize, "%Y/%m/%d %H:%M:%S.%06f")) {
      timeString = buffer;
    } else {
      timeString = nullptr;
    }
  } catch (...) {
    timeString = nullptr;
  }
  std::va_list varArgs;
  va_start(varArgs, format);
  errorPrintInternal(format, timeString, ::epicsThreadGetNameSelf(), varArgs);
  va_end(varArgs);
}

}
}
}
