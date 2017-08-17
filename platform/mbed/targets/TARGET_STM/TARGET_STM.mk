#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2017 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = stm

###############################################
# Use abslute path to reference mico-os codes #
###############################################

SOURCE_DIR := mico-os/platform/mbed/mbed-os/targets/TARGET_STM

SOURCE_SRC := $(notdir $(wildcard $(SOURCE_DIR)/*.c))

$(NAME)_ABS_SOURCES := $(foreach code, $(SOURCE_SRC), $(addprefix $(SOURCE_DIR)/,$(code)))

GLOBAL_ABS_INCLUDES := $(SOURCE_DIR)