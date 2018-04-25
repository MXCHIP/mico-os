NAME := dda

GLOBAL_INCLUDES += include

$(NAME)_INCLUDES += kernel/protocols/mesh/include

$(NAME)_SOURCES := eloop.c \
                   agent.c \
                   msg.c \
                   hal.c \
                   config_parser.c \
                   master.c

$(NAME)_CFLAGS += -Wall -Werror

GLOBAL_DEFINES += CONFIG_AOS_DDA
GLOBAL_DEFINES += CONFIG_AOS_DDM

GLOBAL_INCLUDES += include
