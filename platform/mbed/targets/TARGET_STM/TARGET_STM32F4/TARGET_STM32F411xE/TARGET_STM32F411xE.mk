#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2017 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = stm32f411xe

SRC_DIR := ../../../../mbed-os/targets/TARGET_STM/TARGET_STM32F4/TARGET_STM32F411xE


$(NAME)_SOURCES := cmsis_nvic.c \
                   $(SRC_DIR)/device/TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/startup_stm32f411xe.S
                   
GLOBAL_INCLUDES := $(SRC_DIR) $(SRC_DIR)/device
                   
DEFAULT_LINK_SCRIPT := TOOLCHAIN_$(TOOLCHAIN_NAME_MBED)/STM32F411XE$(LINK_SCRIPT_SUFFIX)

