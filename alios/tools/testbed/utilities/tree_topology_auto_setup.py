import sys, os, time
sys.path.append('../../')
from autotest import Autotest

ap_ssid = 'aos_test_01'
ap_pass = 'Alios@Embedded'

devices = {}
devices['01'] = 'mk3060-DN02QRJM'
devices['02'] = 'mk3060-DN02QRJN'
devices['03'] = 'mk3060-DN02QRJP'
devices['04'] = 'mk3060-DN02QRJQ'
devices['05'] = 'mk3060-DN02QRJR'
devices['06'] = 'mk3060-DN02QRJU'
devices['07'] = 'mk3060-DN02QRJX'
devices['08'] = 'mk3060-DN02QRJY'
devices['09'] = 'mk3060-DN02QRK3'
devices['10'] = 'mk3060-DN02QRK6'
devices['11'] = 'mk3060-DN02QRK7'
devices['12'] = 'mk3060-DN02QRKB'
devices['13'] = 'mk3060-DN02QRKE'
devices['14'] = 'mk3060-DN02QRKM'
devices['15'] = 'mk3060-DN02QRKQ'

device_list = list(devices)
device_list.sort()
device_attr={}
at=Autotest()
if at.start('10.125.52.132', 34568) == False:
    print 'error: start failed'
    exit(1)
if at.device_subscribe(devices) == False:
    print 'error: subscribe to device failed, some devices may not exist in testbed'
    exit(1)

#reboot and get device mac address
retry = 5
for device in device_list:
    succeed = False
    for i in range(retry):
        at.device_control(device, 'reset')
        time.sleep(2.5)
        at.device_run_cmd(device, ['netmgr', 'clear'])
        at.device_run_cmd(device, ['kv', 'del', 'alink'])
        mac =  at.device_run_cmd(device, ['reboot'], 1, 1.5, ['mac'])
        if mac != False and mac != []:
            mac = mac[0].replace('mac ', '')
            mac = mac.replace(':', '')
            mac = mac.replace(' ', '0')
            device_attr[device] = {'mac':mac}
            succeed = True
            break;
    if succeed == False:
        print 'error: reboot and get mac addr for device {0} failed'.format(device)
        exit(1)
time.sleep(5)

bytes = os.urandom(6)
extnetid = ''
for byte in bytes:
    extnetid = extnetid + '{0:02x}'.format(ord(byte))
for device in device_list:
    at.device_run_cmd(device, ['umesh', 'extnetid', extnetid])
    at.device_run_cmd(device, ['umesh', 'stop'])

#setup tree topology
print 'topology:'
print '               01'
print '            /     \\'
print '      02               03'
print '   /      \\         /   \\'
print '  04      05       06     07\n'
print ' /  \\   /  \\    /  \\    /  \\'
print '08  09 10   11   12   13  14   15\n'
device = '01'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['02']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['03']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '02'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['01']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['04']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['05']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '03'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['01']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['06']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['07']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '04'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['02']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['08']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['09']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '05'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['02']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['10']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['11']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['12']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '06'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['03']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['13']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '07'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['03']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['14']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['15']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '08'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['04']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '09'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['04']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '10'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['05']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '11'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['05']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '12'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['05']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '13'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['06']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '14'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['07']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])
device = '15'
at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr['07']['mac']+'0000'])
at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
at.device_run_cmd(device, ['umesh', 'whitelist'])

#start devices
filter = ['disabled', 'detached', 'attached', 'leaf', 'router', 'super_router', 'leader', 'unknown']
for i in range(len(device_list)):
    device = device_list[i]
    if i == 0:
        at.device_run_cmd(device, ['netmgr', 'connect', ap_ssid, ap_pass])
        time.sleep(12)
        uuid = at.device_run_cmd(device, ['uuid'], 1, 1)
        if uuid == []:
            print 'error: alink connect to server failed'
            exit(1)
    else:
        at.device_run_cmd(device, ['umesh', 'start'])
        time.sleep(5)

    if i == 0:
        expected_state = 'leader'
    else:
        expected_state = 'router'

    succeed = False; retry = 5
    while retry > 0:
        state = at.device_run_cmd(device, ['umesh', 'state'], 1, 1.5, filter)
        if state == [expected_state]:
            succeed = True
            break
        at.device_run_cmd(device, ['umesh', 'stop'], 1, 1)
        at.device_run_cmd(device, ['umesh', 'start'], 5, 1)
        time.sleep(5)
        retry -= 1
    if succeed == True:
        print '{0} connect to mesh as {1} succeed'.format(device, expected_state)
    else:
        print 'error: {0} connect to mesh as {1} failed'.format(device, expected_state)
        exit(1)

    succeed = False; retry = 3
    while retry > 0:
        ipaddr = at.device_run_cmd(device, ['umesh', 'ipaddr'], 2, 1, ['.'])
        if ipaddr == False or ipaddr == [] or len(ipaddr) != 2:
            continue
        ipaddr[0] = ipaddr[0].replace('\t', '')
        ipaddr[1] = ipaddr[1].replace('\t', '')
        device_attr[device]['ipaddr'] = ipaddr[0:2]
        succeed = True
        break;
    if succeed == False:
        print 'error: get ipaddr for device {0} failed'.format(device)
        exit(1)
for device in device_list:
    print "{0}:{1}".format(device, device_attr[device])

at.stop()

