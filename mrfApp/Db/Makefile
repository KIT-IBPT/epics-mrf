TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

MRF_ALL_DEVICES = $(MRF_EVG_DEVICES) $(MRF_EVR_DEVICES)
MRF_EVG_DEVICES = vme-evg-230
MRF_EVR_DEVICES = mtca-evr-300 vme-evr-230 vme-evr-230rf vme-evr-300

MRF_DB_DEPS = \
 $(COMMON_DIR)/../generate_db_or_req.py \
 $(COMMON_DIR)/../mrf_db_gen/*.py \
 $(COMMON_DIR)/../templates/*.db
MRF_REQ_DEPS = $(MRF_DB_DEPS:.db=.req)

MRF_DB_TARGETS = \
 $(foreach device_type,$(MRF_ALL_DEVICES),$(COMMON_DIR)/$(device_type).db)
MRF_REQ_TARGETS = $(MRF_DB_TARGETS:.db=.req)

MRF_DESCRIPTION_RECORD_TYPE ?= lso

#----------------------------------------------------
#  Optimization of db files using dbst (DEFAULT: NO)
#DB_OPT = YES

#----------------------------------------------------
# Create and install (or just install) into <top>/db
# databases, templates, substitutions like this
DB += $(notdir $(MRF_DB_TARGETS))
DB_INSTALLS += $(MRF_REQ_TARGETS)
DB += mrf-autosave-menu.db
DB += mrf-write-all-settings-with-status.db

#----------------------------------------------------
# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_template = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

$(MRF_DB_TARGETS): $(MRF_DB_DEPS)
	python3 "$(COMMON_DIR)/../generate_db_or_req.py" \
	 --description-record-type "$(MRF_DESCRIPTION_RECORD_TYPE)" \
	 "$@"

$(MRF_REQ_TARGETS): $(MRF_REQ_DEPS)
	python3 "$(COMMON_DIR)/../generate_db_or_req.py" \
	 --description-record-type "$(MRF_DESCRIPTION_RECORD_TYPE)" \
	 "$@"

# This line is needed so that make does not complain about missing *.d files
# when using recent versions of EPICS Base.
.PHONY: $(foreach device_type,$(MRF_ALL_DEVICES),$(device_type).db$(DEP))
