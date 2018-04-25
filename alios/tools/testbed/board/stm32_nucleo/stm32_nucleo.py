import os, sys, time, serial, subprocess, shlex, traceback, glob
from os import path

nucleo_stlink_serials = {}
nucleo_debug_sessions = {}

def list_devices(host_os):
    return glob.glob('/dev/stm32*-*')

def exist(device):
    return path.exists(device)

def new_device(device):
    info = subprocess.check_output(['udevadm', 'info', '-q', 'property', '-n', device])
    vendor_id = None
    model_id  = None
    serial_id = None
    for line in info.split('\n'):
        line.replace('\n', '')
        if line.startswith('ID_VENDOR_ID='):
            vendor_id = line.replace('ID_VENDOR_ID=', '')
        if line.startswith('ID_MODEL_ID='):
            model_id = line.replace('ID_MODEL_ID=', '')
        if line.startswith('ID_SERIAL_SHORT='):
            serial_id = line.replace('ID_SERIAL_SHORT=', '')
    if vendor_id != '0483' or model_id != '374b' or serial_id == None:
        print('stm32_nucleo: parse stlink serial_id for {0} failed'.format(device))
        print('stm32_nucleo: vendor_id:{0} model_id:{1} serial_id:{2}'.format(repr(vendor_id), repr(model_id), repr(serial_id)))
        return None
    if len(serial_id) > 15:
        serial_id = serial_id[:15]
    serial_hexid = ''
    for c in serial_id:
        serial_hexid += '{0:02x}'.format(ord(c))
    nucleo_stlink_serials[device] = serial_hexid
    try:
        ser = serial.Serial(device, 115200, timeout = 0.02)
        subprocess.call(['st-flash', '--serial', nucleo_stlink_serials[device], 'reset'])
    except:
        ser = None
        print('stm32_nucleo: open {0} error'.format(device))
    return ser

def erase(device):
    retry = 3
    if device not in nucleo_stlink_serials:
        return 'fail'
    if device in nucleo_debug_sessions:
        return 'busy'
    error = 'fail'
    while retry > 0:
        script = ['st-flash', '--serial', nucleo_stlink_serials[device], 'erase']
        ret = subprocess.call(script)
        if ret == 0:
            error =  'success'
            break
        retry -= 1
        time.sleep(4)
    return error

def program(device, address, file):
    retry = 3
    if device not in nucleo_stlink_serials:
        return 'fail'
    if device in nucleo_debug_sessions:
        return 'busy'
    error = 'fail'
    while retry > 0:
        script = ['st-flash', '--serial', nucleo_stlink_serials[device]]
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
    if device not in nucleo_stlink_serials:
        return ret
    try:
        if operation == 'reset':
            subprocess.call(['st-flash', '--serial', nucleo_stlink_serials[device], 'reset'])
            ret = 'success'
        elif operation == 'stop':
            ret = 'fail'
        elif operation == 'start':
            ret = 'fail'
    except:
        pass
    return ret

def debug_start(device, port):
    if device not in nucleo_stlink_serials:
        return 'nonexist'
    if device in nucleo_debug_sessions:
        return 'busy'

    for device in nucleo_debug_sessions:
        if nucleo_debug_sessions[device]['port'] == port:
            return 'port_in_use'
    logfolder = path.dirname(path.abspath(__file__))
    logfile = path.join(logfolder, '{0}-debuglog.txt'.format(path.basename(device)))
    try:
        flog = open(logfile, 'a+')
    except:
        traceback.print_exc()
        return 'open_log_fail'
    command = 'st-util --serial {0} -p {1}'.format(nucleo_stlink_serials[device], port)
    command = shlex.split(command)
    p = subprocess.Popen(command, stdout=flog, stderr=flog)
    time.sleep(0.2)
    if p.poll() != None:
        return 'fail'
    nucleo_debug_sessions[device] = {'process': p, 'port': port, 'flog':flog }
    return 'success'

def debug_stop(device):
    if device not in nucleo_debug_sessions:
        return 'fail'
    time.sleep(0.2)
    nucleo_debug_sessions[device]['process'].kill()
    nucleo_debug_sessions[device]['flog'].close()
    nucleo_debug_sessions.pop(device)
    return 'success'

