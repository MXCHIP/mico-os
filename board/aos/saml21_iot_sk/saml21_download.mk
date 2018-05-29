.PHONY:  download copy_output_for_eclipse

EXTRA_POST_BUILD_TARGETS += copy_output_for_eclipse

ifeq (download,$(findstring download,$(MAKECMDGOALS)))
OPENOCD_LOG_FILE ?= $(BUILD_DIR)/openocd_log.txt
DOWNLOAD_LOG := >> $(OPENOCD_LOG_FILE)
endif


download_app: $(STRIPPED_LINK_OUTPUT_FILE) display_map_summary kill_openocd
	$(eval IMAGE_SIZE := $(shell $(PYTHON) $(IMAGE_SIZE_SCRIPT) $(BIN_OUTPUT_FILE)))
	$(QUIET)$(ECHO) Downloading application size: $(IMAGE_SIZE) bytes... 
	$(call CONV_SLASHES, $(OPENOCD_FULL_NAME)) -s $(SOURCE_ROOT) -f $(OPENOCD_CFG_PATH)/interface/jlink.cfg -c "transport select swd" -f $(OPENOCD_CFG_PATH)/at91samdXX/at91samdXX.cfg -c init -c "reset halt" -c "flash write_image erase $(LINK_OUTPUT_FILE)" -c reset -c shutdown $(DOWNLOAD_LOG) 2>&1 && $(ECHO) Download complete && $(ECHO_BLANK_LINE) || $(ECHO) Download failed. See $(OPENOCD_LOG_FILE) for detail.

download: download_app

run: $(SHOULD_I_WAIT_FOR_DOWNLOAD)
	$(QUIET)$(ECHO) Resetting target
	$(QUIET)$(call CONV_SLASHES,$(OPENOCD_FULL_NAME)) -c "log_output $(OPENOCD_LOG_FILE)" -s $(SOURCE_ROOT) -f $(OPENOCD_PATH)/scripts/interface/jlink.cfg -c "transport select swd" -f $(OPENOCD_PATH)/scripts/target/at91samdXX.cfg -c init -c soft_reset_halt -c resume -c shutdown $(DOWNLOAD_LOG) 2>&1 && $(ECHO) Target running


copy_output_for_eclipse: build_done kill_openocd
	$(QUIET)$(call MKDIR, $(BUILD_DIR)/eclipse_debug/)
	$(QUIET)$(CP) $(LINK_OUTPUT_FILE) $(BUILD_DIR)/eclipse_debug/last_built.elf

	
ifneq ($(MICO_OS_PATH),)
kill_openocd:
	$(KILL_OPENOCD)
else
kill_openocd:
endif