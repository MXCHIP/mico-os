#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_Sodium
ALWAYS_OPTIMISE := 1

SOURCE_ROOT :=../../../

BYPASS_LIBRARY_POISON_CHECK=1

include Sodium_src.mk


CFLAGS += -I$(SOURCE_ROOT)include \
          -I$(SOURCE_ROOT)mico/security \
          -I$(SOURCE_ROOT)libraries/utilities

SOURCES := $($(NAME)_SOURCES)
CFLAGS += $(addprefix -I,$(GLOBAL_INCLUDES)) $(addprefix -D,$(GLOBAL_DEFINES)) $(addprefix -I,$($(NAME)_INCLUDES)) $(addprefix -D,$($(NAME)_DEFINES)) $($(NAME)_CFLAGS)


KEEP_LIST_DIR := $(SOURCE_ROOT)mico/security/Sodium

KEEP_LIST:= $(NAME)*.a \
            Sodium.mk

include $(SOURCE_ROOT)makefiles/mico_library_build.mk

