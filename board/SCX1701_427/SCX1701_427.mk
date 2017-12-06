############################################################################### 
#
#  The MIT License
#  Copyright (c) 2017 MXCHIP Inc.
#
#  Permission is hereby granted, free of charge, to any person obtaining a copy 
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights 
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is furnished
#  to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included in
#  all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
#  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR 
#  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
############################################################################### 

NAME := Board_SCX1701_427

WLAN_CHIP            	:= 43362
WLAN_CHIP_REVISION   	:= A2
WLAN_CHIP_FAMILY     	:= 43362
WLAN_CHIP_FIRMWARE_VER  := 5.90.230.15

MODULE              	:= 1062
HOST_MCU_FAMILY      	:= STM32F4xx
HOST_MCU_VARIANT     	:= STM32F427
HOST_MCU_PART_NUMBER 	:= STM32F427VGT6

ifndef BUS
BUS := SDIO
endif

# Extra build target in mico_standard_targets.mk, include bootloader, and copy output file to eclipse debug file (copy_output_for_eclipse)
EXTRA_TARGET_MAKEFILES +=  $(MAKEFILES_PATH)/mico_standard_targets.mk

# Global includes
GLOBAL_INCLUDES  := .

GLOBAL_DEFINES += $$(if $$(NO_CRLF_STDIO_REPLACEMENT),,CRLF_STDIO_REPLACEMENT)
GLOBAL_LDFLAGS  += -L $(MICO_OS_PATH)/board/SCX1701_427

# Components
$(NAME)_COMPONENTS += drivers/spi_flash
$(NAME)_COMPONENTS += drivers/keypad/gpio_button

# Source files
$(NAME)_SOURCES := mico_board.c \
                   platform_eth.c

ifndef NO_WIFI_FIRMWARE
WIFI_FIRMWARE := $(MICO_OS_PATH)/resources/wifi_firmware/$(WLAN_CHIP)/$(WLAN_CHIP)$(WLAN_CHIP_REVISION)$(WLAN_CHIP_BIN_TYPE)-$(WLAN_CHIP_FIRMWARE_VER).bin
endif

###################################################################################################

# Add mbed support, add mbed target definitions here
MBED_SUPPORT 	        := 1
MBED_DEVICES            := ANALOGIN ANALOGOUT ERROR_RED I2C I2CSLAVE I2C_ASYNCH INTERRUPTIN PORTIN PORTINOUT PORTOUT PWMOUT RTC SERIAL SLEEP SPI SPISLAVE SPI_ASYNCH STDIO_MESSAGES SDIO
MBED_TARGETS            := STM STM32F4 STM32F427 STM32F429xI STM32F427VG

GLOBAL_DEFINES          += INITIAL_SP=(0x20020000UL)
GLOBAL_DEFINES          += OS_TASKCNT=14
GLOBAL_DEFINES          += OS_MAINSTKSIZE=256
GLOBAL_DEFINES          += OS_CLOCK=168000000
GLOBAL_DEFINES          += OS_ROBINTOUT=1
                           
GLOBAL_DEFINES += TRANSACTION_QUEUE_SIZE_SPI=2 USB_STM_HAL MXCHIP_LIBRARY HSE_VALUE=((uint32_t)25000000)

# Source files
$(NAME)_SOURCES += mbed/PeripheralPins.c \
                   mbed/system_stm32f4xx.c
                   
# Global includes
GLOBAL_INCLUDES  += mbed
