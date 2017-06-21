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

NAME := Board_EML3047

WLAN_CHIP            	:= NONE
WLAN_CHIP_REVISION   	:= NONE
WLAN_CHIP_FAMILY     	:= NONE
WLAN_CHIP_FIRMWARE_VER  := NONE

NO_WIFI_FIRMWARE := YES        

MODULE              	:= EML3047
HOST_MCU_FAMILY      	:= STM32L071KB
HOST_MCU_VARIANT     	:= STM32L071KB
HOST_MCU_PART_NUMBER 	:= 

# ifndef NO_WIFI_FIRMWARE
# WIFI_FIRMWARE := $(MICO_OS_PATH)/resources/wifi_firmware/$(WLAN_CHIP)/$(WLAN_CHIP)$(WLAN_CHIP_REVISION)$(WLAN_CHIP_BIN_TYPE)-$(WLAN_CHIP_FIRMWARE_VER).bin
# endif
BUS := EML3047

# # Extra build target in mico_standard_targets.mk, include bootloader, and copy output file to eclipse debug file (copy_output_for_eclipse)
EXTRA_TARGET_MAKEFILES +=  $(MAKEFILES_PATH)/mico_standard_targets_for_stm32l0xx.mk

# Global includes
GLOBAL_INCLUDES  := .

# Source files
$(NAME)_SOURCES := mico_board.c
# Global defines

GLOBAL_LDFLAGS := -L $(MICO_OS_PATH)/board/EML3047

# Components
$(NAME)_COMPONENTS := drivers/spi_flash \
                      drivers/keypad/gpio_button
                     
###################################################################################################

# Add mbed support, add mbed target definitions here
MBED_SUPPORT 	        := 1
MBED_DEVICES            := ANALOGIN ANALOGOUT I2C I2CSLAVE I2C_ASYNCH INTERRUPTIN LOWPOWERTIMER PORTIN PORTINOUT PORTOUT PWMOUT RTC SERIAL SERIAL_FC SERIAL_ASYNCH SLEEP SPI SPISLAVE SPI_ASYNCH STDIO_MESSAGES TRNG
MBED_TARGETS            := STM STM32L0 STM32L071KB

GLOBAL_DEFINES          += INITIAL_SP=(0x20005000UL)
GLOBAL_DEFINES          += OS_TASKCNT=14
GLOBAL_DEFINES          += OS_MAINSTKSIZE=256
GLOBAL_DEFINES          += OS_CLOCK=32000000
GLOBAL_DEFINES          += OS_ROBINTOUT=1
                           
GLOBAL_DEFINES += TRANSACTION_QUEUE_SIZE_SPI=2 USB_STM_HAL USBHOST_OTHER MXCHIP_LIBRARY

# Source files
$(NAME)_SOURCES += mbed/PeripheralPins.c \
                   mbed/system_stm32l0xx.c
                   
# Global includes
GLOBAL_INCLUDES  += mbed

VECT_TAB_OFFSET_APP := 0xC000


