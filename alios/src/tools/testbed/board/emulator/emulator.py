import os, glob, serial, traceback

def list_devices(host_os):
    return glob.glob('/dev/emulator-*')

def exist(device):
    return os.path.exists(device)

def new_device(device):
    try:
        ser = serial.Serial(device, 115200, timeout = 0.02)
    except:
        ser = None
        traceback.print_exc()
        print 'emulator: open {0} error'.format(device)
    return ser

def erase(device):
    return 'fail'

def program(device, address, file):
    return 'fail'

def control(device, operation):
    return 'fail'
