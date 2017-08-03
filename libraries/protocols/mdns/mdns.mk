#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2017 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_mdns

GLOBAL_INCLUDES := include

$(NAME)_SOURCES := mdns_cli.c \
                   mdns_main.c \
                   mdns_query.c \
                   mdns_respond_thread.c \
                   mdns_respond.c \
                   mdns.c \
                   internal/debug.c \
                   internal/dname.c
                   
$(NAME)_INCLUDES := internal



#$(NAME)_CFLAGS  = $(COMPILER_SPECIFIC_PEDANTIC_CFLAGS)

#$(NAME)_ALWAYS_OPTIMISE := 1
