#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := NoRTOS

GLOBAL_DEFINES += NO_MICO_RTOS

GLOBAL_INCLUDES := .

$(NAME)_SOURCES := rtos.c


ifneq ($(MBED_SUPPORT),)
$(NAME)_SOURCES += mbed_main.cpp
$(NAME)_LINK_FILES := mbed_main.o
else
$(NAME)_SOURCES += mico_main.c
endif
