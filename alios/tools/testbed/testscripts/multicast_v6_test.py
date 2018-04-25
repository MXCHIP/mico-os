import sys, os, time
sys.path.append('../')
from autotest import Autotest

ap_ssid = 'aos_test_01'
ap_pass = 'Alios@Embedded'

devices = {}
devices['A'] = 'mk3060-DN02QRIQ'
devices['B'] = 'mk3060-DN02QRIU'
devices['C'] = 'mk3060-DN02QRIX'
devices['D'] = 'mk3060-DN02QRJ3'

device_list = list(devices)
device_list.sort()
device_attr={}
at=Autotest()
logname=time.strftime('%Y-%m-%d@%H-%M')
logname = 'multicast-' + logname +'.log'
if at.start('10.125.52.132', 34568, logname) == False:
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

#setup topology
print 'topology:'
print 'router  leader  router  mobile'
print '  A <---> B <---> C <---> D'
for i in range(len(device_list)):
    device = device_list[i]
    at.device_run_cmd(device, ['umesh', 'whitelist', 'clear'])
    if (i-1) >= 0:
        prev_dev = device_list[i-1]
        at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr[prev_dev]['mac']+'0000'])
    if (i+1) < len(device_list):
        next_dev = device_list[i+1]
        at.device_run_cmd(device, ['umesh', 'whitelist', 'add', device_attr[next_dev]['mac']+'0000'])
    at.device_run_cmd(device, ['umesh', 'whitelist', 'enable'])
    at.device_run_cmd(device, ['umesh', 'whitelist'])

#start devices
#router leader router mobile
#  A <--> B <--> C <--> D
filter = ['disabled', 'detached', 'attached', 'leaf', 'router', 'super_router', 'leader', 'unknown']
for i in [1, 2, 3, 0]:
    device = device_list[i]
    if device == 'B':
        at.device_run_cmd(device, ['netmgr', 'connect', ap_ssid, ap_pass])
        time.sleep(12)
        uuid = at.device_run_cmd(device, ['uuid'], 1, 1)
        if uuid == []:
            print 'error: alink connect to server failed'
            exit(1)
    elif device == 'D':
        at.device_run_cmd(device, ['umesh', 'mode', 'MOBILE'])
        at.device_run_cmd(device, ['umesh', 'start'])
        time.sleep(5)
    else:
        at.device_run_cmd(device, ['umesh', 'start'])
        time.sleep(5)

    if device == 'B':
        expected_state = 'leader'
    elif device == 'D':
        expected_state = 'leaf'
    else:
        expected_state = 'router'

    succeed = False; retry = 5
    while retry > 0:
        state = at.device_run_cmd(device, ['umesh', 'state'], 1, 1, filter)
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
        ipaddr = at.device_run_cmd(device, ['umesh', 'ipaddr'], 3, 1.5, [':'])
        if ipaddr == False or ipaddr == [] or len(ipaddr) != 3:
            retry -= 1
            continue
        ipaddr[0] = ipaddr[0].replace('\t', '')
        ipaddr[1] = ipaddr[1].replace('\t', '')
        ipaddr[2] = ipaddr[2].replace('\t', '')
        device_attr[device]['ipaddr'] = ipaddr[0:3]
        succeed = True
        break;
    if succeed == False:
        print 'error: get ipaddr for device {0} failed'.format(device)
        exit(1)

for device in device_list:
    print "{0}:{1}".format(device, device_attr[device])

#udp multicast test
print 'test multicast connectivity:'
success_num = 0; fail_num = 0
for pkt_len in ['20', '500', '1000']:
    for device in device_list:
        dst_ip = device_attr[device]['ipaddr'][2]
        at.device_run_cmd(device, ['umesh', 'autotest', dst_ip, '1', pkt_len])
        time.sleep(4)
        response = at.device_run_cmd(device, ['umesh', 'testcmd', 'autotest_acked'], 1, 1, ['3'])
        if response == [] or len(response) != 1 or '3' not in response[0]:
            print '{0} multicast {1} bytes failed'.format(device, pkt_len)
            fail_num += 1
        else:
            success_num += 1
print 'udp: succeed-{0}, failed-{1}'.format(success_num, fail_num)

at.stop()

