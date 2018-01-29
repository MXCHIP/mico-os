#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#


NAME = MX1101

EXTRA_TARGET_MAKEFILES +=  ./mico-os/platform/MCU/MX1101/mx1101_standard_targets.mk

# Host architecture is ARM Cortex M4
HOST_ARCH := Cortex-M3

# Host MCU alias for OpenOCD
HOST_OPENOCD := mx1101

GLOBAL_INCLUDES := . \
                   .. \
                   ../.. \
                   ../../include \
                   ../../$(TOOLCHAIN_NAME) \
                   ../../$(HOST_ARCH) \
                   ../../$(HOST_ARCH)/CMSIS \
                   peripherals \
                   hwlib/CMSIS/Include \
                   hwlib/inc

# Global defines
GLOBAL_DEFINES  := 

# Global flags
GLOBAL_CFLAGS   += $$(CPU_CFLAGS)    $$(ENDIAN_CFLAGS_LITTLE) -w
GLOBAL_CXXFLAGS += $$(CPU_CXXFLAGS)  $$(ENDIAN_CXXFLAGS_LITTLE)
GLOBAL_ASMFLAGS += $$(CPU_ASMFLAGS)  $$(ENDIAN_ASMFLAGS_LITTLE)
GLOBAL_LDFLAGS  += $$(CPU_LDFLAGS)   $$(ENDIAN_LDFLAGS_LITTLE) 
GLOBAL_LDFLAGS  += $$(CLIB_LDFLAGS_NANO_FLOAT)

GLOBAL_LDFLAGS  += -nostartfiles
GLOBAL_LDFLAGS  += -Wl,--defsym,__STACKSIZE__=$$($(RTOS)_START_STACK)
GLOBAL_LDFLAGS  += -L ./platform/MCU/$(NAME)/$(TOOLCHAIN_NAME)
GLOBAL_LDFLAGS  += mico-os/platform/MCU/MX1101/hwlib/libdriver.a
# Components
$(NAME)_COMPONENTS += $(TOOLCHAIN_NAME)
$(NAME)_COMPONENTS += MCU/MX1101/peripherals
$(NAME)_COMPONENTS += utilities
ifdef TOOLCHAIN_NAME
ifneq ($(wildcard $(CURDIR)lwip.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a),)
$(NAME)_PREBUILT_LIBRARY := lwip.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a
else
# Build from source
$(NAME)_COMPONENTS += MCU/MX1101/lwip
endif
endif

# Source files
$(NAME)_SOURCES := ../../$(HOST_ARCH)/crt0_$(TOOLCHAIN_NAME).c \
                   ../../$(HOST_ARCH)/platform_core.c \
                   ../../$(HOST_ARCH)/hardfault_handler.c \
                   ../platform_nsclock.c \
                   ../mico_platform_common.c \
                   platform_init.c \
                   main.c \
                   startup.s \
                   stubs.c

ifeq ($(APP),bootloader)
####################################################################################
# Building bootloader
####################################################################################

DEFAULT_LINK_SCRIPT += $(TOOLCHAIN_NAME)/bootloader$(LINK_SCRIPT_SUFFIX)
GLOBAL_INCLUDES     += 

else
ifneq ($(filter spi_flash_write, $(APP)),)
####################################################################################
# Building spi_flash_write
####################################################################################

PRE_APP_BUILDS      += bootloader
DEFAULT_LINK_SCRIPT := $(TOOLCHAIN_NAME)/app_ram$(LINK_SCRIPT_SUFFIX)
GLOBAL_DEFINES      += __JTAG_FLASH_WRITER_DATA_BUFFER_SIZE__=16384
GLOBAL_INCLUDES     += 

else
ifeq ($(USES_BOOTLOADER),1)
####################################################################################
# Building standard application to run with bootloader
####################################################################################

PRE_APP_BUILDS      += bootloader
DEFAULT_LINK_SCRIPT := $(TOOLCHAIN_NAME)/app_with_bootloader$(LINK_SCRIPT_SUFFIX)
GLOBAL_INCLUDES     += 

else
####################################################################################
# Building a standalone application (standalone app without bootloader)
####################################################################################

DEFAULT_LINK_SCRIPT := $(TOOLCHAIN_NAME)/app_no_bootloader$(LINK_SCRIPT_SUFFIX)
GLOBAL_INCLUDES     += 

endif # USES_BOOTLOADER = 1
endif # APP=spi_flash_write
endif # APP=bootloader

