#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#


NAME = MX1280

# Host architecture is ARM Cortex M4
HOST_ARCH := Cortex-M4F
HOST_ARCH_M4 = Cortex-M4
# Host MCU alias for OpenOCD
# MX1280 use the same openocd as MX1290
HOST_OPENOCD := MX1290

GLOBAL_DEFINES += __FPU_PRESENT CONFIG_CPU_MX1290

# Global flags
GLOBAL_CFLAGS   += $$(CPU_CFLAGS)    $$(ENDIAN_CFLAGS_LITTLE)
GLOBAL_CXXFLAGS += $$(CPU_CXXFLAGS)  $$(ENDIAN_CXXFLAGS_LITTLE)
GLOBAL_ASMFLAGS += $$(CPU_ASMFLAGS)  $$(ENDIAN_ASMFLAGS_LITTLE)
GLOBAL_LDFLAGS  += $$(CPU_LDFLAGS)   $$(ENDIAN_LDFLAGS_LITTLE)


GLOBAL_LDFLAGS  += -nostartfiles
GLOBAL_LDFLAGS  += -Wl,--defsym,__STACKSIZE__=$$($(RTOS)_START_STACK)
GLOBAL_LDFLAGS  += -L ./platform/MCU/$(NAME)/$(TOOLCHAIN_NAME)

# Components
$(NAME)_COMPONENTS += $(TOOLCHAIN_NAME)
$(NAME)_COMPONENTS += drivers/keypad/gpio_button

GLOBAL_INCLUDES := . \
                   .. \
                   ../.. \
                   ../../include \
                   ../../$(TOOLCHAIN_NAME) \
                   ../../$(HOST_ARCH_M4) \
                   ../../$(HOST_ARCH_M4)/CMSIS \
                   interface \
                   peripherals 

$(NAME)_SOURCES := ../../$(HOST_ARCH_M4)/crt0_$(TOOLCHAIN_NAME).c \
                   ../../$(HOST_ARCH_M4)/platform_core.c \
                   ../platform_nsclock.c

$(NAME)_CFLAGS += -Wno-implicit-function-declaration -Wno-unused-variable


####################################################################################
# MOC application
####################################################################################
$(NAME)_SOURCES += ../moc_platform_common.c
$(NAME)_SOURCES += moc/moc_adapter.c


ifneq ($(filter spi_flash_write, $(APP)),)
####################################################################################
# Building spi_flash_write
####################################################################################

DEFAULT_LINK_SCRIPT := $(TOOLCHAIN_NAME)/app_ram$(LINK_SCRIPT_SUFFIX)
GLOBAL_DEFINES      += __JTAG_FLASH_WRITER_DATA_BUFFER_SIZE__=16384

else
ifneq ($(filter $(subst ., ,$(COMPONENTS)),mocOS),)
####################################################################################
# Building standard moc application
####################################################################################

DEFAULT_LINK_SCRIPT := $(TOOLCHAIN_NAME)/app_with_moc$(LINK_SCRIPT_SUFFIX)

endif # APP=moc
endif # APP=spi_flash_write

