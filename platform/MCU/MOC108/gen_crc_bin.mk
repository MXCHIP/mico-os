
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

CRC_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=_crc$(BIN_OUTPUT_SUFFIX))
CRC_XZ_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=_crc$(BIN_OUTPUT_SUFFIX).xz)
OTA_BIN_OUTPUT_FILE := $(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.ota$(BIN_OUTPUT_SUFFIX))

EXTRA_POST_BUILD_TARGETS += gen_crc_bin

gen_crc_bin:
	$(eval OUT_MSG := $(shell $(ENCRYPT) $(BIN_OUTPUT_FILE) 0 0 0 0; \
	$(XZ) --lzma2=dict=32KiB --check=crc32 -k $(CRC_BIN_OUTPUT_FILE); \
	$(CP) $(CRC_XZ_BIN_OUTPUT_FILE) $(OTA_BIN_OUTPUT_FILE); \
	$(RM) $(CRC_BIN_OUTPUT_FILE); \
	$(RM) $(CRC_XZ_BIN_OUTPUT_FILE); \
	$(RM) $(BIN_OUTPUT_FILE)))