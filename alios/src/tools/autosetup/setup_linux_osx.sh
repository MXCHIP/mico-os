#!/bin/bash

OS=`uname -s`

if [ "$OS" != "Darwin" ] && [ "$OS" != "Linux" ]; then
    echo "error: unsupported OS $OS"
    exit 1
fi

echo -n "Input which directory you want to put code (default:~/code/aos):"
read OBJ_DIR
if [ "${OBJ_DIR}" = "" ]; then
    OBJ_DIR=~/code/aos
fi

echo "Object directory is ${OBJ_DIR}"
if [ -d ${OBJ_DIR} ];then
    echo -n "Object directory already exist，overwrite(Y/N)?:"
    read -n 1 OVERWRITE
    if [ "${OVERWRITE}" != "Y" ] && [ "${OVERWRITE}" != "y" ];then
        exit 0
    fi
    rm -rf ${OBJ_DIR}
fi
mkdir -p ${OBJ_DIR}
if [ $? -ne 0 ]; then
    echo "error: create directory ${OBJ_DIR} failed"
    exit 1
fi

echo ""
echo -n "install dependent software packages ..."
if [ "$OS" = "Darwin" ]; then
    if [ "`which brew`" = "" ]; then
        /usr/bin/ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"
    fi
    if [ "`which pip`" = "" ];then
        sudo easy_install pip > /dev/null
    fi
    if [ "`which gcc`" = "" ];then
        brew install gcc > /dev/null
    fi
    if [ "`which wget`" = "" ];then
        brew install wget > /dev/null
    fi
    if [ "`which axel`" = "" ];then
        brew install axel > /dev/null
    fi
    GCC_ARM_URL="https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-mac.tar.bz2"
    GCC_XTENSA_ESP32_URL="https://dl.espressif.com/dl/xtensa-esp32-elf-osx-1.22.0-75-gbaf03c2-5.2.0.tar.gz"
    GCC_XTENSA_ESP8266_URL="http://arduino.esp8266.com/osx-xtensa-lx106-elf.tar.gz"
    HOST_OS=OSX
    BASHRC_FILE=.profile
    PIP_CMD=pip
else #Some Linux version
    GCC_ARM_URL="https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-linux.tar.bz2"
    BASHRC_FILE=.bashrc
    PIP_CMD=pip
    if [ "`uname -m`" = "x86_64" ]; then
        GCC_XTENSA_ESP32_URL="https://dl.espressif.com/dl/xtensa-esp32-elf-linux64-1.22.0-75-gbaf03c2-5.2.0.tar.gz"
	GCC_XTENSA_ESP8266_URL="http://arduino.esp8266.com/linux64-xtensa-lx106-elf.tar.gz"
        HOST_OS=Linux64
        if [ "`which apt-get`" != "" ]; then
            sudo apt-get update > /dev/null
            sudo apt-get -y install git wget axel make flex bison gperf unzip python-pip > /dev/null
            sudo apt-get -y install gcc-multilib > /dev/null
            sudo apt-get -y install libssl-dev libssl-dev:i386 > /dev/null
            sudo apt-get -y install libncurses-dev libncurses-dev:i386 > /dev/null
            sudo apt-get -y install libreadline-dev libreadline-dev:i386 > /dev/null
        elif [ "`which yum`" != "" ]; then
            sudo yum -y install git wget make flex bison gperf unzip python-pip > /dev/null
            sudo yum -y install gcc gcc-c++ glibc-devel glibc-devel.i686 libgcc libgcc.i686 > /dev/null
            sudo yum -y install libstdc++-devel libstdc++-devel.i686 > /dev/null
            sudo yum -y install openssl-devel openssl-devel.i686 > /dev/null
            sudo yum -y install ncurses-devel ncurses-devel.i686 > /dev/null
            sudo yum -y install readline-devel readline-devel.i686 > /dev/null
        elif [ "`which pacman`" != "" ]; then
            sudo pacman -Sy
            sudo pacman -S --needed gcc git wget axel make ncurses flex bison gperf unzip python2-pip > /dev/null
            PIP_CMD=pip2
        else
            echo "error: unknown package manerger"
            exit 1
        fi
    else
        GCC_XTENSA_ESP32_URL="https://dl.espressif.com/dl/xtensa-esp32-elf-linux32-1.22.0-75-gbaf03c2-5.2.0.tar.gz"
	GCC_XTENSA_ESP8266_URL="http://arduino.esp8266.com/linux32-xtensa-lx106-elf.tar.gz"
        HOST_OS=Linux32
        if [ "`which apt-get`" != "" ]; then
            sudo apt-get -y install git wget axel make flex bison gperf unzip python-pip > /dev/null
            sudo apt-get -y install libssl-dev libncurses-dev libreadline-dev > /dev/null
        elif [ "`which yum`" != "" ]; then
            sudo yum -y install git wget make flex bison gperf unzip python-pip > /dev/null
            sudo yum -y install gcc gcc-c++ glibc-devel libgcc libstdc++-devel > /dev/null
            sudo yum -y install openssl-devel ncurses-devel readline-devel > /dev/null
        elif [ "`which pacman`" != "" ]; then
            sudo pacman -Sy
            sudo pacman -S --needed gcc git wget axel make ncurses flex bison gperf unzip python2-pip > /dev/null
            PIP_CMD=pip2
        else
            echo -e "\nerror: unknown package manerger\n"
            exit 1
        fi
    fi
