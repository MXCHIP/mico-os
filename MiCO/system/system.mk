#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = Lib_MiCO_System


$(NAME)_SOURCES := mico_system_init.c \
                   mico_system_monitor.c \
                   mico_system_notification.c \
                   mico_system_para_storage.c \
                   mico_system_time.c \
                   mico_system_power_daemon.c \
                   mico_filesystem.c \
                   system_misc.c 
                   
$(NAME)_SOURCES  += command_console/mico_cli.c
$(NAME)_INCLUDES += command_console

$(NAME)_SOURCES += config_server/config_server_menu.c \
                   config_server/config_server.c
                   
$(NAME)_SOURCES += easylink/system_easylink_delegate.c \
                   easylink/system_easylink_wac.c \
                   easylink/system_easylink.c \
                   easylink/system_easylink_usr.c \
                   easylink/system_easylink_softap.c \
                   easylink/system_easylink_monitor.c \
                   easylink/internal/easylink_bonjour.c
                   
$(NAME)_INCLUDES += easylink/internal


$(NAME)_SOURCES += mdns/mico_mdns.c \
                   mdns/system_discovery.c
                               
$(NAME)_SOURCES += tftp_ota/tftp_ota.c \
                   tftp_ota/tftpc.c
                   

$(NAME)_COMPONENTS := system/qc_test system/easylink/MFi_WAC

ifneq ($(DISABLE_MICO_AWS),1)
$(NAME)_SOURCES += easylink/system_aws.c
$(NAME)_COMPONENTS += system/easylink/aws
$(NAME)_DEFINES += CONFIG_MICO_AWS
endif

$(NAME)_COMPONENTS += system/kv