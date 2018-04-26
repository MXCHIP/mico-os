#!/usr/bin/env bash

sudo apt-get update
sudo apt-get -y upgrade

#install testbed related files
sudo apt-get -y install vim mutt msmtp
sudo apt-get -y install python-pip git screen
sudo pip install pyserial
sudo usermod -a -G dialout $(whoami)

cwd=`pwd`
if [ ! -d ~/tools ]; then
    mkdir ~/tools
fi
cd ~/tools
git clone https://github.com/espressif/esptool.git
echo LC_ALL=\"en_US.UTF-8\" >> ~/.bashrc
echo "export ESPTOOL_PATH=~/tools/esptool" >> ~/.bashrc

cd $cwd
cp msmtprc ~/.msmtprc
chmod 400 ~/.msmtprc
cp muttrc ~/.muttrc
chmod 400 ~/.muttrc
sudo cp msmtprc /root/.msmtprc
sudo chown root:root /root/.msmtprc
sudo chmod 400 /root/.msmtprc
sudo cp muttrc /root/.muttrc
sudo chown root:root /root/.muttrc
sudo chmod 400 /root/.muttrc
sudo cp send-ip-mail.sh /root/
sudo chown root:root /root/send-ip-mail.sh
sudo cp rc.local /etc/rc.local
sudo chown root:root /etc/rc.local
sudo cp 98-usb-serial.rules /etc/udev/rules.d/
sudo cp tb*_st* /usr/local/bin/
sudo chmod +x /usr/local/bin/tb*_st*

#install aos build dependent packages
sudo apt-get -y install gcc-multilib
sudo apt-get -y install libssl-dev libssl-dev:i386
sudo apt-get -y install libncurses5-dev libncurses5-dev:i386
sudo apt-get -y install libreadline-dev libreadline-dev:i386
sudo pip install aos-cube

#install extensa toolchain
sudo apt-get -y install wget make flex bison gperf
cd ${HOME}/tools
if [ ! -d xtensa-esp32-elf ]; then
    arch=`uname -m`
    if [ "${arch}" = "x86_64" ]; then
        wget https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-73-ge28a011-5.2.0.tar.gz
        tar xzf xtensa-esp32-elf-linux64-1.22.0-73-ge28a011-5.2.0.tar.gz
        rm -f xtensa-esp32-elf-linux64-1.22.0-73-ge28a011-5.2.0.tar.gz
    else
        wget https://dl.espressif.com/dl/xtensa-esp32-elf-linux32-1.22.0-73-ge28a011-5.2.0.tar.gz
        tar xzf xtensa-esp32-elf-linux32-1.22.0-73-ge28a011-5.2.0.tar.gz
        rm -f xtensa-esp32-elf-linux32-1.22.0-73-ge28a011-5.2.0.tar.gz
    fi
fi
exist=`cat ~/.bashrc | grep xtensa-esp32-elf/bin`
if [ "${exist}" = "" ]; then
    echo "export PATH=\"\$HOME/tools/xtensa-esp32-elf/bin:\$PATH\"" >> ~/.bashrc
fi

if [ ! -d xtensa-lx106-elf ]; then
    arch=`uname -m`
    if [ "${arch}" = "x86_64" ]; then
        wget http://arduino.esp8266.com/linux64-xtensa-lx106-elf.tar.gz
        tar xzf linux64-xtensa-lx106-elf.tar.gz
        rm -f linux64-xtensa-lx106-elf.tar.gz
    else
        wget http://arduino.esp8266.com/linux32-xtensa-lx106-elf.tar.gz
        tar xzf linux32-xtensa-lx106-elf.tar.gz
        rm -f linux32-xtensa-lx106-elf.tar.gz
    fi
fi
exist=`cat ~/.bashrc | grep xtensa-lx106-elf/bin`
if [ "${exist}" = "" ]; then
    echo "export PATH=\"\$HOME/tools/xtensa-lx106-elf/bin:\$PATH\"" >> ~/.bashrc
fi

#add alink test server to /etc/hosts
exist=`cat /etc/hosts | grep pre-iotx-qs.alibaba.com`
if [ "${exist}" = "" ]; then
    sudo echo "140.205.173.181 pre-iotx-qs.alibaba.com" >> /etc/hosts
fi

#install java
#exist=`ls ~/tools | grep jdk`
#if [ "${exist}" = "" ]; then
#    wget /path/to/jdk
#    unzip jdk.zip
#fi
#exist=`cat ~/.bashrc | grep "jdk*/bin"`
#if [ "${exist}" = "" ]; then
#    echo "PATH=\"\$PATH:\$HOME/tools/jdk1.8.0_151/bin\"" >> ~/.bashrc
#fi

#install CloudSparrow
if [ ! -d CloudSparrow ]; then
    wget http://cloudsparrow.oss-cn-beijing.aliyuncs.com/resources/CloudSparrow.zip
    unzip CloudSparrow.zip
    rm -f CloudSparrow.zip
fi
exist=`cat ~/.bashrc | grep CloudSparrow/ADB/linux`
if [ "${exist}" = "" ]; then
    echo "export PATH=\"\$HOME/tools/CloudSparrow/ADB/linux:\$PATH\"" >> ~/.bashrc
fi



