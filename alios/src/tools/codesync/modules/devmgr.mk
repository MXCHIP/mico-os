NAME := devmgr

GLOBAL_INCLUDES += ./

$(NAME)_INCLUDES := ../ ../system/ ../os/ ../json/ ../accs/ ../stdd/
$(NAME)_INCLUDES +=  ../../../../framework/connectivity/ ../../../../framework/connectivity/wsf/ ../../../ywss/
$(NAME)_INCLUDES += ../../../../utility/digest_algorithm/
$(NAME)_CFLAGS += -DGATEWAY_SDK

ifeq ($(findstring linuxhost, $(BUILD_STRING)), linuxhost)
$(NAME)_PREBUILT_LIBRARY := lib/linuxhost/libdevmgr.a
else ifeq ($(findstring mk3060, $(BUILD_STRING)), mk3060)
$(NAME)_PREBUILT_LIBRARY := lib/mk3060/libdevmgr.a
endif

