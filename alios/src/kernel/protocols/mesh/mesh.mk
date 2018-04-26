NAME := mesh

$(NAME)_TYPE := kernel
GLOBAL_INCLUDES += include

$(NAME)_INCLUDES += include

$(NAME)_COMPONENTS += yloop

$(NAME)_SOURCES := src/core/umesh.c \
                   src/core/mesh/mesh_mgmt.c \
                   src/core/mesh/network_mgmt.c \
                   src/core/mesh/link_mgmt.c \
                   src/core/mesh/network_data.c \
                   src/core/mesh/mesh_forwarder.c \
                   src/core/mesh/address_mgmt.c \
                   src/core/mesh/fragments.c \
                   src/core/mesh/mcast.c \
                   src/core/routing/router_mgr.c \
                   src/core/routing/sid_router.c \
                   src/core/routing/ssid_allocator.c \
                   src/core/security/keys_mgr.c \
                   src/ip/ip_address.c \
                   src/hal/interfaces.c \
                   src/hal/umesh_hal.c \
                   src/hal/wifi/80211_frame.c \
                   src/hal/wifi/wifi.c \
                   src/utilities/message.c \
                   src/utilities/timer.c \
                   src/utilities/memory.c \
                   src/utilities/configs.c \
                   src/utilities/maths.c \
                   src/utilities/mac_whitelist.c \
                   src/tools/cli.c

ifneq ($(SDK), 1)
$(NAME)_SOURCES += src/platform/aos.c
else
$(NAME)_SOURCES += src/platform/loop.c
endif

MESHDEBUG ?= 1
ifeq ($(MESHDEBUG), 1)
$(NAME)_SOURCES += src/tools/diags.c
$(NAME)_SOURCES += src/utilities/logging.c
GLOBAL_DEFINES += CONFIG_AOS_MESH_DEBUG
endif

MESHSUPER ?= 1
ifeq ($(MESHSUPER), 1)
$(NAME)_SOURCES += src/core/routing/vector_router.c
$(NAME)_SOURCES += src/core/routing/rsid_allocator.c
GLOBAL_DEFINES += CONFIG_AOS_MESH_SUPER
endif

MESHLOWPOWER ?= 0
ifeq ($(MESHLOWPOWER), 1)
$(NAME)_SOURCES += src/core/mesh/lowpower_mgmt.c
GLOBAL_DEFINES += CONFIG_AOS_MESH_LOWPOWER
endif

MESHAUTH ?= 0
ifeq ($(MESHAUTH), 1)
$(NAME)_SOURCES += src/core/security/auth_dot1x.c
$(NAME)_SOURCES += src/core/security/auth_eap.c
$(NAME)_SOURCES += src/core/security/auth_mgmt.c
$(NAME)_SOURCES += src/core/security/auth_relay.c
GLOBAL_DEFINES += CONFIG_AOS_MESH_AUTH
endif

ifeq ($(CONFIG_AOS_MESH_TAPIF), 1)
$(NAME)_SOURCES += src/ip/tapif_adapter.c
$(NAME)_DEFINES += CONFIG_AOS_MESH_TAPIF
endif

LWIP ?=1
ifeq ($(LWIP), 1)
$(NAME)_SOURCES += src/ip/lwip_adapter.c
$(NAME)_SOURCES += src/ip/compress6.c
$(NAME)_SOURCES += src/tools/cli_ip.c
else
$(NAME)_SOURCES += src/utilities/mem/pbuf.c
$(NAME)_SOURCES += src/utilities/mem/def.c
endif

#default gcc
ifeq ($(COMPILER),)
$(NAME)_CFLAGS      += -Wall -Werror
else ifeq ($(COMPILER),gcc)
$(NAME)_CFLAGS      += -Wall -Werror
endif

GLOBAL_DEFINES += CONFIG_AOS_MESH
