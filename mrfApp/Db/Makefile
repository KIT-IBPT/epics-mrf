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
DB += vme-evg-230.db
DB_INSTALLS += $(COMMON_DIR)/vme-evg-230.req
DB += mtca-evr-300.db
DB_INSTALLS += $(COMMON_DIR)/mtca-evr-300.req
DB += vme-evr-230.db
DB_INSTALLS += $(COMMON_DIR)/vme-evr-230.req
DB += vme-evr-230rf.db
DB_INSTALLS += $(COMMON_DIR)/vme-evr-230rf.req
DB += mrf-autosave-menu.db
DB += mrf-write-all-settings-with-status.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

$(COMMON_DIR)/vme-evg-230.db: $(COMMON_DIR)/../generate_evg_records.sh $(COMMON_DIR)/../generate_evg.sh $(COMMON_DIR)/../generate_lib.sh $(COMMON_DIR)/../evg-*.inc.db $(COMMON_DIR)/../template-*.inc.db
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" "$(COMMON_DIR)/../generate_evg_records.sh" "$(basename $(notdir $@))" >"$@"

$(COMMON_DIR)/vme-evg-230.req: $(COMMON_DIR)/../generate_evg_autosave_request_file.sh $(COMMON_DIR)/../generate_evg.sh $(COMMON_DIR)/../generate_lib.sh $(COMMON_DIR)/../evg-*.inc.req $(COMMON_DIR)/../template-*.inc.req
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" "$(COMMON_DIR)/../generate_evg_autosave_request_file.sh" "$(basename $(notdir $@))" >"$@"

$(COMMON_DIR)/mtca-evr-300.db $(COMMON_DIR)/vme-evr-230.db $(COMMON_DIR)/vme-evr-230rf.db: $(COMMON_DIR)/../generate_evr_records.sh $(COMMON_DIR)/../generate_evr.sh $(COMMON_DIR)/../generate_lib.sh $(COMMON_DIR)/../evr-*.inc.db $(COMMON_DIR)/../template-*.inc.db
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" "$(COMMON_DIR)/../generate_evr_records.sh" "$(basename $(notdir $@))" >"$@"

$(COMMON_DIR)/mtca-evr-300.req $(COMMON_DIR)/vme-evr-230.req $(COMMON_DIR)/vme-evr-230rf.req: $(COMMON_DIR)/../generate_evr_autosave_request_file.sh $(COMMON_DIR)/../generate_evr.sh $(COMMON_DIR)/../generate_lib.sh $(COMMON_DIR)/../evr-*.inc.req $(COMMON_DIR)/../template-*.inc.req
	MRF_DESCRIPTION_RECORD_TYPE="$(MRF_DESCRIPTION_RECORD_TYPE)" "$(COMMON_DIR)/../generate_evr_autosave_request_file.sh" "$(basename $(notdir $@))" >"$@"

# This line is needed so that make does not complain about missing *.d files
# when using recent versions of EPICS Base.
.PHONY: mtca-evr-300.db$(DEP) vme-evg-230.db$(DEP) vme-evr-230.db$(DEP) vme-evr-230rf.db$(DEP)
