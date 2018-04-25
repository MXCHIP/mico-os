#!/bin/bash

# install dependence
sudo apt-get install -y libusb-dev
sudo apt-get install -y libhidapi-dev 

# add no-root user privilege to acess USB device such as Jlink
echo 'SUBSYSTEM=="usb",GROUP="users",MODE="0666"' | sudo tee -a /etc/udev/rules.d/90-usbpermission.rules
sudo udevadm trigger
touch build/OpenOCD/beken/beken_gdb_jtag.cfg
