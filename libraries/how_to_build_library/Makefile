#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_Helloworld

#Uncomment to disable debug log
#ALWAYS_OPTIMISE := 1

include how_to_build_library_src.mk

SOURCE_ROOT :=../../

# Directories to search Header files when build a static library, add if required
CFLAGS += -I$(SOURCE_ROOT)include \
          -I$(SOURCE_ROOT)mico/system \
          -I$(SOURCE_ROOT)mico/security \
          -I$(SOURCE_ROOT)libraries/drivers \
          -I$(SOURCE_ROOT)libraries/utilities


#Donot change below---------------------------------------------------------------------------
BYPASS_LIBRARY_POISON_CHECK=1

# Source codes needed to build a static library
SOURCES := $($(NAME)_SOURCES)

CFLAGS += $(addprefix -I,$(GLOBAL_INCLUDES)) \
          $(addprefix -D,$(GLOBAL_DEFINES)) \
          $(addprefix -I,$($(NAME)_INCLUDES)) \
          $(addprefix -D,$($(NAME)_DEFINES)) \
          $($(NAME)_CFLAGS)
include $(SOURCE_ROOT)makefiles/mico_library_build.mk

