############################################################################### 
#
#  The MIT License
#  Copyright (c) 2016 MXCHIP Inc.
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

NAME := Board_MK3165

WLAN_CHIP            	:= 43362
WLAN_CHIP_REVISION   	:= A2
WLAN_CHIP_FAMILY     	:= 43362
WLAN_CHIP_FIRMWARE_VER  := 5.90.230.15

MODULE              	:= 3165
HOST_MCU_FAMILY      	:= STM32F4xx
HOST_MCU_VARIANT     	:= STM32F411
HOST_MCU_PART_NUMBER 	:= STM32F411CEY6

ifndef NO_WIFI_FIRMWARE
WIFI_FIRMWARE := $(MICO_OS_PATH)/resources/wifi_firmware/$(WLAN_CHIP)/$(WLAN_CHIP)$(WLAN_CHIP_REVISION)$(WLAN_CHIP_BIN_TYPE)-$(WLAN_CHIP_FIRMWARE_VER).bin
endif

BUS := SDIO

# Extra build target in mico_standard_targets.mk, include bootloader, and copy output file to eclipse debug file (copy_output_for_eclipse)
EXTRA_TARGET_MAKEFILES +=  $(MAKEFILES_PATH)/mico_standard_targets.mk

# Global includes
GLOBAL_INCLUDES  := .

# Source files
$(NAME)_SOURCES := mico_board.c

# Global defines
GLOBAL_LDFLAGS  := -L $(MICO_OS_PATH)/board/MK$(MODULE)mbed

# Components
$(NAME)_COMPONENTS := drivers/spi_flash \
                      drivers/keypad/gpio_button


###################################################################################################

# Add mbed support, add mbed target definitions here
MBED_SUPPORT 	        := 1
MBED_DEVICES            := ANALOGIN ERROR_RED I2C I2CSLAVE I2C_ASYNCH INTERRUPTIN LOWPOWERTIMER PORTIN PORTINOUT PORTOUT PWMOUT RTC SERIAL SERIAL_ASYNCH SERIAL_FC SLEEP SPI SPISLAVE SPI_ASYNCH STDIO_MESSAGES SDIO
MBED_TARGETS            := STM STM32F4 STM32F411xE

GLOBAL_DEFINES          += INITIAL_SP=(0x20040000UL)
GLOBAL_DEFINES          += OS_TASKCNT=14
GLOBAL_DEFINES          += OS_MAINSTKSIZE=256
GLOBAL_DEFINES          += OS_CLOCK=96000000
GLOBAL_DEFINES          += OS_ROBINTOUT=1
GLOBAL_DEFINES          += CLOCK_SOURCE=USE_PLL_HSE_XTAL CLOCK_SOURCE_USB=1
                           
GLOBAL_DEFINES += TRANSACTION_QUEUE_SIZE_SPI=2 USB_STM_HAL USBHOST_OTHER MXCHIP_LIBRARY HSE_VALUE=((uint32_t)26000000)

# Source files
$(NAME)_SOURCES += mbed/PeripheralPins.c \
                   mbed/system_clock.c
                   
# Global includes
GLOBAL_INCLUDES  += mbed

# EMW3165 legacy configuration
VECT_TAB_OFFSET_APP := 0xC000

