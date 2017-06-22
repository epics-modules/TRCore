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
DB += TRBase.db
DB += TRChannel.db
DB += TRChannelData.db
DB += TRGenericRequest.db
DB += TRSampleRateAttrTest.db

# Install the Python script for customizing PV names.
# The cfg folder would be the preferred place for this but it does
# not work with old EPICS versions as they would try to include the
# script from a makefile. So install to the db folder instead.
DB_INSTALLS += ../customize_pvs.py

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

