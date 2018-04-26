import os, sys, time, serial, subprocess, traceback, glob

eml3047_stlink_serials = {
    '/dev/lora-001':'001_serial_id',
    '/dev/lora-002':'002_serial_id'
    }

def list_devices(os):
    return glob.glob('/dev/lora-*')

def exist(device):
    return os.path.exists(device)

def new_device(device):
    if device not in eml3047_stlink_serials:
        print('eml3047: unknow board {0}'.format(device))
        return None
    try:
        ser = serial.Serial(device, 115200, timeout = 0.02)
        subprocess.call(['st-flash', '--serial', eml3047_stlink_serials[device], 'reset'])
    except:
        ser = None
        print('eml3047: open {0} error'.format(device))
    return ser

def erase(device):
    retry = 3
    error = 'fail'
    if device not in eml3407_stlink_serials:
        return error
    while retry > 0:
        script = ['st-flash', '--serial', eml3407_stlink_serials[device], 'erase']
        ret = subprocess.call(script)
        if ret == 0:
            error =  'success'
            break
        retry -= 1
        time.sleep(4)
    return error

def program(device, address, file):
    retry = 3
    error = 'fail'
    if device not in eml3047_stlink_serials:
        return error
    while retry > 0:
        script = ['st-flash', '--serial', eml3047_stlink_serials[device]]
        script += ['write', file, address]
        ret = subprocess.call(script)
        if ret == 0:
            error =  'success'
            break
        retry -= 1
        time.sleep(4)
    return error

def control(device, operation):
    ret = 'fail'
    if device not in eml3047_stlink_serials:
        return ret
    try:
        if operation == 'reset':
            subprocess.call(['st-flash', '--serial', eml3047_stlink_serials[device], 'reset'])
            ret = 'success'
        elif operation == 'stop':
            ret = 'fail'
        elif operation == 'start':
            ret = 'fail'
    except:
        pass
    return ret
