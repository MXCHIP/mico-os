
NAME := legacy_linkkitapp

GLOBAL_DEFINES      += ALIOT_DEBUG IOTX_DEBUG MQTT_DIRECT USE_LPTHREAD  FOTA_RAM_LIMIT_MODE
CONFIG_OTA_CH = linkkit
$(NAME)_SOURCES     := linkkit-example.c linkkit_app.c json.c thread.c

$(NAME)_COMPONENTS := protocol.legacy_linkkit protocol.alink-ilop connectivity.mqtt cjson fota netmgr framework.common  ywss4linkkit

LWIP := 0
ifeq ($(LWIP),1)
$(NAME)_COMPONENTS  += protocols.net
no_with_lwip := 0
endif
#ifeq ($(auto_netmgr),1)
GLOBAL_DEFINES += AUTO_NETMGR
#endif
