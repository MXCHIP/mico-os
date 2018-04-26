import sys, json
from autotest import Autotest

def print_help():
    print "Usages:"
    print "1. list devices: python {0} --list [--server serverip] [--port port]".format(sys.argv[0])
    print "2. get device nerghbors: python {0} --nbrs device_string [--server serverip] [--port port]".format(sys.argv[0])


def list_devices(at):
    ret={}
    devices = at.get_device_list()
    for i in range(len(devices)):
        index = str(i)
        devstr = devices[i].split(',')[2]
        devstr = devstr.replace('/dev/', '')
        if at.device_subscribe({index:devstr}) == False:
            continue
        mac =  at.device_run_cmd(index, ['umesh', 'macaddr'], 1, 1, [':'])
        if mac == False or len(mac) != 1:
            continue
        mac = mac[0].replace(':', '')
        ret[devstr] = mac
    retstr = json.dumps(ret, sort_keys=True, indent=4)
    print retstr
    return True

def get_nbrs(at, devices):
    ret = {}
    fail_num = 0
    for i in range(len(devices)):
        index = str(i)
        devstr = devices[i]
        ret[devstr] = {}
        if at.device_subscribe({index:devstr}) == False:
            fail_num += 1
            continue
        nbrs = at.device_run_cmd(index, ['umesh', 'nbrs'], 32, 2, [','])
        if nbrs == False or len(nbrs) == 0:
            fail_num += 1
            continue
        index = 0
        for j in range(len(nbrs)):
            if nbrs[j].startswith('\t') == False:
                continue
            nbr_index = '{0:02d}'.format(index)
            index += 1
            nbr = nbrs[j].replace('\t', '')
            ret[devstr][nbr_index] = nbr
    retstr = json.dumps(ret, sort_keys=True, indent=4)
    print retstr
    return (fail_num == 0)

operation = None
serverip = '10.125.52.132'
serverport = 34568
i = 1
while i < len(sys.argv):
    arg = sys.argv[i]
    if arg == '--list':
        operation = 'list_devices'
    elif arg == '--nbrs' and (i + 1) < len(sys.argv):
        operation = 'get_nbrs'
        devices = sys.argv[i + 1].split(',')
        i += 1
    elif arg == '--server' and (i + 1) < len(sys.argv):
        serverip = sys.argv[i + 1]
        i += 1
    elif arg == '--port' and (i + 1) < len(sys.argv) and sys.argv[i+1].isdigit():
        serverport = int(sys.argv[i + 1])
        i += 1
    elif arg == '--help':
        print_help()
    i += 1

if operation == None:
    sys.exit(0)

at=Autotest()
if at.start(serverip, serverport) == False:
    print 'error: connect to server {0}:{1} failed'.format(serverip, serverport)
    exit(1)

if operation == 'list_devices':
    ret = list_devices(at)
elif operation == 'get_nbrs':
    ret = get_nbrs(at, devices)

at.stop()
exit(ret == True)

