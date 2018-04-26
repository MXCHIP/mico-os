NAME := wsf

$(NAME)_INCLUDES += ../../protocol/alink/os/
$(NAME)_DEFINES += DEBUG

ifeq ($(findstring linuxhost, $(BUILD_STRING)), linuxhost)
$(NAME)_PREBUILT_LIBRARY := lib/linuxhost/libwsf.a
else ifeq ($(findstring mk3060, $(BUILD_STRING)), mk3060)
$(NAME)_PREBUILT_LIBRARY := lib/mk3060/libwsf.a
endif
