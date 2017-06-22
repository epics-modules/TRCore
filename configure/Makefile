TOP=..

include $(TOP)/configure/CONFIG

TARGETS = $(CONFIG_TARGETS)
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

# Install the PV customization script and rules.
CFG += RULES.customize_pvs

include $(TOP)/configure/RULES
