import sys, os, time
from autotest import Autotest
from mesh_common import *

def main(firmware='lb-mk3060.bin', model='mk3060'):
    ap_ssid = 'aos_test_01'
    ap_pass = 'Alios@Embedded'
    server = '10.125.52.132'
    port = 34568

    #parse input
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg.startswith('--firmware='):
            args = arg.split('=')
            if len(args) != 2:
                print 'wrong argument {0} input, example: --firmware=firmware.bin'.format(arg)
            firmware = args[1]
        elif arg.startswith('--model='):
            args = arg.split('=')
            if len(args) != 2:
                print 'wrong argument {0} input, example: --model=mk3060'.format(arg)
            model = args[1]
        elif arg.startswith('--wifissid='):
            args = arg.split('=')
            if len(args) != 2:
                print 'wrong argument {0} input, example: --wifissid=test_wifi'.format(arg)
            ap_ssid = args[1]
        elif arg.startswith('--wifipass='):
            args = arg.split('=')
            if len(args) != 2:
                print 'wrong argument {0} input, example: --wifipass=test_password'.format(arg)
            ap_pass = args[1]
        elif arg.startswith('--server='):
            args = arg.split('=')
            if len(args) != 2:
                print 'wrong argument {0} input, example: --server=192.168.10.16'.format(arg)
                return [1, 'argument {0} error'.format(arg)]
            server = args[1]
        elif arg.startswith('--port='):
            args = arg.split('=')
            if len(args) != 2 or args[1].isdigit() == False:
                print 'wrong argument {0} input, example: --port=34568'.format(arg)
                return [1, 'argument {0} error'.format(arg)]
            port = int(args[1])
        elif arg=='--help':
            print 'Usage: python {0} [--firmware=xxx.bin] [--wifissid=wifi_ssid] [--wifipass=password]'.format(sys.argv[0])
            return [0, 'help']
        i += 1

    at=Autotest()
    logname=time.strftime('%Y-%m-%d@%H-%M')
    logname = 'multicast-' + logname +'.log'
    if at.start(server, port, logname) == False:
        print 'error: start failed'
        return [1, 'connect testbed failed']

    #request device allocation
    number = 4
    timeout = 300
    allocated = allocate_devices(at, model, number, timeout)
    if len(allocated) != number:
        return [1, 'allocate device failed']

    devices = {}
    for i in range(len(allocated)):
        devices[chr(ord('A')+i)] = allocated[i]
    device_list = list(devices)
    device_list.sort()
    device_attr={}
    print_device_list(devices)

    #subscribe and reboot devices
    result = subscribe_and_reboot_devices(at, devices)
    if result == False:
        return [1, 'subscribe devices failed']

    #program devices
    result = program_devices(at, devices, model, firmware)
    if result == False:
        return [1, 'program device failed']

    #reboot and get device mac address
    result = reboot_and_get_mac(at, device_list, device_attr)
    if result == False:
        return [1, 'reboot and get macaddr failed']

    #set random extnetid to isolate the network
    set_random_extnetid(at, device_list)

    #setup whitelist for line topology
    print 'topology:'
    print 'router  leader  router  router'
    print '  A <---> B <---> C <---> D'
    device = 'A'
    device_attr[device]['nbrs'] = []
    device_attr[device]['nbrs'].append(device_attr['B']['mac'])
    device_attr[device]['role'] = 'router'
    device = 'B'
    device_attr[device]['nbrs'] = []
    device_attr[device]['nbrs'].append(device_attr['A']['mac'])
    device_attr[device]['nbrs'].append(device_attr['C']['mac'])
    device_attr[device]['role'] = 'leader'
    device = 'C'
    device_attr[device]['nbrs'] = []
    device_attr[device]['nbrs'].append(device_attr['B']['mac'])
    device_attr[device]['nbrs'].append(device_attr['D']['mac'])
    device_attr[device]['role'] = 'router'
    device = 'D'
    device_attr[device]['nbrs'] = []
    device_attr[device]['nbrs'].append(device_attr['C']['mac'])
    device_attr[device]['role'] = 'router'

    #start devices to form mesh network
    device_start_order = ['B', 'C', 'D', 'A']
    result = start_devices(at, device_start_order, device_attr, ap_ssid, ap_pass)
    if result == False:
        restore_device_status(at, device_list)
        return [1, 'form desired mesh network failed']

    #get device ips
    get_device_ips(at, device_list, device_attr)

    #print device attributes
    print_device_attrs(device_attr)

    #udp multicast test
    [pass_num, fail_num] = multicast_test(at, device_list, device_attr)

    restore_device_status(at, device_list)
    at.stop()
    return [0, 'succeed. multicast: pass-{0} fail-{1}'.format(pass_num, fail_num)]

if __name__ == '__main__':
    #flush to output immediately
    sys.stdout = os.fdopen(sys.stdout.fileno(), 'w', 0)
    sys.stderr = os.fdopen(sys.stderr.fileno(), 'w', 0)
    [code, msg] = main()
    sys.exit(code)

