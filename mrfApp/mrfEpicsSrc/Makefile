TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += mrfEpics

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install mrf.dbd into <top>/dbd
DBD += mrfCommon.dbd

INC += MrfDeviceRegistry.h
INC += MrfMemoryCache.h
INC += mrfEpicsError.h

# specify all source files to be compiled and added to the library
mrfEpics_SRCS += MrfBiRecord.cpp
mrfEpics_SRCS += MrfBiInterruptRecord.cpp
mrfEpics_SRCS += MrfBoRecord.cpp
mrfEpics_SRCS += MrfDeviceRegistry.cpp
mrfEpics_SRCS += MrfInterruptRecordAddress.cpp
mrfEpics_SRCS += MrfLonginRecord.cpp
mrfEpics_SRCS += MrfLonginInterruptRecord.cpp
mrfEpics_SRCS += MrfLongoutRecord.cpp
mrfEpics_SRCS += MrfLongoutFineDelayShiftRegisterRecord.cpp
mrfEpics_SRCS += MrfMbbiDirectInterruptRecord.cpp
mrfEpics_SRCS += MrfMemoryCache.cpp
mrfEpics_SRCS += MrfRecordAddress.cpp
mrfEpics_SRCS += MrfStringinRecord.cpp
mrfEpics_SRCS += MrfWaveformInRecord.cpp
mrfEpics_SRCS += MrfWaveformOutRecord.cpp
mrfEpics_SRCS += mrfArrayASubRoutines.c
mrfEpics_SRCS += mrfEpicsError.cpp
mrfEpics_SRCS += mrfIocshReadUInt16.cpp
mrfEpics_SRCS += mrfIocshReadUInt32.cpp
mrfEpics_SRCS += mrfIocshWriteUInt16.cpp
mrfEpics_SRCS += mrfIocshWriteUInt32.cpp
mrfEpics_SRCS += mrfRecordDefinitions.cpp
mrfEpics_SRCS += mrfRegistrarCommon.cpp

mrfEpics_LIBS += $(EPICS_BASE_IOC_LIBS)
mrfEpics_LIBS += mrfCommon

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

