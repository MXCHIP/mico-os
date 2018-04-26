#!/usr/bin/env bash

apt-get update
apt-get -y upgrade
apt-get -y install raspberrypi-kernel-headers git libgmp3-dev gawk qpdf bison flex make bc
apt-get -y install vim libnl1 libnl-3-200 libnl-dev libnl-genl-3-200 libnl-route-3-200

if [ -d nexmon ]; then
    rm -rf nexmon
fi
git clone https://github.com/seemoo-lab/nexmon.git
cd nexmon
nexmondir=`pwd`

cd buildtools/isl-0.10
./configure
make
make install
if [ -f /usr/lib/arm-linux-gnueabihf/libisl.so.10 ]; then
    rm -f /usr/lib/arm-linux-gnueabihf/libisl.so.10
fi
ln -s /usr/local/lib/libisl.so /usr/lib/arm-linux-gnueabihf/libisl.so.10

cd ${nexmondir}
source setup_env.sh
make
if [ $? -ne 0 ]; then
    echo "error: build nexmon failed"
    exit 1
fi

cd patches/bcm43430a1/7_45_41_46/nexmon/
make
if [ $? -ne 0 ]; then
    echo "error: build brcmfmac firmware failed"
    exit 1
fi
make backup-firmware
make install-firmware

brcmpath=`modinfo brcmfmac | grep filename | awk '{print $2}'`
if [ ! -f ${brcmpath}.orig ]; then
    cp ${brcmpath} ${brcmpath}.orig
fi
cp ${nexmondir}/patches/bcm43430a1/7_45_41_46/nexmon/brcmfmac_kernel49/brcmfmac.ko ${brcmpath}
depmod -a

cd ${nexmondir}/utilities/nexutil/
make
if [ $? -ne 0 ]; then
    echo "error: build utility failed"
    exit 1
fi
make install
nexutil -m2
