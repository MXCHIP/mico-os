#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_Utilities

GLOBAL_INCLUDES += .

$(NAME)_SOURCES := AESUtils.c \
                   LinkListUtils.c \
                   SocketUtils.c \
                   HTTPUtils.c \
                   TimeUtils.c \
                   TLVUtils.c \
                   URLUtils.c
                   
ifeq ($(ALIOS_SUPPORT),y)
# AliOS + moc108 source codes
ifneq ($(filter moc108 rtl8710bn,$(HOST_MCU_FAMILY)),)

else
ifneq ($(filter stm32f4xx,$(HOST_MCU_FAMILY)),)
$(NAME)_SOURCES += StringUtils.c
else
# AliOS source codes
$(NAME)_SOURCES += RingBufferUtils.c \
                   StringUtils.c
endif #stm32f4xx
endif #moc108 rtl8710bn

else
# MiCO source codes
$(NAME)_SOURCES += CheckSumUtils.c \
                   RingBufferUtils.c \
                   StringUtils.c
endif
                   
$(NAME)_COMPONENTS += utilities/json_c

                   
$(NAME)_CFLAGS   += -Wno-char-subscripts



