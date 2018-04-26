import hashlib

DEBUG = False

TYPE_NONE            = 'NONE'
CLIENT_DEV           = 'CDEV'
ALL_DEV              = 'ADEV'
DEVICE_LOG           = 'DLOG'
DEVICE_STATUS        = 'DSTU'
DEVICE_CMD           = 'DCMD'
DEVICE_ERASE         = 'DERS'
DEVICE_PROGRAM       = 'DPRG'
DEVICE_RESET         = 'DRST'
DEVICE_START         = 'DSTR'
DEVICE_STOP          = 'DSTP'
DEVICE_ALLOC         = 'DALC'
DEVICE_DEBUG_START   = 'DDBS'
DEVICE_DEBUG_DATA    = 'DDBD'
DEVICE_DEBUG_REINIT  = 'DDBR'
DEVICE_DEBUG_STOP    = 'DDBE'
LOG_SUB              = 'LGSB'
LOG_UNSUB            = 'LGUS'
STATUS_SUB           = 'STSB'
STATUS_UNSUB         = 'STUS'
LOG_DOWNLOAD         = 'LGDL'
FILE_BEGIN           = 'FBGN'
FILE_DATA            = 'FDTA'
FILE_END             = 'FEND'
CMD_DONE             = 'CMDD'
CMD_ERROR            = 'CMDE'
HEARTBEAT            = 'HTBT'
CLIENT_LOGIN         = 'CLGI'
TERMINAL_LOGIN       = 'TLGI'
ACCESS_LOGIN         = 'ALGI'
ACCESS_REPORT_STATUS = 'ARPS'
ACCESS_ADD_CLIENT    = 'AADC'
ACCESS_DEL_CLIENT    = 'ADLC'
ACCESS_ADD_TERMINAL  = 'AADT'
ACCESS_UPDATE_TERMINAL = 'AUPT'
ACCESS_DEL_TERMINAL  = 'ADLT'

def is_valid_type(type):
    #frequently used commands
    if type == DEVICE_LOG:
        return True
    if type == DEVICE_STATUS:
        return True
    if type == DEVICE_DEBUG_DATA:
        return True
    if type == HEARTBEAT:
        return True
    if type == DEVICE_CMD:
        return True
    if type == CMD_DONE:
        return True
    if type == CMD_ERROR:
        return True
    if type == DEVICE_ERASE:
        return True
    if type == DEVICE_PROGRAM:
        return True
    if type == DEVICE_RESET:
        return True
    if type == DEVICE_START:
        return True
    if type == DEVICE_STOP:
        return True
    if type == DEVICE_DEBUG_START:
        return True
    if type == DEVICE_DEBUG_REINIT:
        return True
    if type == DEVICE_DEBUG_STOP:
        return True
    #less frequently used commands
    if type == CLIENT_DEV:
        return True
    if type == ALL_DEV:
        return True
    if type == LOG_SUB:
        return True
    if type == LOG_UNSUB:
        return True
    if type == STATUS_SUB:
        return True
    if type == STATUS_UNSUB:
        return True
    if type == LOG_DOWNLOAD:
        return True
    if type == FILE_BEGIN:
        return True
    if type == FILE_DATA:
        return True
    if type == FILE_END:
        return True
    if type == DEVICE_ALLOC:
        return True
    if type == CLIENT_LOGIN:
        return True
    if type == TERMINAL_LOGIN:
        return True
    if type == ACCESS_LOGIN:
        return True
    if type == ACCESS_REPORT_STATUS:
        return True
    if type == ACCESS_ADD_CLIENT:
        return True
    if type == ACCESS_DEL_CLIENT:
        return True
    if type == ACCESS_ADD_TERMINAL:
        return True
    if type == ACCESS_UPDATE_TERMINAL:
        return True
    if type == ACCESS_DEL_TERMINAL:
        return True
    return False

def construct(type, value):
    if is_valid_type(type) == False:
        return ''
    if len(value) > 99999:
        print "warning: data size larger than permited"
        return ''
    frame = '{' + type + ',' + '{0:05d}'.format(len(value)) + ',' + value + '}'
    return frame

def parse(msg):
    sync = False
    type = TYPE_NONE
    length = 0
    value = ''
    while msg != '':
        if len(msg) < 12:
            type = TYPE_NONE
            length = 0
            value = ''
            break;
        #   print(msg)
        for i in range(len(msg)):
            if msg[i] != '{':
                continue
            if (i + 13) > len(msg):
                break;
            if is_valid_type(msg[i+1: i+5]) == False:
                continue
            if msg[i + 5] != ',':
                continue
            if msg[i+6 : i+11].isdigit() == False:
                continue
            if msg[i+11] != ',':
                continue
            sync = True
            if DEBUG and i > 0:
                print("msg:{0}".format(msg))
                print("discard:{0}".format(msg[0:i]))
            msg = msg[i:]
            break
        if sync == False:
            break

        type = msg[1:5]
        length = int(msg[6:11])
        if len(msg) < length + 13:
            type = TYPE_NONE
            length = 0
            value = ''
            break
        if msg[length + 12] != '}':
            sync = False
            if DEBUG: print(msg[0:12] + " Lose sync because of FOOTER error")
            msg = msg[1:]
            continue
        value = msg[12:length+12]
        msg = msg[length+13:]
        break;
    return type, length, value, msg

def hash_of_file(filename):
    h = hashlib.sha1()
    with open(filename, 'rb', buffering=0) as f:
        for b in iter(lambda : f.read(1024), b''):
            h.update(b)
    return h.hexdigest()
