import sys
import traceback
import subprocess
import os
import errno
import argparse
import time

from modules import syncpreparelib
from modules import meshlib
from modules import rhinolib
from modules import ywsslib
from modules import wsflib
from modules import devmgrlib
from modules import msdplib
from modules import gatewaylib
from modules import bekenlib
from modules import pushcodelib
import configslib

parser = argparse.ArgumentParser(prog='codesync',
    description="Code sync tool for aos",
    formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument("-d", "--dstbase", dest="dstbase", default='github')
parser.add_argument("-k", "--mks", dest="mks", nargs="*", default='../modules')
parser.add_argument('-f', action='store_true', default=False,
                    dest='force',
                    help='Push to master')

def main():
    basedir = os.getcwd()
    pargs, remainder = parser.parse_known_args(sys.argv[1:])
    configs = configslib.configslib()
    if configs.get_vendor_configs(pargs.dstbase):
        print "do not support *" + pargs.dstbase + "*"
        exit()
    syncprepare = syncpreparelib(configs, pargs.mks)

    syncprepare.cleanup_codebase()
    syncprepare.cleanup_boards()
    syncprepare.cleanup_build()
    syncprepare.cleanup_script()
    syncprepare.cleanup_platform()
    syncprepare.cleanup_kernel()
    syncprepare.cleanup_example()
    syncprepare.cleanup_bootloader()
    syncprepare.cleanup_test()
    syncprepare.cleanup_tools()
    modules = configs.modules.split(' ')
    for module in modules:
        print module
        if module == "mesh":
            mesh = meshlib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir, syncprepare.vendor)
            mesh.make_folder()
            mesh.make_lib()
        elif module == "ywss":
            ywss = ywsslib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir, syncprepare.vendor)
            ywss.make_folder()
            ywss.make_lib()
        elif module == "beken":
            beken = bekenlib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir)
            beken.make_folder()
            beken.make_lib()
        elif module == "rhino":
            rhino = rhinolib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir, syncprepare.vendor)
            rhino.make_folder()
            rhino.make_lib()
        elif module == "msdp":
            msdp = msdplib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir, syncprepare.vendor)
            msdp.make_folder()
            msdp.make_lib()
        elif module == "wsf":
            wsf = wsflib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir, syncprepare.vendor)
            wsf.make_folder()
            wsf.make_lib()
        elif module == "devmgr":
            devmgr = devmgrlib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir, syncprepare.vendor)
            devmgr.make_folder()
            devmgr.make_lib()
        elif module == "gateway":
            gateway = gatewaylib(syncprepare.srcdir, syncprepare.dstdir, syncprepare.srcbase, syncprepare.dstbase, syncprepare.mkdir, syncprepare.vendor)
            gateway.make_folder()
            gateway.make_lib()
        else:
            print "unknown module", module

    pushcode = pushcodelib(basedir, syncprepare.srcdir, syncprepare.dstdir, pargs.force)
    pushcode.execute()
    pushcode.cleancode()

if __name__ == "__main__":
    main()
