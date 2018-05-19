#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2018 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := alios_crypto



ifeq ($(ALIOS_NATIVE_APP),y)
$(NAME)_COMPONENTS := alicrypto
$(NAME)_SOURCES := md5_wrap.c
else
$(NAME)_COMPONENTS := wolfSSL
endif
