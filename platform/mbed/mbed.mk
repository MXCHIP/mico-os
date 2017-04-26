#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = MiCO_mbed_hal_Interface

MBED_OS_DIR = mbed-os
LIST_SUB_DIRS_SCRIPT  := $(MAKEFILES_PATH)/scripts/list_sub_directories.py

GLOBAL_INCLUDES := . \
                   .. \
                   include \
                   peripherals

# Global flags
GLOBAL_CFLAGS   += $$(CPU_CFLAGS)    $$(ENDIAN_CFLAGS_LITTLE)
GLOBAL_CXXFLAGS += $$(CPU_CXXFLAGS)  $$(ENDIAN_CXXFLAGS_LITTLE)
GLOBAL_ASMFLAGS += $$(CPU_ASMFLAGS)  $$(ENDIAN_ASMFLAGS_LITTLE)
GLOBAL_LDFLAGS  += $$(CPU_LDFLAGS)   $$(ENDIAN_LDFLAGS_LITTLE)


#GLOBAL_LDFLAGS  += -Wl,--defsym __STACKSIZE__=$$($(RTOS)_START_STACK)


# Components
$(NAME)_COMPONENTS += $(TOOLCHAIN_NAME)
$(NAME)_COMPONENTS += platform/mbed/mbed-os

# Components, add mbed targets
TARGETS := $(foreach target, $(MBED_TARGETS), TARGET_$(target))
$(eval DIRS := $(shell $(PYTHON) $(LIST_SUB_DIRS_SCRIPT) mico-os/platform/mbed/targets))
$(foreach DIR, $(DIRS), $(if $(filter $(notdir $(DIR)), $(TARGETS)), $(eval $(NAME)_COMPONENTS += $(DIR)),))

# Source files
$(NAME)_SOURCES := mico_platform_common.c \
                   wlan_platform_common.c \
                   peripherals/platform_init.c \
                   peripherals/platform_gpio.c \
                   peripherals/platform_uart.c \
                   peripherals/platform_watchdog.c \
                   peripherals/platform_mcu_powersave.c
                   

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
#DEFAULT_LINK_SCRIPT := $(TOOLCHAIN_NAME)/app_with_bootloader$(LINK_SCRIPT_SUFFIX)
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

