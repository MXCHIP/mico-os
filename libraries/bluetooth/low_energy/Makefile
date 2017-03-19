#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

BLUETOOTH_LIB_TYPE := low_energy

NAME := BTE_$(BLUETOOTH_LIB_TYPE)
ALWAYS_OPTIMISE := 1

BUILD_BTE_LIB = 1
MINDRAY = 1             ####for mindray minimize bte size

SOURCE_ROOT :=../../../

BT_CHIP              := 20702
BT_CHIP_REVISION     := B0

BYPASS_LIBRARY_POISON_CHECK=1

include $(BLUETOOTH_LIB_TYPE)_src.mk

CFLAGS += -I.
CFLAGS += -I$(SOURCE_ROOT)include
CFLAGS += -I$(SOURCE_ROOT)tools/prebuild_stubs
CFLAGS += -I$(SOURCE_ROOT)libraries/utilities/ring_buffer
CFLAGS += -I$(SOURCE_ROOT)libraries/bluetooth/include
CFLAGS += -I$(SOURCE_ROOT)platform/include
CFLAGS += -I$(SOURCE_ROOT)MiCO/RTOS/FreeRTOS/mico/Cortex-M3
CFLAGS += -I$(SOURCE_ROOT)MICO/RTOS/FreeRTOS/mico
CFLAGS += -I$(SOURCE_ROOT)MICO/RTOS/FreeRTOS/ver7.1.0/Source/include
CFLAGS += -I$(SOURCE_ROOT)MICO/RTOS/FreeRTOS/ver7.1.0/Source/portable/GCC/ARM_CM3
CFLAGS += -I$(SOURCE_ROOT)MICO/RTOS/FreeRTOS/ver7.1.0/Source/


SOURCES := $($(NAME)_SOURCES)
CFLAGS += $(addprefix -I,$(GLOBAL_INCLUDES)) $(addprefix -D,$(GLOBAL_DEFINES)) $(addprefix -I,$($(NAME)_INCLUDES)) $(addprefix -D,$($(NAME)_DEFINES)) $($(NAME)_CFLAGS)


KEEP_LIST_DIR := $(SOURCE_ROOT)libraries/bluetooth_bte

KEEP_LIST:= $(NAME)*.a \
            bluetooth_bte.mk
            
clean_up: all
	$(QUIET)$(RM) -rf $(SOURCE_ROOT)/build/BTE

include $(SOURCE_ROOT)makefiles/mico_library_build.mk




