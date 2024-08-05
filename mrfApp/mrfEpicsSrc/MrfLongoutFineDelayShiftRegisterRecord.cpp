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

#include <cstring>
#include <string>
#include <stdexcept>
#include <tuple>

#include <alarm.h>
#include <recGbl.h>

#include "MrfDeviceRegistry.h"

#include "MrfLongoutFineDelayShiftRegisterRecord.h"

namespace anka {
namespace mrf {
namespace epics {

// We use an anonymous namespace for all functions that are local to this
// compilation unit.
namespace {

std::pair<std::size_t, std::size_t> findNextToken(const std::string &str,
    const std::string &delimiters, std::size_t startPos) {
  if (str.length() == 0) {
    return std::make_pair(std::string::npos, 0);
  }
  std::size_t startOfToken = str.find_first_not_of(delimiters, startPos);
  if (startOfToken == std::string::npos) {
    return std::make_pair(std::string::npos, 0);
  }
  std::size_t endOfToken = str.find_first_of(delimiters, startOfToken);
  if (endOfToken == std::string::npos) {
    return std::make_pair(startOfToken, str.length() - startOfToken);
  }
  return std::make_pair(startOfToken, endOfToken - startOfToken);
}

std::tuple<std::uint32_t, signed char> parseMemoryAddress(
    const std::string &addressString) {
  // We use the stoul function for converting. This only works, if the unsigned
  // long data-type is large enough (which it should be on virtually all
  // platforms).
  static_assert(sizeof(unsigned long) >= sizeof(std::uint32_t), "unsigned long data-type is not large enough to hold a uint32_t");
  // We have a 32-bit register and need three bits in addition to the one that
  // is specified in the address. Therefore, the highest index that may be
  // specified is 28.
  constexpr signed char maxBitIndex = 28;
  std::size_t numberLength;
  unsigned long address;
  try {
    address = std::stoul(addressString, &numberLength, 0);
  } catch (std::invalid_argument&) {
    throw std::invalid_argument(
        std::string("Invalid memory address in record address: ")
            + addressString);
  } catch (std::out_of_range&) {
    throw std::invalid_argument(
        std::string("Invalid memory address in record address: ")
            + addressString);
  }
  if (address > UINT32_MAX) {
    throw std::invalid_argument(
        std::string("Invalid memory address in record address: ")
            + addressString);
  }
  if (numberLength == addressString.length()) {
    throw std::invalid_argument(
        std::string(
            "Bit index is missing in memory address in record address: ")
            + addressString);
  }
  if (addressString[numberLength] != '[') {
    throw std::invalid_argument(
        std::string("Invalid memory address in record address: ")
            + addressString + ". Expected '[' but found '"
            + addressString[numberLength] + "'.");
  }
  if (addressString[addressString.length() - 1] != ']') {
    throw std::invalid_argument(
        std::string("Invalid memory address in record address: ")
            + addressString + ". Expected ']' but found '"
            + addressString[addressString.length() - 1] + "'.");
  }
  std::string bitIndexString = addressString.substr(numberLength + 1,
      addressString.length() - numberLength - 2);
  int bitIndex;
  try {
    bitIndex = std::stoi(bitIndexString, &numberLength, 0);
  } catch (std::invalid_argument&) {
    throw std::invalid_argument(
        std::string("Invalid bit index in record address: ") + addressString);
  } catch (std::out_of_range&) {
    throw std::invalid_argument(
        std::string("Invalid bit index in record address: ") + addressString);
  }
  if (numberLength != bitIndexString.length()) {
    throw std::invalid_argument(
        std::string("Invalid bit index in record address: ") + addressString);
  }
  if (bitIndex < 0 || bitIndex > maxBitIndex) {
    throw std::invalid_argument(
        std::string("Invalid bit index in record address: ") + addressString);
  }
  return std::make_tuple(static_cast<std::uint32_t>(address), bitIndex);
}

} // End of anonymous namespace

MrfLongoutFineDelayShiftRegisterRecord::MrfLongoutFineDelayShiftRegisterRecord(
    ::longoutRecord *record) :
    record(record), writeSuccessful(false), writeCallback(
        std::make_shared<CallbackImpl>(*this)) {
  // Parse the record address.
  if (this->record->out.type != INST_IO) {
    throw std::runtime_error(
        "Invalid device address. Maybe mixed up INP/OUT or forgot '@'?");
  }
  std::string addressString(
      this->record->out.value.instio.string ?
          this->record->out.value.instio.string : "");
  const std::string delimiters(" \t\n\v\f\r");
  std::size_t tokenStart, tokenLength;
  // First, read the device name.
  std::tie(tokenStart, tokenLength) = findNextToken(addressString, delimiters,
      0);
  if (tokenStart == std::string::npos) {
    throw std::invalid_argument("Could not find device ID in record address.");
  }
  std::string deviceId(addressString.substr(tokenStart, tokenLength));
  // Next, read the address and bit index for the GPIO direction register.
  std::tie(tokenStart, tokenLength) = findNextToken(addressString, delimiters,
      tokenStart + tokenLength);
  if (tokenStart == std::string::npos) {
    throw std::invalid_argument(
        "Could not find memory address of GPIO direction register in record address.");
  }
  std::tie(this->gpioDirectionRegisterAddress,
      this->gpioDirectionRegisterBitShift) = parseMemoryAddress(
      addressString.substr(tokenStart, tokenLength));
  // Next, read the address and bit index for the GPIO output register.
  std::tie(tokenStart, tokenLength) = findNextToken(addressString, delimiters,
      tokenStart + tokenLength);
  if (tokenStart == std::string::npos) {
    throw std::invalid_argument(
        "Could not find memory address of GPIO output register in record address.");
  }
  std::tie(this->gpioOutputRegisterAddress, this->gpioOutputRegisterBitShift) =
      parseMemoryAddress(addressString.substr(tokenStart, tokenLength));
  // Ensure that there is no more input.
  std::tie(tokenStart, tokenLength) = findNextToken(addressString, delimiters,
      tokenStart + tokenLength);
  if (tokenStart != std::string::npos) {
    throw std::invalid_argument(
        std::string("Unrecognized token in record address: ")
            + addressString.substr(tokenStart, tokenLength));
  }
  // Get the device.
  this->device = MrfDeviceRegistry::getInstance().getDevice(deviceId);
  if (!this->device) {
    throw std::runtime_error(
        std::string("Could not find device ") + deviceId + ".");
  }
  // Prepare the transfer callback.
  // EPICS Base does not provide a function for doing a general initialization,
  // but it expects some private fields to be initialized with zero. We use
  // memset to initialize these fields. This is more compatible than
  // initializing the fields individually (they might change in future versions
  // of EPICS Base).
  std::memset(&this->nextTransferStepCallback, 0, sizeof(this->nextTransferStepCallback));
  callbackSetCallback(startNextTransferStepStatic,
      &this->nextTransferStepCallback);
  callbackSetPriority(priorityHigh, &this->nextTransferStepCallback);
  callbackSetUser(this, &this->nextTransferStepCallback);
}

void MrfLongoutFineDelayShiftRegisterRecord::processRecord() {
  if (this->record->pact) {
    this->record->pact = false;
    // Check for an error condition and finish the processing of the record.
    if (!writeSuccessful) {
      recGblSetSevr(this->record, WRITE_ALARM, INVALID_ALARM);
      throw std::runtime_error(writeErrorMessage);
    } else {
      // The value has been written successfully, thus the record is not
      // undefined any longer.
      this->record->udf = false;
    }
  } else {
    // Start the write process.
    nextTransferStepIndex = 0;
    writeOutputValue = record->val;
    startNextTransferStep();
    this->record->pact = true;
  }
}

void MrfLongoutFineDelayShiftRegisterRecord::scheduleProcessing() {
  // Registering the callback establishes a happens-before relationship due to
  // an internal lock. Therefore, data written before registering the callback
  // is seen by the callback function.
  ::callbackRequestProcessCallback(&this->processCallback, priorityMedium,
      this->record);
}

void MrfLongoutFineDelayShiftRegisterRecord::startNextTransferStep() {
  // We increment the step index before taking any action. This way, the index
  // has been incremented, even if the callback from the action (and thus this
  // function) is executed again before we return from this function.
  int currentTransferStepIndex = nextTransferStepIndex;
  ++nextTransferStepIndex;
  if (currentTransferStepIndex == 0) {
    // First, we have to configure the GPIO pins for output. A pin is configured
    // as an output by setting the direction bit to one. Therefore, the mask and
    // the value are the same. There is a small risk that the corresponding bits
    // in the output register are not zero. This is a problem if the bit for the
    // transfer latch clock is not zero, because an undefined value will be
    // latched into the delay chip. However, the register is initialized with
    // zero on device startup and we reset it to zero after every write
    // operation, so this is very unlikely to happen. Even if it happened, we
    // would overwrite the value immediately, so that the undefined delay value
    // would only be active for a very short period.
    lastValueWrittenMask = 0x0f << gpioDirectionRegisterBitShift;
    lastValueWritten = lastValueWrittenMask;
    device->writeUInt32(gpioDirectionRegisterAddress, lastValueWritten,
        lastValueWrittenMask, writeCallback);
  } else if (currentTransferStepIndex >= 1 && currentTransferStepIndex <= 48) {
    // We have to write 24 bits in total. Each bit requires two write
    // operations: The first write operation sets the clock line low and the
    // data line according to the bit value. The second write operation sets the
    // clock line high, so that the bit is latched into the shift register. The
    // second operation always has an even step index.
    int bitIndex = (currentTransferStepIndex - 1) / 2;
    bool clockHigh = (currentTransferStepIndex % 2 == 0);
    bool outputDisable = (writeOutputValue & 0x80000000);
    bool bitValue;
    // The order in which the bits are latched into the shift register does not
    // have any particular sense. According to the documentation it is:
    // DA7, DA6, DA5, DA4, DA3, DA2, DA1, DA0, DB3, DB2, DB1, DB0, LENA, unused,
    // DA9, DA8, LENB, unused, DB9, DB8, DB7, DB6, DB5, DB4
    switch (bitIndex) {
    case 0:
      bitValue = writeOutputValue & 0x00000080;
      break;
    case 1:
      bitValue = writeOutputValue & 0x00000040;
      break;
    case 2:
      bitValue = writeOutputValue & 0x00000020;
      break;
    case 3:
      bitValue = writeOutputValue & 0x00000010;
      break;
    case 4:
      bitValue = writeOutputValue & 0x00000008;
      break;
    case 5:
      bitValue = writeOutputValue & 0x00000004;
      break;
    case 6:
      bitValue = writeOutputValue & 0x00000002;
      break;
    case 7:
      bitValue = writeOutputValue & 0x00000001;
      break;
    case 8:
      bitValue = writeOutputValue & 0x00080000;
      break;
    case 9:
      bitValue = writeOutputValue & 0x00040000;
      break;
    case 10:
      bitValue = writeOutputValue & 0x00020000;
      break;
    case 11:
      bitValue = writeOutputValue & 0x00010000;
      break;
    case 12:
      // This is the latch-enable bit for the first output.
      bitValue = writeOutputValue & 0x00000400;
      break;
    case 13:
      // This bit is unused, so we set it to zero.
      bitValue = false;
      break;
    case 14:
      bitValue = writeOutputValue & 0x00000200;
      break;
    case 15:
      bitValue = writeOutputValue & 0x00000100;
      break;
    case 16:
      // This is the latch-enable bit for the second output.
      bitValue = writeOutputValue & 0x04000000;
      break;
    case 17:
      // This bit is unused, so we set it to zero.
      bitValue = false;
      break;
    case 18:
      bitValue = writeOutputValue & 0x02000000;
      break;
    case 19:
      bitValue = writeOutputValue & 0x01000000;
      break;
    case 20:
      bitValue = writeOutputValue & 0x00800000;
      break;
    case 21:
      bitValue = writeOutputValue & 0x00400000;
      break;
    case 22:
      bitValue = writeOutputValue & 0x00200000;
      break;
    case 23:
      bitValue = writeOutputValue & 0x00100000;
      break;
    }
    lastValueWrittenMask = 0x0f << gpioOutputRegisterBitShift;
    lastValueWritten = 0;
    if (bitValue) {
      lastValueWritten |= 0x01;
    }
    if (clockHigh) {
      lastValueWritten |= 0x02;
    }
    if (outputDisable) {
      lastValueWritten |= 0x08;
    }
    lastValueWritten <<= gpioOutputRegisterBitShift;
    device->writeUInt32(gpioOutputRegisterAddress, lastValueWritten,
        lastValueWrittenMask, writeCallback);
  } else if (currentTransferStepIndex == 49) {
    // All bits have been latched into the shift register. Now we have to enable
    // the transfer latch clock, so that they become active.
    lastValueWrittenMask = 0x0f << gpioOutputRegisterBitShift;
    lastValueWritten = 0x04;
    bool outputDisable = (writeOutputValue & 0x80000000);
    if (outputDisable) {
      lastValueWritten |= 0x08;
    }
    lastValueWritten <<= gpioOutputRegisterBitShift;
    device->writeUInt32(gpioOutputRegisterAddress, lastValueWritten,
        lastValueWrittenMask, writeCallback);
  } else if (currentTransferStepIndex == 50) {
    // Finally, we disable the transfer latch clock again.
    lastValueWrittenMask = 0x0f << gpioOutputRegisterBitShift;
    lastValueWritten = 0;
    bool outputDisable = (writeOutputValue & 0x80000000);
    if (outputDisable) {
      lastValueWritten |= 0x08 << gpioOutputRegisterBitShift;
    }
    device->writeUInt32(gpioOutputRegisterAddress, lastValueWritten,
        lastValueWrittenMask, writeCallback);
  } else {
    // We are finished, so the only thing left to do is to process the record
    // again.
    writeSuccessful = true;
    scheduleProcessing();
  }
}

void MrfLongoutFineDelayShiftRegisterRecord::startNextTransferStepStatic(
    ::CALLBACK *callback) {
  void *deviceSupport;
  callbackGetUser(deviceSupport, callback);
  static_cast<MrfLongoutFineDelayShiftRegisterRecord *>(deviceSupport)->startNextTransferStep();
}

void MrfLongoutFineDelayShiftRegisterRecord::CallbackImpl::success(
    std::uint32_t, std::uint32_t value) {
  if ((value & deviceSupport.lastValueWrittenMask)
      != (deviceSupport.lastValueWritten & deviceSupport.lastValueWrittenMask)) {
    deviceSupport.writeSuccessful = false;
    deviceSupport.writeErrorMessage =
        "Mismatch between the value written to the device and the value read back from the device.";
    deviceSupport.scheduleProcessing();
  } else {
    // We schedule a delayed callback for the next transfer step. This way, we
    // can ensure that the bits are not written too quickly (which could cause
    // the shift register to not recognize them correctly). When using the
    // UDP/IP protocol, the delay introduced by the protocol is already enough,
    // but for a more direct memory access, the extra delay of 500 ns should be
    // sufficient.
    callbackRequestDelayed(&deviceSupport.nextTransferStepCallback, 0.0000005);
  }
}

void MrfLongoutFineDelayShiftRegisterRecord::CallbackImpl::failure(
    std::uint32_t address, MrfMemoryAccess::ErrorCode errorCode,
    const std::string &details) {
  deviceSupport.writeSuccessful = false;
  deviceSupport.writeErrorMessage = std::string("Error writing to address ")
      + mrfMemoryAddressToString(address) + ": "
      + (details.empty() ? mrfErrorCodeToString(errorCode) : details);
  deviceSupport.scheduleProcessing();
}

}
}
}
