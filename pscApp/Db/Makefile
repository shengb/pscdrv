TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this
ifneq ($(DEVIOCSTATS),)
#DB_INSTALLS += $(EPICS_BASE)/db/iocAdminSoft.db
endif
#DB_INSTALLS += $(EPICS_BASE)/db/save_restoreStatus.db

# PSC IOC database
DB += Commands.db
DB += Status.db
DB += ADCWaveform.db
DB += RampingTable.db

DB += Ch-WfIn-Coeff.db
DB += Ch-WfOut-Coeff.db
DB += Ch-DIn.db
DB += Ch-DOut.db

DB += Average.db
DB += FFT.db
DB += MonitBinary.db
DB += MonitBinaryCommon.db
DB += MonitScalar.db
DB += MonitScalarCommon.db
DB += MonitWf.db
DB += MonitWfCommon.db
DB += Slicer.db
DB += Slicer2.db
DB += Subsampling.db
DB += Subtract.db

DB += EncoderRx.db
DB += EncoderTx.db
DB += EncoderSts.db

DB += GenRx.db
DB += GenTx.db
DB += GenSts.db


#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

