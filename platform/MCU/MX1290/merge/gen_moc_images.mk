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

EXTRA_POST_BUILD_TARGETS += gen_moc_images

#############################Generate MOC_KERNEL_APP_BIN_OUTPUT_FILE ################
#moc kernel
MOC_KERNEL_BIN_FILE := $(MICO_OS_PATH)/resources/moc_kernel/$(MODULE)/kernel.bin

#moc user application
MOC_APP_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.usr$(BIN_OUTPUT_SUFFIX))
MOC_APP_OFFSET      := 0x75000
######################################################################################


#MOC bootloader
MOC_BOOT_BIN_FILE    := $(MICO_OS_PATH)/resources/moc_kernel/$(MODULE)/boot.bin
MOC_BOOT_OFFSET      := 0x0

#moc kernel + user application
MOC_KERNEL_APP_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.ota$(BIN_OUTPUT_SUFFIX))
MOC_KERNEL_APP_OFFSET:= 0x13000

#ATE 
ifeq ($(MODULE),3080B_big)
MOC_ATE_BIN_OUTPUT_FILE := $(MICO_OS_PATH)/platform/MCU/MX1290/merge/ate_big.bin
MOC_ATE_OFFSET       := 0x00105000
else
MOC_ATE_BIN_OUTPUT_FILE := $(MICO_OS_PATH)/platform/MCU/MX1290/merge/ate.bin
MOC_ATE_OFFSET       := 0xD0000
endif

#wifi firmware
# WIFI_FIRMWARE_OFFSET := 0x1a0000


# Required to build Full binary file
GEN_MOC_BIN_OUTPUT_FILE_SCRIPT 	 := $(SCRIPTS_PATH)/gen_moc_bin_output_file.py
GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT:= $(SCRIPTS_PATH)/gen_common_bin_output_file.py

MOC_ALL_BIN_OUTPUT_FILE :=$(LINK_OUTPUT_FILE:$(LINK_OUTPUT_SUFFIX)=.all$(BIN_OUTPUT_SUFFIX))

gen_moc_images: build_done
	$(QUIET)$(ECHO) Generate MOC OTA Image: $(MOC_KERNEL_APP_BIN_OUTPUT_FILE)
	$(PYTHON) $(GEN_MOC_BIN_OUTPUT_FILE_SCRIPT) -u $(BIN_OUTPUT_FILE) -d $(MOC_APP_OFFSET) -k $(MOC_KERNEL_BIN_FILE)
	$(QUIET)$(ECHO) Generate MOC Flash Images: $(MOC_ALL_BIN_OUTPUT_FILE)
	$(QUIET)$(RM) $(MOC_ALL_BIN_OUTPUT_FILE)
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MOC_ALL_BIN_OUTPUT_FILE) -f $(MOC_BOOT_OFFSET)       $(MOC_BOOT_BIN_FILE)              
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MOC_ALL_BIN_OUTPUT_FILE) -f $(MOC_KERNEL_APP_OFFSET) $(MOC_KERNEL_APP_BIN_OUTPUT_FILE)
	$(PYTHON) $(GEN_COMMON_BIN_OUTPUT_FILE_SCRIPT) -o $(MOC_ALL_BIN_OUTPUT_FILE) -f $(MOC_ATE_OFFSET)        $(MOC_ATE_BIN_OUTPUT_FILE) 
	$(QUIET)$(ECHO_BLANK_LINE)





