TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#==================================================
# build a support library

LIBRARY_IOC += trCore

INC += TRArmInfo.h
INC += TRBaseConfig.h
INC += TRBaseDriver.h
INC += TRBurstMetaInfo.h
INC += TRChannelDataSubmit.h
INC += TRChannelsDriver.h
INC += TRConfigParam.h
INC += TRConfigParamTraits.h
INC += TRNonCopyable.h
INC += TRTimeArrayDriver.h
INC += TRWorkerThread.h

trCore_SRCS += TRBaseDriver.cpp
trCore_SRCS += TRChannelDataSubmit.cpp
trCore_SRCS += TRChannelsDriver.cpp
trCore_SRCS += TRConfigParam.cpp
trCore_SRCS += TRTimeArrayDriver.cpp
trCore_SRCS += TRWorkerThread.cpp

trCore_LIBS += ADBase asyn $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

