EXTRA_POST_BUILD_TARGETS += gen_standard_images

ifeq ($(HOST_OS),Win32)
ENCRYPT := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/encrypt_win.exe"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/xz_win.exe"
else  # Win32
ifeq ($(HOST_OS),Linux32)
ENCRYPT := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/encrypt_linux"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/xz_linux"
else # Linux32
ifeq ($(HOST_OS),Linux64)
ENCRYPT := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/encrypt_linux"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/xz_linux"
else # Linux64
ifeq ($(HOST_OS),OSX)
ENCRYPT := "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/encrypt_osx"
XZ 		:= "$(SOURCE_ROOT)/mico-os/platform/MCU/MOC108/xz_osx"
else # OSX
$(error not surport for $(HOST_OS))
endif # OSX
endif # Linux64
endif # Linux32
endif # Win32

#bootloader
BOOT_BIN_FILE := $(MICO_OS_PATH)/resources/moc_kernel/$(MODULE)/boot.bin
BOOT_OFFSET   := 0x0

#application 
APP_BIN_FILE := $(BIN_OUTPUT_FILE)
APP_OFFSET := 0x13200

#ate firmware
ATE_BIN_FILE := $(MICO_OS_PATH)/resources/ate_firmware/$(MODULE)/ate.bin
ATE_OFFSET := 0xA1090

# _crc.bin
CRC_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=_crc$(BIN_OUTPUT_SUFFIX))
# _crc.bin.xz
CRC_XZ_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=_crc$(BIN_OUTPUT_SUFFIX).xz)
# .ota.bin
OTA_BIN_OUTPUT_FILE := $(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.ota$(BIN_OUTPUT_SUFFIX))

# Required to build Full binary file
GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT:= $(SCRIPTS_PATH)/gen_common_bin_output_file.py

MICO_ALL_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.all$(BIN_OUTPUT_SUFFIX))

gen_crc_bin: build_done
	$(eval OUT_MSG := $(shell $(ENCRYPT) $(BIN_OUTPUT_FILE) 0 0 0 0))

gen_xz_bin: gen_crc_bin
	$(eval OUT_MSG := $(shell $(XZ) --lzma2=dict=32KiB --check=crc32 -k $(CRC_BIN_OUTPUT_FILE)))

gen_standard_images: gen_crc_bin gen_xz_bin
	$(CP) $(CRC_BIN_OUTPUT_FILE) $(BIN_OUTPUT_FILE)
	$(RM) $(CRC_BIN_OUTPUT_FILE)
	$(CP) $(CRC_XZ_BIN_OUTPUT_FILE) $(OTA_BIN_OUTPUT_FILE)
	$(RM) $(CRC_XZ_BIN_OUTPUT_FILE)
	$(QUIET)$(RM) $(MICO_ALL_BIN_OUTPUT_FILE)
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MICO_ALL_BIN_OUTPUT_FILE) -f $(BOOT_OFFSET) $(BOOT_BIN_FILE)              
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MICO_ALL_BIN_OUTPUT_FILE) -f $(APP_OFFSET)  $(APP_BIN_FILE)
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MICO_ALL_BIN_OUTPUT_FILE) -f $(ATE_OFFSET)  $(ATE_BIN_FILE)
