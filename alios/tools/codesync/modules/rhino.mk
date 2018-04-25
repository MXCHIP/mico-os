
NAME := rhino

VERSION_MAJOR    = $(word 1, $(subst ., ,$(VERSION)))
VERSION_MINOR    = $(word 1, $(subst ., ,$(VERSION)))
VERSION_REVISION = $(word 1, $(subst ., ,$(VERSION)))

$(NAME)_COMPONENTS += rhino

# Define some macros to allow for some network-specific checks
GLOBAL_DEFINES += RTOS_$(NAME)=1
GLOBAL_DEFINES += $(NAME)_VERSION_MAJOR=$(VERSION_MAJOR)
GLOBAL_DEFINES += $(NAME)_VERSION_MINOR=$(VERSION_MINOR)
GLOBAL_DEFINES += $(NAME)_VERSION_REVISION=$(VERSION_REVISION)
GLOBAL_DEFINES += HAVE_RHINO_KERNEL

GLOBAL_INCLUDES += core/include

GLOBAL_LDFLAGS +=

ifeq ($(HOST_ARCH),ARM968E-S)
$(NAME)_CFLAGS += -marm 
endif

$(NAME)_CFLAGS += -Wall -Werror
ifeq ($(findstring linuxhost, $(BUILD_STRING)), linuxhost)
$(NAME)_PREBUILT_LIBRARY := lib/linuxhost/librhino.a
else ifeq ($(findstring mk3060, $(BUILD_STRING)), mk3060)
$(NAME)_PREBUILT_LIBRARY := lib/mk3060/librhino.a
endif

