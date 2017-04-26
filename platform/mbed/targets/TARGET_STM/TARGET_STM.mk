#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2017 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = stm

SOURCE_DIR := ../../mbed-os/targets/TARGET_STM

SOURCE_SRC   := $(notdir $(wildcard $(CURDIR)/$(SOURCE_DIR)/*.c))

$(NAME)_SOURCES := $(foreach code, $(SOURCE_SRC), $(addprefix $(SOURCE_DIR)/,$(code)))

GLOBAL_INCLUDES := $(SOURCE_DIR)