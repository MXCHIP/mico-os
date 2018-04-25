NAME := mqttest

GLOBAL_DEFINES      += ALIOT_DEBUG IOTX_DEBUG
CONFIG_OTA_CH = mqtt
ifeq ($(STM32_NONSTD_SOCKET), true)
$(NAME)_SOURCES     := mqttest-b_l475e.c
else
$(NAME)_SOURCES     := mqttest.c
endif

$(NAME)_COMPONENTS := cli connectivity.mqtt cjson fota netmgr framework.common

LWIP := 0
ifeq ($(LWIP),1)
$(NAME)_COMPONENTS  += protocols.net
no_with_lwip := 0
endif

