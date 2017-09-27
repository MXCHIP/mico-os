#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME := Lib_MiCO_Kernel

GLOBAL_INCLUDES := 

WLAN_RF_MODULE_LIST := 1062 1088

MODULE_LIST := 3165 3166 3238 3239 3297

ifneq ($(filter $(MODULE),$(WLAN_RF_MODULE_LIST)),)
MICO_PREBUILT_LIBRARY := MiCO.$(MODULE).$(BUS).$(HOST_ARCH).$(TOOLCHAIN_NAME).a
#MICO_PREBUILT_LIBRARY := ../../../../../mico_source/branch_for_merge/output/MiCO.$(MODULE).$(BUS).$(HOST_ARCH).$(TOOLCHAIN_NAME).a
else
ifneq ($(filter $(MODULE),$(MODULE_LIST)),)
MICO_PREBUILT_LIBRARY := MiCO.$(MODULE).$(TOOLCHAIN_NAME).a
#MICO_PREBUILT_LIBRARY := ../../../../../mico_source/branch_for_merge/output/MiCO.$(MODULE).$(TOOLCHAIN_NAME).a
endif
endif


ifneq ($(filter $(subst ., ,$(COMPONENTS)),mocOS mocIP),)
$(info MiCO core based on MOC ! )
$(NAME)_SOURCES += ../../platform/MCU/$(HOST_MCU_FAMILY)/moc/moc_api.c 
                   
GLOBAL_INCLUDES += . \
                   ../../platform/MCU/$(HOST_MCU_FAMILY)/moc
                   
GLOBAL_DEFINES += MOC=1

GLOBAL_LDFLAGS   += $$(CLIB_LDFLAGS_NANO_FLOAT)
else
$(info MiCO core based on pre-build library: ===$(MICO_PREBUILT_LIBRARY)=== )
ifneq ($(wildcard $(CURDIR)$(MICO_PREBUILT_LIBRARY)),)
$(NAME)_PREBUILT_LIBRARY := $(MICO_PREBUILT_LIBRARY)
$(NAME)_SOURCES += rf_driver/wlan_firmware.c
else
# Build from source (MXCHIP internal)
$(info MiCO core based on source code ! $(MICO_PREBUILT_LIBRARY) is not found! )
endif #ifneq ($(wildcard $(CURDIR)$(MICO_PREBUILT_LIBRARY)),)
endif #ifneq ($(filter $(MODULE),$(MOC_MODULE_LIST)),)

