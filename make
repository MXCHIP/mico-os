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


linux32unamestr=`"${scriptdir}mico-os/MiCoder/cmd/Linux32/uname" 2> /dev/null`
linux64unamestr=`"${scriptdir}mico-os/MiCoder/cmd/Linux64/uname" 2> /dev/null`
osxunamestr=`"${scriptdir}mico-os/MiCoder/cmd/OSX/uname"  2> /dev/null`
win32unamestr=`"${scriptdir}mico-os/MiCoder/cmd/Win32/uname.exe" -o 2> /dev/null`

if [ "$linux32unamestr" == "Linux" ] || [ "$linux64unamestr" == "Linux" ]; then
# Linux
linuxuname64str=`"${scriptdir}mico-os/MiCoder/cmd/Linux64/uname" -m 2> /dev/null`
if [ "$linuxuname64str" == "x86_64" ]; then

# Linux64
#echo Host is Linux64
"${scriptdir}mico-os/MiCoder/cmd/Linux64/make" "$@" HOST_OS=Linux64

else

# Linux32
#echo Host is Linux32
"${scriptdir}mico-os/MiCoder/cmd/Linux32/make" "$@" HOST_OS=Linux32

fi
elif [[ "$osxunamestr" == *Darwin* ]]; then

#OSX
#echo Host is OSX
"${scriptdir}mico-os/MiCoder/cmd/OSX/make" "$@" HOST_OS=OSX

elif [ "${win32unamestr}" == "MinGW" ]; then

#MinGW / Cygwin
#echo Host is MinGW
"${scriptdir}mico-os/MiCoder/cmd/Win32/make.exe" "$@" HOST_OS=Win32

else
echo Unknown host! 
echo Make sure you have extract the correct MiCoder tools under MiCO SDK
echo For example, under macOS. You should extract MiCoder.OSX.tar.gz to "<MiCO_SDK>"/MiCoder
fi

