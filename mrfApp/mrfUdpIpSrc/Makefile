TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += mrfUdpIp

# xxxRecord.h will be created from xxxRecord.dbd
#DBDINC += xxxRecord
# install mrfUdpIp.dbd into <top>/dbd
#DBD += mrfUdpIp.dbd

INC += MrfUdpIpClient.h
INC += MrfUdpIpMemoryAccess.h
INC += MrfUdpPacket.h

# specify all source files to be compiled and added to the library
mrfUdpIp_SRCS += MrfUdpIpClient.cpp
mrfUdpIp_SRCS += MrfUdpIpMemoryAccess.cpp
mrfUdpIp_SRCS += MrfUdpPacket.cpp

# mrfUdpIp_LIBS += $(EPICS_BASE_IOC_LIBS)
mrfUdpIp_LIBS += mrfCommon

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

