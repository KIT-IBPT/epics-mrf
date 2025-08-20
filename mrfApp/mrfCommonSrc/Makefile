TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += mrfCommon

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install mrfCommon.dbd into <top>/dbd
#DBD += mrfCommon.dbd

INC += MrfConsistentAsynchronousMemoryAccess.h
INC += MrfConsistentMemoryAccess.h
INC += MrfFdSelector.h
INC += MrfMemoryAccess.h
INC += mrfGaiErrorCategory.h

# specify all source files to be compiled and added to the library
mrfCommon_SRCS += MrfConsistentAsynchronousMemoryAccess.cpp
mrfCommon_SRCS += MrfConsistentMemoryAccess.cpp
mrfCommon_SRCS += MrfFdSelector.cpp
mrfCommon_SRCS += MrfMemoryAccess.cpp
mrfCommon_SRCS += mrfGaiErrorCategory.cpp

# mrfCommon_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

