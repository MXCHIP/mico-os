NAME := ywss

$(NAME)_TYPE := framework
GLOBAL_INCLUDES += .
$(NAME)_SOURCES := awss.c enrollee.c sha256.c zconfig_utils.c zconfig_ieee80211.c wifimgr.c ywss_utils.c
$(NAME)_SOURCES += zconfig_ut_test.c registrar.c zconfig_protocol.c zconfig_vendor_common.c
ifeq ($(awss_ble),1)
$(NAME)_SOURCES += awss_blefi.c blefi_config.c
GLOBAL_DEFINES += CONFIG_AWSS_BLE
endif

$(NAME)_DEFINES += DEBUG

ifeq ($(COMPILER),)
$(NAME)_CFLAGS  += -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration
$(NAME)_CFLAGS  += -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized
$(NAME)_CFLAGS  += -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable
$(NAME)_CFLAGS  += -Wno-unused-value -Wno-strict-aliasing
else ifeq ($(COMPILER),gcc)
$(NAME)_CFLAGS  += -Wall -Werror -Wno-unused-variable -Wno-unused-parameter -Wno-implicit-function-declaration
$(NAME)_CFLAGS  += -Wno-type-limits -Wno-sign-compare -Wno-pointer-sign -Wno-uninitialized
$(NAME)_CFLAGS  += -Wno-return-type -Wno-unused-function -Wno-unused-but-set-variable
$(NAME)_CFLAGS  += -Wno-unused-value -Wno-strict-aliasing
endif

GLOBAL_DEFINES += CONFIG_YWSS
