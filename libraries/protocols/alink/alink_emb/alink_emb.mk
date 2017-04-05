#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_Alink_emb

GLOBAL_INCLUDES := .

$(NAME)_SOURCES := alink_aws.c alink_aws_easylink.c

ifeq ($(HOST_ARCH),Cortex-M4F)
$(NAME)_PREBUILT_LIBRARY := lib_alink_emb_fpu.a
else
$(NAME)_PREBUILT_LIBRARY := lib_alink_emb.a
endif

$(NAME)_COMPONENTS := protocols/alink/alink_common
