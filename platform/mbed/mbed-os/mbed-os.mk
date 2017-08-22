#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

NAME = mbed-os

# mbed device drivers
$(NAME)_SOURCES := drivers/AnalogIn.cpp \
                   drivers/BusIn.cpp \
                   drivers/BusInOut.cpp \
                   drivers/CAN.cpp \
                   drivers/Ethernet.cpp \
                   drivers/FileBase.cpp \
                   drivers/FilePath.cpp \
                   drivers/FileSystemLike.cpp \
                   drivers/FlashIAP.cpp \
                   drivers/I2C.cpp \
                   drivers/I2CSlave.cpp \
                   drivers/InterruptIn.cpp \
                   drivers/InterruptManager.cpp \
                   drivers/LocalFileSystem.cpp \
                   drivers/RawSerial.cpp \
                   drivers/Serial.cpp \
                   drivers/SerialBase.cpp \
                   drivers/SPI.cpp \
                   drivers/SPISlave.cpp \
                   drivers/Stream.cpp \
                   drivers/Ticker.cpp \
                   drivers/Timeout.cpp \
                   drivers/Timer.cpp \
                   drivers/TimerEvent.cpp
                   
# mbed hal
$(NAME)_SOURCES += hal/mbed_gpio.c \
                   hal/mbed_lp_ticker_api.c \
                   hal/mbed_pinmap_common.c \
                   hal/mbed_ticker_api.c \
                   hal/mbed_us_ticker_api.c
                   
                                 
# mbed platform
$(NAME)_SOURCES += platform/CallChain.cpp \
                   platform/mbed_alloc_wrappers.cpp \
                   platform/mbed_application.c \
                   platform/mbed_assert.c \
                   platform/mbed_board.c \
                   platform/mbed_critical.c \
                   platform/mbed_error.c \
                   platform/mbed_interface.c \
                   platform/mbed_mem_trace.c \
                   platform/mbed_retarget.cpp \
                   platform/mbed_rtc_time.cpp \
                   platform/mbed_semihost_api.c \
                   platform/mbed_stats.c \
                   platform/mbed_wait_api_no_rtos.c \
                   platform/mbed_wait_api_rtos.cpp \
                   
# mbed rtos
ifeq ($(RTOS),RTX)
$(NAME)_SOURCES += rtos/Mutex.cpp \
                   rtos/RtosTimer.cpp \
                   rtos/Semaphore.cpp \
                   rtos/Thread.cpp \
                   rtos/rtos_idle.c
GLOBAL_INCLUDES += rtos
endif
                   
GLOBAL_DEFINES  := MBED_CONF_PLATFORM_STDIO_BAUD_RATE=115200 \
                   MBED_CONF_PLATFORM_STDIO_FLUSH_AT_EXIT=1 \
                   MBED_CONF_PLATFORM_DEFAULT_SERIAL_BAUD_RATE=9600
                   
#                  MBED_CONF_PLATFORM_STDIO_CONVERT_NEWLINES \#

GLOBAL_INCLUDES += . cmsis drivers hal platform

GLOBAL_DEFINES += $(foreach dev, $(MBED_DEVICES), DEVICE_$(dev)) \
                  __MBED__=1 \
                  
GLOBAL_LDFLAGS  +=-Wl,--wrap,main -Wl,--wrap,_malloc_r -Wl,--wrap,_free_r -Wl,--wrap,_realloc_r -Wl,--wrap,_calloc_r -Wl,--wrap,exit -Wl,--wrap,atexit
GLOBAL_LDFLAGS  +=-Wl,--start-group -lstdc++ -lsupc++ -lm -lc -lgcc -lnosys -Wl,--end-group
                  




                   