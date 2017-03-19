#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_Helloworld

ifneq ($(wildcard $(CURDIR)$(NAME).$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a),)
GLOBAL_INCLUDES := .
$(NAME)_PREBUILT_LIBRARY := $(NAME).$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a
else
include $(CURDIR)how_to_build_library_src.mk
endif
