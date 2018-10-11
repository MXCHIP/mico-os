# -*- coding: UTF-8 -*-
# Date    : 2018/08/07
# Author  : Snow Yang
# Mail    : yangsw@mxchip.com

import os
import sys
import struct
import argparse
from subprocess import Popen, CalledProcessError, PIPE
from progressbar import *

MARKER = {'OSX':'█', 'Win32':'#'}
FILL = {'OSX':'░', 'Win32':'-'}

def progress(size, hostos):
    widgets = [' ',
    Bar(marker=MARKER[hostos], left='', right='', fill=FILL[hostos]), ' ', 
    AnimatedMarker(), ' ', 
    Percentage(),' ', 
    AdaptiveETA(), ' ', 
    FileTransferSpeed(), ' ', 
    DataSize()]
    pbar = ProgressBar(widgets=widgets, max_value=size)
    pbar.start()
    return pbar

def sflasher(argv):
    parser = argparse.ArgumentParser(description='Download binary file to flash')
    parser.add_argument('-b', '--board' , type=str, required=True, help='Board')
    parser.add_argument('-m', '--mcu'   , type=str, required=True, help='MCU')
    parser.add_argument('-f', '--file'	, type=str, required=True, help='File name')
    parser.add_argument('-a', '--addr'  , type=str, required=True, help='Address')
    args = parser.parse_args(argv)

    hostos = 'OSX' if sys.platform == 'darwin' else 'Linux64' if sys.platform == 'linux2' else 'Win32'
    pbar = progress(os.path.getsize(args.file), hostos)
    openocd = os.path.join(os.getcwd(), 'mico-os/makefiles/OpenOCD/binary/'+hostos+'/openocd_mico').replace('\\', '/')
    cmd = openocd+' -f mico-os/makefiles/OpenOCD/interface/jlink_swd.cfg \
    -f mico-os/makefiles/OpenOCD/'+args.mcu+'/'+args.mcu+'.cfg \
    -f mico-os/sub_build/spi_flash_write/sflash_write.tcl \
    -c "sflash_write_file '+args.file+' 0 '+args.addr+' '+args.board+' 0" -c shutdown 2>out/openocd.log'
    proc = Popen(cmd, shell=True, universal_newlines=True, stdout=PIPE)
    while True:
        out = proc.stdout.readline().strip()
        if proc.poll() != None:
            if proc.poll():
                os.system('cat out/openocd.log')
            return
        else:
            if out.isdigit():
                pbar.update(int(out))
    pbar.finish()

if __name__ == "__main__":
	try:
		sflasher(sys.argv[1:])
	except Exception as e:
		print(e)
		sys.exit(2)