fi
sudo ${PIP_CMD} install --upgrade pyserial aos-cube > /dev/null 2>&1
echo "done"

cd ${OBJ_DIR}/../
rm -rf ${OBJ_DIR}
echo -n "cloning AliOS Things code..."
git clone https://github.com/alibaba/AliOS-Things ${OBJ_DIR} > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo -e "\nerror: clone code failed\n"
    exit 1
fi
echo "done"

echo -n "installing gcc-arm-none-eabi toolchain ..."
ARM_TOOLCHAIN_VERSION=5_4-2016q3-20160926
ARM_TOOLCHAIN_PATH=${OBJ_DIR}/build/compiler/arm-none-eabi-${ARM_TOOLCHAIN_VERSION}
if [ ! -d ${ARM_TOOLCHAIN_PATH} ]; then
    mkdir -p ${ARM_TOOLCHAIN_PATH}
fi
cd ${ARM_TOOLCHAIN_PATH}
if [ "`which axel`" = "" ];then
    wget -q ${GCC_ARM_URL}
    result=$?
else
    axel -q -n 8 ${GCC_ARM_URL}
    result=$?
fi
if [ ${result} -ne 0 ]; then
    echo -e "\nerror: download gcc-arm-none-eabi toolchain faield\n"
    exit 1
fi
tar jxf gcc-arm-none-eabi*.tar.bz2
if [ $? -ne 0 ]; then
    echo -e "\nerror: extract gcc-arm-none-eabi toolchain faield\n"
    exit 1
fi
rm -f gcc-arm-none-eabi*.tar.bz2
mv gcc-arm-none-eabi* ${HOST_OS}
exist=`cat ~/${BASHRC_FILE} | grep arm-none-eabi`
if [ "${exist}" = "" ]; then
    echo "export PATH=\"${ARM_TOOLCHAIN_PATH}/${HOST_OS}/bin:\$PATH\"" >> ~/${BASHRC_FILE}
fi
echo "done"

echo -n "installing gcc-xtensa-esp32 toolchain ..."
XTENSA_TOOLCHAIN_PATH=${OBJ_DIR}/build/compiler/gcc-xtensa-esp32
if [ ! -d ${XTENSA_TOOLCHAIN_PATH} ]; then
    mkdir -p ${XTENSA_TOOLCHAIN_PATH}
fi
cd ${XTENSA_TOOLCHAIN_PATH}
wget -q ${GCC_XTENSA_ESP32_URL}
if [ $? -ne 0 ]; then
    echo -e "\nerror: download gcc-xtensa-esp32 toolchain faield\n"
    exit 1
fi
tar zxf xtensa-esp32-elf*.tar.gz
if [ $? -ne 0 ]; then
    echo -e "\nerror: extract gcc-xtensa-esp32 toolchain faield\n"
    exit 1
fi
rm -f xtensa-esp32-elf*.tar.gz
mv xtensa-esp32-elf* ${HOST_OS}
exist=`cat ~/${BASHRC_FILE} | grep gcc-xtensa-esp32`
if [ "${exist}" = "" ]; then
    echo "export PATH=\"${XTENSA_TOOLCHAIN_PATH}/${HOST_OS}/bin:\$PATH\"" >> ~/${BASHRC_FILE}
fi
echo "done"

echo -n "installing gcc-xtensa-lx106 toolchain ..."
XTENSA_TOOLCHAIN_PATH=${OBJ_DIR}/build/compiler/gcc-xtensa-lx106
if [ ! -d ${XTENSA_TOOLCHAIN_PATH} ]; then
    mkdir -p ${XTENSA_TOOLCHAIN_PATH}
fi
cd ${XTENSA_TOOLCHAIN_PATH}
wget -q ${GCC_XTENSA_ESP8266_URL}
if [ $? -ne 0 ]; then
    echo -e "\nerror: download gcc-xtensa-lx106 toolchain faield\n"
    exit 1
fi
tar zxf *xtensa-lx106-elf.tar.gz
if [ $? -ne 0 ]; then
    echo -e "\nerror: extract gcc-xtensa-lx106 toolchain faield\n"
    exit 1
fi
rm -f *xtensa-lx106-elf.tar.gz
mv *xtensa-lx106-elf ${HOST_OS}
exist=`cat ~/${BASHRC_FILE} | grep gcc-xtensa-lx106`
if [ "${exist}" = "" ]; then
    echo "export PATH=\"${XTENSA_TOOLCHAIN_PATH}/${HOST_OS}/bin:\$PATH\"" >> ~/${BASHRC_FILE}
fi
echo "done"

echo -n "installing OpenOCD ..."
OPENOCD_PATH=${OBJ_DIR}/build/OpenOCD
OPENOCD_URL="https://files.alicdn.com/tpsservice/27ba2d597a43abfca94de351dae65dff.zip"
if [ -d ${OPENOCD_PATH} ]; then
    rm -rf ${OPENOCD_PATH}
fi
cd ${OBJ_DIR}/build/
wget -q ${OPENOCD_URL}
if [ $? -ne 0 ]; then
    echo -e "\nerror: download OpenOCD faield\n"
    exit 1
fi
unzip -qq 27ba2d*.zip
rm -f 27ba2d*.zip
rm -rf __MACOSX
echo "done"

echo "install finieshed!"
echo "please run \"source ~/${BASHRC_FILE}\" command to make settings effective"
