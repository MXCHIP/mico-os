#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = MiCO

ifndef USES_BOOTLOADER
USES_BOOTLOADER :=1
endif

ifneq ($(ALIOS_SUPPORT),y)
$(NAME)_COMPONENTS += MiCO/core
endif

$(NAME)_SOURCES += mico_main.c core/mico_config.c

ifneq ($(filter $(subst ., ,$(COMPONENTS)),mocOS mocIP),)
$(NAME)_SOURCES += moc_main.c
endif

ifneq ($(ALIOS_SUPPORT),y)
$(NAME)_COMPONENTS += MiCO/security \
                      MiCO/system
endif

$(NAME)_COMPONENTS += utilities

GLOBAL_DEFINES += 

# Add MCU component
ifeq ($(ALIOS_SUPPORT),y)
$(NAME)_COMPONENTS += alios
else
ifneq ($(MBED_SUPPORT),)
$(NAME)_COMPONENTS += platform/mbed
else
$(NAME)_COMPONENTS += platform/MCU/$(HOST_MCU_FAMILY)
endif
endif

# Easylink Button
$(NAME)_COMPONENTS += drivers/keypad/gpio_button

# Define the default ThreadX and FreeRTOS starting stack sizes
FreeRTOS_START_STACK := 800
ThreadX_START_STACK  := 800
mocOS_START_STACK    := 800


GLOBAL_INCLUDES += . \
                   system \
                   system/include \
                   security
                   
# $(NAME)_CFLAGS  = $(COMPILER_SPECIFIC_PEDANTIC_CFLAGS)


