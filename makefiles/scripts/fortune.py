# -*- coding: UTF-8 -*-
# Date    : 2018/08/10
# Author  : Snow Yang
# Mail    : yangsw@mxchip.com

import sys
import zlib
import random
import argparse

def main(argv):
    parser = argparse.ArgumentParser(description='Show/Compress/Uncompress fortune')
    parser.add_argument('-x', '--command' , type=str, required=True, help='Board')
    parser.add_argument('-i', '--input'	, type=str, required=True, help='File name')
    parser.add_argument('-o', '--output'  , type=str, required=False, help='Address')
    args = parser.parse_args(argv)

    with open(args.input, 'rb') as f:
        c = f.read()

    if args.command == 'show':
        lines = zlib.decompress(c).splitlines()
        line = lines[random.randint(0 ,len(lines)-1)]
        line = line.decode('UTF-8').encode('GBK') if sys.platform == 'win32' else line
        print('\r\n'+line+'\r\n')
    elif args.command == 'compress':
        with open(args.output, 'wb') as f:
            f.write(zlib.compress(c))
    elif args.command == 'decompress':
        with open(args.output, 'wb') as f:
            f.write(zlib.decompress(c))

if __name__ == '__main__':
	try:
		main(sys.argv[1:])
	except Exception as e:
		print(e)
		sys.exit(2)