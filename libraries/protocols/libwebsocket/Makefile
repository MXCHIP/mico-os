#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_libwebsocket
#ALWAYS_OPTIMISE := 1

SOURCE_ROOT :=../../../
#DEBUG := 1

BYPASS_LIBRARY_POISON_CHECK=1

include libwebsocket.mk

LWIP_DIR := $(SOURCE_ROOT)/MiCO/net/LwIP/ver1.4.0.rc1/src
SOURCES := $($(NAME)_SOURCES)
CFLAGS += $(addprefix -I,$(GLOBAL_INCLUDES)) $(addprefix -D,$(GLOBAL_DEFINES)) $(addprefix -I,$($(NAME)_INCLUDES)) $(addprefix -D,$($(NAME)_DEFINES)) $($(NAME)_CFLAGS)
CFLAGS += -I$(LWIP_DIR)/include -I$(LWIP_DIR)/include/ipv4 -I$(SOURCE_ROOT)/MiCO/net/LwIP/mico
CFLAGS += -I$(SOURCE_ROOT)/include

include $(SOURCE_ROOT)makefiles/mico_library_build.mk

