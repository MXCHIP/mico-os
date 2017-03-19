#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_HomeKit_Server


GLOBAL_INCLUDES += .

ifneq ($(wildcard $(CURDIR)Lib_HomeKit_Server.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a),)
$(NAME)_PREBUILT_LIBRARY := Lib_HomeKit_Server.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a
else
# Build from source
include $(CURDIR)homekit_server_src.mk
endif


$(NAME)_COMPONENTS += drivers/MiCODriverMFiAuth
