Customizing PVs       {#customizing-pvs}
============

# Customization Script

A Python (2.7) script `customize_pvs.py` is provided to customize the names
of PVs and macros in DB files. The script is located in `TRCore/Db`
and is copied to `TRCore/db` during build.

For example, to rename `TRBase.db` PVs "$(PREFIX):arm" and "$(PREFIX):NUM_BURSTS" to
"$(PREFIX).Arm" and "$(PREFIX).NumBursts", run:

```
TRCore/Db/customize_pvs.py -s TRCore/TRCoreApp/Db -b TRBase.db -r arm .Arm -r NUM_BURSTS .NumBursts -d <dest-dir>
```

Here, `<dest-dir>` is the directory where the customized DB files will be written.
By default, the new DB files will have the same name as the originals, but if
you pass `-p <prefix>`, they will have a prefix.

The script can also substitute macros. For example, to replace the macro AUTORESTART_ZNAM
with "no", pass the following to the script: `-m AUTORESTART_ZNAM no`.

# Integration into the EPICS Build System

It is possible to run the script from the EPICS build system using already
provided makefile rules.
The assumption here is that the module being built depends on TRCore
by specifying the variable TR_CORE in its RELEASE file.

Then, the module can generate customized TRCore DB files by including
these lines in its Db Makefile:

```
# This line goes just after "include $(TOP)/configure/CONFIG".
# It is needed only for old EPICS, newer EPICS will include it automatically.
# It is okay to add it in any case since it will detect double inclusion.
include $(TR_CORE)/cfg/RULES.customize_pvs

# This goes in the place where DBs are specified.
# If you want a prefix adjust TR_CUSTOM_DB_PREFIX otherwise leave it empty.
TR_CUSTOM_DB_PREFIX :=
DB += $(addprefix $(TR_CUSTOM_DB_PREFIX),$(TRCORE_CUSTOMIZABLE_DBS))

# This goes to the bottom BELOW "#  ADD RULES AFTER THIS LINE"
DB_CUSTOMIZE := -r arm :Arm -r autoRestart :AutoRestart
$(eval $(call TRCORE_CUSTOM_DB_RULES,$(TRCORE_DB_DIR),$(TRCORE_CUSTOMIZABLE_DBS),$(TR_CUSTOM_DB_PREFIX),$(DB_CUSTOMIZE)))
```

The TRCore DBs which are customizable (TRCORE_CUSTOMIZABLE_DBS) are
`TRBase.db`, `TRChannel.db` and `TRChannelData.db`.

For your IOC you will of course have to use the resulting customized DB
files which will be installed to the `db` folder of the module that
performed this customization, and not the original TRCore files.

You can also use this approach to customize digitizer-specific PVs.
Just use the appropriate values instead of TRCORE_CUSTOMIZABLE_DBS and TRCORE_DB_DIR
and different variable names to avoid conflicts.

In order to also customize device-specific DBs, add the following lines in addition
to the ones shown above. The example is for TRSIS and will need minor adaptations
for a different driver.

```
# This goes in the place where DBs are specified.
TRSIS_DBS := TRSIS.db TRSIS_Channel.db
DB += $(addprefix $(TR_CUSTOM_DB_PREFIX),$(TRSIS_DBS))

# This goes to the bottom BELOW "#  ADD RULES AFTER THIS LINE"
$(eval $(call TRCORE_CUSTOM_DB_RULES,$(TRSIS)/db,$(TRSIS_DBS),$(TR_CUSTOM_DB_PREFIX),$(DB_CUSTOMIZE)))
```

Here it is assumed that `RULES.customize_pvs` was already included
and that `TR_CUSTOM_DB_PREFIX` and `DB_CUSTOMIZE` are already
defined, as shown in the previous example.

It is advsed to use the same file prefix and customizations hence the variables
`TR_CUSTOM_DB_PREFIX` and `DB_CUSTOMIZE` should only be defined once and
used both for customization of TRCore and device-specific DBs.

In this example, `$(TRSIS)/db` is the location of the non-customized device-specific
DBs. The variable name corresponds to the RELEASE variable which points to the
driver.



