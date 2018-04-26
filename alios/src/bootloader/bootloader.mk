NAME := APP_bootloader

GLOBAL_INCLUDES := .

$(NAME)_SOURCES :=  main.c \
                    menu.c \
                    getline.c \
                    update_for_ota.c \
                    ymodem.c

GLOBAL_DEFINES := AOS_NO_WIFI

GLOBAL_LDFLAGS += $$(CLIB_LDFLAGS_NANO)

GLOBAL_DEFINES += BOOTLOADER
