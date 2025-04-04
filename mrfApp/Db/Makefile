TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

MRF_ALL_DEVICES = $(MRF_EVG_DEVICES) $(MRF_EVR_DEVICES)
MRF_EVG_DEVICES = vme-evg-230
MRF_EVR_DEVICES = mtca-evr-300 vme-evr-230 vme-evr-230rf vme-evr-300
MRF_DB_DEPS = \
 $(COMMON_DIR)/../generate_$(1).sh \
 $(COMMON_DIR)/../generate_lib.sh \
 $(COMMON_DIR)/../$(1)-*.inc.db \
 $(COMMON_DIR)/../template-*.inc.db
MRF_REQ_DEPS = $(MRF_DB_DEPS:.db=.req)

MRF_EVG_DB_DEPS = $(call MRF_DB_DEPS,evg)
MRF_EVG_DB_TARGETS = \
 $(foreach device_type,$(MRF_EVG_DEVICES),$(COMMON_DIR)/$(device_type).db)
MRF_EVG_REQ_DEPS = $(MRF_EVG_DB_DEPS:.db=.req)
MRF_EVG_REQ_TARGETS = $(MRF_EVG_DB_TARGETS:.db=.req)
MRF_EVR_DB_DEPS = $(call MRF_DB_DEPS,evr)
MRF_EVR_DB_TARGETS = \
 $(foreach device_type,$(MRF_EVR_DEVICES),$(COMMON_DIR)/$(device_type).db)
MRF_EVR_REQ_DEPS = $(MRF_EVR_DB_DEPS:.db=.req)
MRF_EVR_REQ_TARGETS = $(MRF_EVR_DB_TARGETS:.db=.req)

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this
DB += $(foreach device_type,$(MRF_ALL_DEVICES),$(device_type).db)
DB_INSTALLS += $(MRF_EVG_REQ_TARGETS)
DB_INSTALLS += $(MRF_EVR_REQ_TARGETS)
DB += mrf-autosave-menu.db
DB += mrf-write-all-settings-with-status.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

$(MRF_EVG_DB_TARGETS): $(MRF_EVG_DB_DEPS)
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" \
	 "$(COMMON_DIR)/../generate_evg.sh" \
	 "$(basename $(notdir $@))" \
	 records \
	 >"$@"

$(MRF_EVG_REQ_TARGETS): $(MRF_EVG_REQ_DEPS)
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" \
	 "$(COMMON_DIR)/../generate_evg.sh" \
	 "$(basename $(notdir $@))" \
	 autosave_request_file \
	 >"$@"

$(MRF_EVR_DB_TARGETS): $(MRF_EVR_DB_DEPS)
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" \
	 "$(COMMON_DIR)/../generate_evr.sh" \
	 "$(basename $(notdir $@))" \
	 records \
	 >"$@"

$(MRF_EVR_REQ_TARGETS): $(MRF_EVR_REQ_DEPS)
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" \
	 "$(COMMON_DIR)/../generate_evr.sh" \
	 "$(basename $(notdir $@))" \
	 autosave_request_file \
	 >"$@"

# This line is needed so that make does not complain about missing *.d files
# when using recent versions of EPICS Base.
.PHONY: $(foreach device_type,$(MRF_ALL_DEVICES),$(device_type).db$(DEP))
