#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_MiCO_System_WAC

ifneq ($(wildcard $(CURDIR)Lib_MFi_WAC.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a),)
$(NAME)_PREBUILT_LIBRARY := Lib_MFi_WAC.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a
else
# Build from source
SRC_MAKEFILE := $(CURDIR)Lib_MFi_WAC_src.mk
ifeq ($(SRC_MAKEFILE), $(wildcard $(SRC_MAKEFILE)))
include $(SRC_MAKEFILE)
endif
endif

$(NAME)_COMPONENTS += drivers/MiCODriverMFiAuth

