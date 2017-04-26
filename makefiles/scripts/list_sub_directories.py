#! /usr/bin/env python
# Copyright (C) 2015 Marvell International Ltd.
# All Rights Reserved.

# Load application to ram helper script
# Note: sys.stdout.flush() and sys.stderr.flush() are required for proper
# console output in eclipse

import os, sys, getopt

def print_usage():
    print ""
    print "Usage:"
    print sys.argv[0]
    print " Usage: list all sub-directories"
    sys.stdout.flush()

def main():
    
    if not len(sys.argv)==2:
        print_usage()
        sys.exit(2)

    ROOT_DIR = sys.argv[1]

    if not os.path.isdir(ROOT_DIR):
        print "Input a directory name!"
        sys.stdout.flush()
        sys.exit()

    for root, dirs, files in os.walk(ROOT_DIR):  
    	root_dir = root[0:len(root)]
        print(root_dir)  

#    print image_size
    sys.stdout.flush()
    sys.exit(0)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
