#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := GCC

$(NAME)_SOURCES = mem_newlib.c math_newlib.c stdio_newlib.c time_newlib.c eabi.c

ifeq ($(MiCO_SDK_VERSION_MAJOR),3)
ifeq ($(MiCO_SDK_VERSION_MINOR),5)
ifneq ($(filter 0 1 2 3,$(MiCO_SDK_VERSION_REVISION)),)
$(NAME)_LINK_FILES := mem_newlib.o math_newlib.o stdio_newlib.o time_newlib.o eabi.o
endif
endif
endif

ifneq ($(filter $(subst ., ,$(COMPONENTS)),mocOS),)
GLOBAL_LDFLAGS += -Wl,-wrap,_malloc_r -Wl,-wrap,free -Wl,-wrap,realloc -Wl,-wrap,malloc -Wl,-wrap,calloc -Wl,-wrap,_free_r -Wl,-wrap,_realloc_r #-Wl,-wrap,printf -Wl,-wrap,sprintf
endif
