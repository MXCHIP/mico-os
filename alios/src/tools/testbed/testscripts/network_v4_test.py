import os, sys, time
from autotest import Autotest

def main(firmware='networkapp@stm32l432kc-nucleo.bin', model='stm32'):
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
        elif arg=='--help':
            print 'Usage: python {0} [--firmware=xxx.bin] [--wifissid=wifi_ssid] [--wifipass=password]'.format(sys.argv[0])
            return [0, 'help']
        i += 1

    at=Autotest()
    logname=time.strftime('%Y-%m-%d@%H-%M')
    logname = 'network_test-' + logname +'.log'
    if at.start(server, port, logname) == False:
        return [1, 'connect testbed failed']

    #request device allocation
    number = 1
    timeout = 60
    allocated = at.device_allocate(model, number, timeout, purpose='sal')
    if len(allocated) != number:
        return [1, 'allocate device failed']
        print 'allocated: {0}'.format(allocated)

    devices={'A':allocated[0]} #construct device list

    #subscribe and reboot devices
    result = at.device_subscribe(devices)
    if result == False:
        print 'subscribe devices failed'
        return [1, 'subscribe devices failed']

    #program devices
    device_list = list(devices)
    device_list.sort()
    for device in device_list:
        result = at.device_erase(device)
        if result == False:
            print 'erase device failed'
            return [1, 'erase device failed']
        time.sleep(2)
        result = at.device_program(device, '0x8000000', firmware)
        if result == False:
            print 'program device failed'
            return [1, 'program device failed']
        time.sleep(2)
        result = at.device_control(device, 'reset')
        if result == False:
            print("device %s reset failed " % allocated[0])
            return [1, 'reset device failed']
        time.sleep(3)
        print 'program succeed'

    #do test operations
    at.device_run_cmd('A', 'netmgr clear')  #run 'netmgr clear' command at device A
    time.sleep(0.5)
    at.device_control('A', 'reset')              #control device A, let it reboot
    time.sleep(5)                                #wait some time
    at.device_run_cmd('A', 'netmgr connect {0} {1}'.format(ap_ssid, ap_pass)) #connect device A to wifi
    time.sleep(20)                               #wait some time

    filter = ['command']
    print 'before network cmd'
    response = at.device_run_cmd('A', 'network domain www.baidu.com', 1, 5, filter) #run 'network domain www.baidu.com' command at devcie A
    expected_respone = 'successed'
    print response

    if response is False or response == [] or expected_respone not in response[0]:
        return [1, 'reponse error']

    at.stop()
    return [0, response[0]]

if __name__ == '__main__':
    [code, msg] = main()
    sys.exit(code)
