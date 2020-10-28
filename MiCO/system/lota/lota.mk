NAME := lib_lota

ifneq ($(wildcard $(CURDIR)lota.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a),)
$(NAME)_PREBUILT_LIBRARY := lota.$(HOST_ARCH).$(TOOLCHAIN_NAME).release.a
else
# Build from source
include $(CURDIR)/lota_src.mk
endif

$(NAME)_SOURCES += lota_flash_port.c
