#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#


NAME = aws


GLOBAL_INCLUDES := . 


ifneq ($(wildcard $(CURDIR)aws.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a),)
$(NAME)_PREBUILT_LIBRARY += aws.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a
else
include $(CURDIR)/aws/aws.mk
endif


