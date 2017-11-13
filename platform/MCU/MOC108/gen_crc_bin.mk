EXTRA_POST_BUILD_TARGETS += $(MICO_ALL_BIN_OUTPUT_FILE)

ifeq ($(HOST_OS),Win32)
CRC := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/crc/win/crc.exe"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/xz/win/xz.exe"
else  # Win32
ifeq ($(HOST_OS),Linux32)
CRC := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/crc/linux/crc"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/xz/linux/xz"
else # Linux32
ifeq ($(HOST_OS),Linux64)
CRC := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/crc/linux/crc"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/xz/linux/xz"
else # Linux64
ifeq ($(HOST_OS),OSX)
CRC := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/crc/osx/crc"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/xz/osx/xz"
else # OSX
$(error not surport for $(HOST_OS))
endif # OSX
endif # Linux64
endif # Linux32
endif # Win32

ADD_MD5_SCRIPT := $(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/tools/add_md5.py

# _crc.bin
CRC_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=_crc$(BIN_OUTPUT_SUFFIX))
# _crc.bin.xz
CRC_XZ_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=_crc$(BIN_OUTPUT_SUFFIX).xz)
# .ota.bin
OTA_BIN_OUTPUT_FILE := $(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.ota$(BIN_OUTPUT_SUFFIX))
# .raw.bin
RAW_BIN_OUTPUT_FILE := $(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.raw$(BIN_OUTPUT_SUFFIX))

#bootloader
BOOT_BIN_FILE := $(MICO_OS_PATH)/resources/moc_kernel/$(MODULE)/boot.bin
BOOT_OFFSET   := 0x0

#application 
APP_BIN_FILE := $(BIN_OUTPUT_FILE)
APP_OFFSET := 0x13200

#ate firmware
ATE_BIN_FILE := $(MICO_OS_PATH)/resources/ate_firmware/$(MODULE)/ate.bin
ATE_OFFSET := 0x100100

# Required to build Full binary file
GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT:= $(SCRIPTS_PATH)/gen_common_bin_output_file.py

MICO_ALL_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.all$(BIN_OUTPUT_SUFFIX))

$(CRC_BIN_OUTPUT_FILE): $(STRIPPED_LINK_OUTPUT_FILE)
	$(QUIET)$(OBJCOPY) -O binary -R .eh_frame -R .init -R .fini -R .comment -R .ARM.attributes $(STRIPPED_LINK_OUTPUT_FILE) $(BIN_OUTPUT_FILE)
	$(CRC) $(BIN_OUTPUT_FILE) 0 0 0 0 > $(DEV_NULL)

$(OTA_BIN_OUTPUT_FILE): $(CRC_BIN_OUTPUT_FILE)
	$(XZ) --lzma2=dict=32KiB --check=crc32 -k $(CRC_BIN_OUTPUT_FILE)
	$(CP) $(CRC_XZ_BIN_OUTPUT_FILE) $(OTA_BIN_OUTPUT_FILE)
	$(RM) $(CRC_XZ_BIN_OUTPUT_FILE)
	$(PYTHON) $(ADD_MD5_SCRIPT) $(OTA_BIN_OUTPUT_FILE)

$(MICO_ALL_BIN_OUTPUT_FILE): $(OTA_BIN_OUTPUT_FILE)
	$(CP) $(BIN_OUTPUT_FILE) $(RAW_BIN_OUTPUT_FILE)
	$(CP) $(CRC_BIN_OUTPUT_FILE) $(BIN_OUTPUT_FILE)
	$(RM) $(CRC_BIN_OUTPUT_FILE)
	$(QUIET)$(RM) $(MICO_ALL_BIN_OUTPUT_FILE)
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MICO_ALL_BIN_OUTPUT_FILE) -f $(BOOT_OFFSET) $(BOOT_BIN_FILE)              
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MICO_ALL_BIN_OUTPUT_FILE) -f $(APP_OFFSET)  $(APP_BIN_FILE)
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MICO_ALL_BIN_OUTPUT_FILE) -f $(ATE_OFFSET)  $(ATE_BIN_FILE)