import sys, time               #import related python module
from autotest import Autotest  #import Autotest module

#connect to testbed server
logname=time.strftime('%Y-%m-%d@%H-%M')
logname = 'example-' + logname +'.log'    #specify log file name
server_ip = '10.125.52.132'               #specify testbed server ip
server_port = 34568                       #specify testbed server port
at=Autotest()
at.start(server_ip, server_port, logname) #connect to testbed server

#request allocating devices
model = 'mk3060'
number = 2
timeout = 60
allocted = at.device_allocate(model, number, timeout) #request to allocate 2 idle mk3060 devices
if len(allocted) != number:
    print "error: request device allocation failed"
    sys.exit(1)

#subscribe to allocated devices (neccessary step)
devices={'A':allocted[0], 'B':allocted[1]} #construct device list
if at.device_subscribe(devices) == False:  #subscribe to these devices
    print 'error: subscribe to device failed, some devices may not exist in testbed'
    sys.exit(1)

#do some test operations
at.device_run_cmd('A', 'netmgr clear')  #run 'netmgr clear' command at device A
at.device_control('A', 'reset')         #control device A, let it reboot
time.sleep(5)                           #wait some time
at.device_run_cmd('A', 'netmgr connect aos_test_01 Alios@Embedded') #connect device A to wifi
time.sleep(30)                          #wait some time
filter = ['disabled', 'detached', 'attached', 'leaf', 'router', 'super_router', 'leader', 'unknown']
print at.device_run_cmd('A', 'umesh state', 1, 0.5, filter) #run 'umesh state' command, and get returned mesh state

#disconnect testbed server
at.stop()

