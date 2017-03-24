#!/bin/bash
#
#  UNPUBLISHED PROPRIETARY SOURCE CODE
#  Copyright (c) 2016 MXCHIP Inc.
#
#  The contents of this file may not be disclosed to third parties, copied or
#  duplicated in any form, in whole or in part, without the prior written
#  permission of MXCHIP Corporation.
#

scriptname=${0##*/}
scriptdir=${0%*$scriptname}

local_tools_root=$(cat .mico 2> /dev/null| grep TOOLS_ROOT=)
global_tools_root=$(cat ~/.mico/.mico 2> /dev/null| grep TOOLS_ROOT=)

# Rerun the script with environment cleared out
if [ "$1" != "ENVCLEARED" ]; then
    exec -c "$scriptdir$scriptname" ENVCLEARED "$PATH" "$HOME" "$@"
    exit
else
    shift
    export SAVED_PATH=$1
    shift
    export SAVED_HOME=$1
    shift
fi
# Delete some automatic bash environment variables
unset PATH
unset TERM
unset PWD
unset OLDPWD
unset SHLVL

tools_root=
if [ $local_tools_root ]; then 
	tools_root=${local_tools_root}
elif [ $global_tools_root ]; then 
	tools_root=${global_tools_root}
elif [ -d "mico-os/MiCoder" ]; then 
	echo Use the mico-os root MiCoder
else
	echo Can not find tools directory!
	exit
fi
echo ${tools_root} > mico-os/makefiles/scripts/tools_root

TOOLS_ROOT=${scriptdir}mico-os/MiCoder
source ${scriptdir}mico-os/makefiles/scripts/tools_root

linux32unamestr=`"${TOOLS_ROOT}/cmd/Linux32/uname" 2> /dev/null`
linux64unamestr=`"${TOOLS_ROOT}/cmd/Linux64/uname" 2> /dev/null`
osxunamestr=`"${TOOLS_ROOT}/cmd/OSX/uname"  2> /dev/null`
win32unamestr=`"${TOOLS_ROOT}/cmd/Win32/uname.exe" -o 2> /dev/null`

if [ "$linux32unamestr" == "Linux" ] || [ "$linux64unamestr" == "Linux" ]; then
# Linux
linuxuname64str=`"{TOOLS_ROOT}/cmd/Linux64/uname" -m 2> /dev/null`
if [ "$linuxuname64str" == "x86_64" ]; then

# Linux64
#echo Host is Linux64
"${TOOLS_ROOT}/cmd/Linux64/make" "$@" HOST_OS=Linux64

else

# Linux32
#echo Host is Linux32
"${TOOLS_ROOT}/cmd/Linux32/make" "$@" HOST_OS=Linux32

fi
elif [[ "$osxunamestr" == *Darwin* ]]; then

#OSX
#echo Host is OSX
"${TOOLS_ROOT}/cmd/OSX/make" "$@" HOST_OS=OSX

elif [ "${win32unamestr}" == "MinGW" ]; then

#MinGW / Cygwin
#echo Host is MinGW
"${TOOLS_ROOT}/cmd/Win32/make.exe" "$@" HOST_OS=Win32

else
echo Unknown host! 
echo Make sure you have extract the correct MiCoder tools under MiCO SDK
echo For example, under macOS. You should extract MiCoder.OSX.tar.gz to "<MiCO_SDK>"/MiCoder
fi

