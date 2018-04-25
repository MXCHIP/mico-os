import os, sys, time, serial, subprocess, traceback, glob

def list_devices(host_os):
    return glob.glob('/dev/esp32-*')

def exist(device):
    return os.path.exists(device)

def new_device(device):
    try:
        ser = serial.Serial(device, 115200, timeout = 0.02)
        ser.setRTS(True)
        ser.setDTR(False)
        time.sleep(0.1)
        ser.setDTR(True)
    except:
        ser = None
        print 'esp32: open {0} error'.format(device)
    return ser

def erase(device):
    retry = 8
    error = 'fail'
    while retry > 0:
        script = ['esptool.py']
        script += ['--chip']
        script += ['esp32']
        script += ['--port']
        script += [device]
        script += ['erase_flash']
        ret = subprocess.call(script)
        if ret == 0:
            error = 'success'
            break
        retry -= 1
        time.sleep(3)
    return error

def program(device, address, file):
    retry = 8
    error = 'fail'
    while retry > 0:
        script = ['esptool.py']
        script += ['--chip']
        script += ['esp32']
        script += ['--port']
        script += [device]
        script += ['--baud']
        script += ['460800']
        script += ['write_flash']
        script += ['-z']
        script += ['--flash_size=detect']
        script += [address]
        script += [file]
        ret = subprocess.call(script)
        if ret == 0:
            error =  'success'
            break
        retry -= 1
        time.sleep(3)
    control(device, 'reset')
    return error

def control(device, operation):
    try:
        ser = serial.Serial(device, 115200)
    except:
        traceback.print_exc()
        print 'esp32 control error: unable to open {0}'.format(device)
        return 'fail'
    ret = 'fail'
    try:
        if operation == 'reset':
            ser.setDTR(False)
            time.sleep(0.1)
            ser.setDTR(True)
            ret = 'success'
        elif operation == 'stop':
            ser.setDTR(False)
            ret = 'success'
        elif operation == 'start':
            ser.setDTR(True)
            ret = 'success'
    except:
        pass
    ser.close()
    return ret
