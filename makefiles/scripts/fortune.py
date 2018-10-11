# -*- coding: UTF-8 -*-
# Date    : 2018/08/10
# Author  : Snow Yang
# Mail    : yangsw@mxchip.com

import sys
import zlib
import random

with open('mico-os/makefiles/scripts/fortune.dat', 'rb') as f:
    c = f.read()

lines = zlib.decompress(c).splitlines()
line = lines[random.randint(0 ,len(lines)-1)]
line = line.decode('UTF-8').encode('GBK') if sys.platform == 'win32' else line
print('\r\n'+line+'\r\n')