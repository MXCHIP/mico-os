#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2017 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = stm32f411xg

SRC_DIR := ../../../../mbed-os/targets/TARGET_STM/TARGET_STM32F4/TARGET_STM32F412xG


$(NAME)_SOURCES := cmsis_nvic.c \
                   TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/startup_stm32f412xx.s
                   
GLOBAL_INCLUDES := $(SRC_DIR) $(SRC_DIR)/device
                   
#DEFAULT_LINK_SCRIPT := TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/STM32F411XE$(LINK_SCRIPT_SUFFIX)


ifeq ($(APP),bootloader)
####################################################################################
# Building bootloader
####################################################################################

DEFAULT_LINK_SCRIPT += TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/STM32F412xG_boot$(LINK_SCRIPT_SUFFIX)
GLOBAL_INCLUDES     += 

else
ifneq ($(filter spi_flash_write, $(APP)),)
####################################################################################
# Building spi_flash_write
####################################################################################

PRE_APP_BUILDS      += bootloader
DEFAULT_LINK_SCRIPT += TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/STM32F412xG_app_ram$(LINK_SCRIPT_SUFFIX)
GLOBAL_DEFINES      += __JTAG_FLASH_WRITER_DATA_BUFFER_SIZE__=16384
GLOBAL_INCLUDES     += 

else
ifeq ($(USES_BOOTLOADER),1)
####################################################################################
# Building standard application to run with bootloader
####################################################################################

PRE_APP_BUILDS      += bootloader
DEFAULT_LINK_SCRIPT := TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/STM32F412xG_app_needs_boot$(LINK_SCRIPT_SUFFIX)
GLOBAL_INCLUDES     += 

else
####################################################################################
# Building a standalone application (standalone app without bootloader)
####################################################################################

DEFAULT_LINK_SCRIPT := TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/STM32F412xG$(LINK_SCRIPT_SUFFIX)
GLOBAL_INCLUDES     += 

endif # USES_BOOTLOADER = 1
endif # APP=spi_flash_write
endif # APP=bootloader


