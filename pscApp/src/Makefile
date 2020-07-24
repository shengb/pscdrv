TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application

PROD_IOC = psc
# psc.dbd will be created and installed
DBD += psc.dbd

# psc.dbd will be made up from these files:
psc_DBD += base.dbd
psc_DBD += pscCore.dbd
psc_DBD += psclocal.dbd
psc_DBD += slicer2.dbd
psc_DBD += subCalcAlarm.dbd

ifeq ($(USE_FFTW),NO)
psc_DBD += pscSig.dbd
endif

# Add all the support libraries needed by this IOC
#psc_LIBS += xxx

# psc_registerRecordDeviceDriver.cpp derives from psc.dbd
psc_SRCS += psc_registerRecordDeviceDriver.cpp

USR_CXXFLAGS_DEFAULT += -fno-operator-names
USR_CXXFLAGS_DEFAULT += -pedantic

psc_SRCS += psctbl.cpp

psc_SRCS += aSubOperations.cpp
psc_SRCS += aSubEnc.cpp
psc_SRCS += aSubGen.cpp

psc_SRCS += slicer2.c
#bpms
psc_SRCS += posPolyCoeffCal.c
psc_SRCS += saRmsCal.c
psc_SRCS += saStdCal.c
psc_SRCS += bpm_aSubUtils.c
psc_SRCS += subCalcAlarm.c
#psc_SRCS += bpm_aSub_fft.c


# Build the main IOC entry point on workstation OSs.
psc_SRCS_DEFAULT += pscMain.cpp
psc_SRCS_vxWorks += -nil-

# Math
psc_SYS_LIBS += gsl gslcblas m

# Save/restore
#psc_DBD  += asSupport.dbd
#psc_LIBS += autosave


# bpm
psc_DBD  += waveAnlRecord.dbd
psc_LIBS += waveAnl
#psc_LIBS += sigLib

# For Channel Finder
psc_DBD += system.dbd

# For other stuff

ifneq ($(CAPUTLOG),)
#psc_DBD +=  caPutLog.dbd
#psc_LIBS += caPutLog
endif

ifneq ($(DEVIOCSTATS),)
#psc_DBD +=  devIocStats.dbd
#psc_LIBS += devIocStats
endif

# Add support from base/src/vxWorks if needed
#psc_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
psc_LIBS += pscCore $(EPICS_BASE_IOC_LIBS)

psc_SYS_LIBS += event_core event_extra

ifeq ($(POSIX),YES)
psc_SYS_LIBS += event_pthreads
endif

ifneq ($(LIBEVENT),)
LDFLAGS += -L$(LIBEVENT)/lib
CPPFLAGS += -I$(LIBEVENT)/include
endif

ifeq ($(USE_FFTW),YES)
psc_LIBS += pscSig
psc_SYS_LIBS += fftw3
endif

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

