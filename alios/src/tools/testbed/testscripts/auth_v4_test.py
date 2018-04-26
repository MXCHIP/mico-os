import sys
import time
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
        elif arg == '--help':
            print 'Usage: python {0} [--firmware=xxx.bin] [--wifissid=wifi_ssid] [--wifipass=password]'.format(sys.argv[0])
            return [0, 'help']
        i += 1

    at = Autotest()
    logname = time.strftime('%Y-%m-%d@%H-%M')
    logname = 'authentication-' + logname +'.log'
    if at.start(server, port, logname) is False:
        print 'error: start failed'
        return [1, 'connect testbed failed']

    #request device allocation
    number = 1
    timeout = 300
    allocated = allocate_devices(at, model, number, timeout)
    if len(allocated) != number:
        return [1, 'allocate device failed']

    #construct the device list
    devices = {}
    for i in range(len(allocated)):
        devices[chr(ord('A')+i)] = allocated[i]
    device_list = list(devices)
    device_list.sort()
    device_attr = {}
    print_device_list(devices)

    #subscribe and reboot devices
    result = subscribe_and_reboot_devices(at, devices)
    if result is False:
        return [1, 'subscribe devices failed']

    #program devices
    result = program_devices(at, devices, model, firmware)
    if result is False:
        return [1, 'program device failed']

    #set specific extnetid to isolate the network
    extnetid = '000102030405' # raspberry pi mesh extended netid
    for device in device_list:
        at.device_run_cmd(device, 'umesh extnetid {0}'.format(extnetid))

    #reboot and get device mac address
    result = reboot_and_get_mac(at, device_list, device_attr)
    if result is False:
        return [1, 'reboot and get macaddr failed']

    #setup whitelist for line topology
    print 'topology:'
    print "A <--> Raspi3(Leader)\n"
    device = 'A'
    raspi3_mac = 'b827eb384ac50000' #raspberry pi mac address
    device_attr[device]['nbrs'] = []
    device_attr[device]['nbrs'].append(raspi3_mac)
    device_attr[device]['role'] = 'router'

    #start devices to form mesh network
    result = start_devices(at, device_list, device_attr, ap_ssid, ap_pass)
    if result is False:
        restore_device_status(at, device_list)
        return [1, 'form desired mesh network failed']

    #get device ips
    get_device_ips(at, device_list, device_attr)

    #print device attributes
    print_device_attrs(device_attr)

    #ping test
    [ping_pass_num, ping_fail_num] = ping_test(at, device_list, device_attr)

    #udp test
    [udp_pass_num, udp_fail_num] = udp_test(at, device_list, device_attr)

    #ping external sp server ipv4 addr
    sp_server_ip = '30.4.10.12'
    [ext_ping_pass_num, ext_ping_fail_num] = ping_ext_test(at, device_list, sp_server_ip)

    restore_device_status(at, device_list)
    at.stop()
    return [0, 'succeed. ping: pass-{0} fail-{1}, udp: pass-{2} fail-{3}, ext ping: pass-{4}, fail-{5}'.format(ping_pass_num, ping_fail_num, udp_pass_num, udp_fail_num, ext_ping_pass_num, ext_ping_fail_num)]

if __name__ == '__main__':
    [code, msg] = main()
    sys.exit(code)
