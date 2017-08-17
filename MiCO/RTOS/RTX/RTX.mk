#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := RTX

VERSION := 1.0.0

VERSION_MAJOR 	= $(word 1, $(subst ., ,$(VERSION)))
VERSION_MINOR 	= $(word 2, $(subst ., ,$(VERSION)))
VERSION_REVISION= $(word 3, $(subst ., ,$(VERSION)))

RTX_DIR   := ../../../platform/mbed/mbed-os/rtos/rtx/TARGET_CORTEX_M

ifneq ($(filter $(HOST_ARCH),Cortex-M4 Cortex-M4F Cortex-M7),)
$(NAME)_SOURCES := $(RTX_DIR)/TARGET_RTOS_M4_M7/TOOLCHAIN_GCC/HAL_CM4.S \
                   $(RTX_DIR)/TARGET_RTOS_M4_M7/TOOLCHAIN_GCC/SVC_Table.S
GLOBAL_DEFINES := TARGET_RTOS_M4_M7
endif


ifneq ($(filter $(HOST_ARCH),Cortex-M3),)
$(NAME)_SOURCES := $(RTX_DIR)/TARGET_M3/TOOLCHAIN_GCC/HAL_CM3.S \
                   $(RTX_DIR)/TARGET_M3/TOOLCHAIN_GCC/SVC_Table.S
GLOBAL_DEFINES := TARGET_RTOS_M3
endif

ifneq ($(filter $(HOST_ARCH),Cortex-M0),)
$(NAME)_SOURCES := $(RTX_DIR)/TARGET_M0/TOOLCHAIN_GCC/HAL_CM0.S \
                   $(RTX_DIR)/TARGET_M0/TOOLCHAIN_GCC/SVC_Table.S
GLOBAL_DEFINES := TARGET_RTOS_M0
endif

ifneq ($(filter $(HOST_ARCH),Cortex-M0plus),)
$(NAME)_SOURCES := $(RTX_DIR)/TARGET_M0P/TOOLCHAIN_GCC/HAL_CM0.S \
                   $(RTX_DIR)/TARGET_M0P/TOOLCHAIN_GCC/SVC_Table.S
GLOBAL_DEFINES := TARGET_RTOS_M0P
endif


$(NAME)_SOURCES += $(RTX_DIR)/HAL_CM.c \
                   $(RTX_DIR)/rt_CMSIS.c \
                   $(RTX_DIR)/rt_Event.c \
                   $(RTX_DIR)/rt_List.c \
                   $(RTX_DIR)/rt_Mailbox.c \
                   $(RTX_DIR)/rt_MemBox.c \
                   $(RTX_DIR)/rt_Mutex.c \
                   $(RTX_DIR)/rt_OsEventObserver.c \
                   $(RTX_DIR)/rt_Robin.c \
                   $(RTX_DIR)/rt_Semaphore.c \
                   $(RTX_DIR)/rt_System.c \
                   $(RTX_DIR)/rt_Task.c \
                   $(RTX_DIR)/rt_Time.c \
                   $(RTX_DIR)/rt_Timer.c \
                   $(RTX_DIR)/RTX_Conf_CM.c

GLOBAL_INCLUDES := $(RTX_DIR)

GLOBAL_DEFINES += $(NAME)_VERSION=$$(SLASH_QUOTE_START)v$(VERSION)$$(SLASH_QUOTE_END) \
                  MBED_CONF_RTOS_PRESENT \
                  __CMSIS_RTOS

$(NAME)_COMPONENTS := RTX/mico



