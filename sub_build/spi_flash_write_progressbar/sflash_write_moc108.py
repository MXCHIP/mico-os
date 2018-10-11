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
    parser.add_argument('-o', '--openocd'   , type=str, required=True, help='openocd path')
    parser.add_argument('-f', '--file'	    , type=str, required=True, help='File name')
    parser.add_argument('-a', '--addr'      , type=str, required=True, help='Address')
    args = parser.parse_args(argv)

    hostos = 'OSX' if sys.platform == 'darwin' else 'Linux64' if sys.platform == 'linux2' else 'Win32'
    pbar = progress(os.path.getsize(args.file), hostos)
    cmd = '%s -f mico-os/makefiles/OpenOCD/interface/jlink.cfg \
    -f mico-os/makefiles/OpenOCD/MOC108/MOC108.cfg \
    -f mico-os/makefiles/OpenOCD/MOC108/flash.tcl \
    -c init -c flash_boot_check -c "flash_program %s %s" -c shutdown 2>build/openocd.log'%(args.openocd, args.file, args.addr)
    proc = Popen(cmd, shell=True, universal_newlines=True, stdout=PIPE)
    while True:
        out = proc.stdout.readline().strip()
        if proc.poll() != None:
            if proc.poll():
                with open('build/openocd.log', 'r') as f:
                    print(f.read())
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
