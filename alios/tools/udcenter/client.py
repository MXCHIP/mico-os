import os, sys, time, platform, json, traceback, random, re, glob, uuid
import socket, ssl, thread, threading, subprocess, signal, Queue, importlib
import select
from os import path
import packet as pkt

MAX_MSG_LENGTH = 65536
ENCRYPT = True
DEBUG = True
EN_STATUS_POLL = True
LOCALLOG = False

def signal_handler(sig, frame):
    print "received SIGINT"
    raise KeyboardInterrupt

def queue_safeput(queue, item):
    try:
        queue.put(item, False)
    except:
        pass

class ConnectionLost(Exception):
    pass

class Client:
    def __init__(self):
        self.service_socket = None
        self.output_queue = Queue.Queue(256)
        self.debug_queue = Queue.Queue(256)
        self.devices = {}
        self.keep_running = True
        self.connected = False
        self.poll_str = '\x1b[t'
        bytes = os.urandom(4)
        for byte in bytes:
            self.poll_str += '{0:02x}'.format(ord(byte))
        self.poll_str += 'm'
        self.poll_interval = 60
        self.uuid = None
        self.model_interface = {}
        self.mesh_changed = [re.compile('become leader'),
                             re.compile('become detached'),
                             re.compile('allocate sid 0x[0-9a-f]{4}, become [0-9] in net [0-9a-f]{4}')]
        self.neighbor_changed = [re.compile('sid [0-9a-f]{4} mac [0-9a-f]{16} is replaced'),
                                 re.compile('[0-9a-f]{1,4} neighbor [0-9a-f]{16} become inactive')]
        self.device_uuid_changed = ["ACCS: connected",
                             "ACCS: disconnected",
                             'GATEWAY: connect to server succeed']

    def packet_send_thread(self):
        heartbeat_timeout = time.time() + 10
        while self.keep_running:
            try:
                [type, content] = self.output_queue.get(block=True, timeout=0.1)
            except Queue.Empty:
                type = None
                pass
            if self.service_socket == None:
                continue
            if type == None:
                if time.time() < heartbeat_timeout:
                    continue
                heartbeat_timeout += 10
                data = pkt.construct(pkt.HEARTBEAT,'')
            else:
                data = pkt.construct(type, content)
            try:
                self.service_socket.send(data)
            except:
                self.connected = False
                continue

    def send_packet(self, type, content, timeout=0.1):
        if self.service_socket == None:
            return False
        try:
            self.output_queue.put([type, content], True, timeout)
            return True
        except Queue.Full:
            print "error: ouput buffer full, drop packet [{0] {1}]".format(type, content)
        return False

    def debug_daemon_thread(self):
        debug_sessions = {}
        while self.keep_running:
            if debug_sessions == {} or self.debug_queue.empty() == False:
                [type, term, device] = self.debug_queue.get()
                if device not in self.devices:
                    content = term + ',' + device + ':' + 'nonexist'
                    self.send_packet(type, content)
                    continue
                interface = self.devices[device]['interface']
                if type == pkt.DEVICE_DEBUG_START:
                    if 'debug_socket' in self.devices[device]: #session exist
                        sock = self.devices[device]['debug_socket']
                        if debug_sessions[sock]['term'] != term:
                            content = term + ',' + device + ':' + 'busy'
                            self.send_packet(type, content)
                            continue
                        interface.debug_stop(device) #stop current session
                        debug_sessions.pop(sock)
                        sock.close()

                    if hasattr(interface, 'debug_start') == False or hasattr(interface, 'debug_stop') == False:
                        content = term + ',' + device + ':' + 'debug unsupported'
                        self.send_packet(type, content)
                        continue

                    used_ports = []
                    for session in debug_sessions:
                        used_ports.append(debug_sessions[session]['port'])
                    port = 3096 + ord(os.urandom(1)) * ord(os.urandom(1)) / 64
                    while port in used_ports:
                        port = 3096 + ord(os.urandom(1)) * ord(os.urandom(1)) / 64

                    result = interface.debug_start(device, port)
                    if result != 'success':
                        print "start st-util for {0} at port {1} failed, ret={2}".format(device, port, result)
                        content = term + ',' + device + ':' + result
                        self.send_packet(type, content)
                        continue

                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    try:
                        sock.connect(('127.0.0.1', port))
                        sock.setblocking(0)
                    except:
                        sock = None
                    if sock:
                        debug_sessions[sock] = {'term': term, 'device': device, 'port': port}
                        self.devices[device]['debug_socket'] = sock
                        content = term + ',' + device + ':' + 'success'
                        print 'start debugging device {0} at port {1} succeed'.format(device, port)
                    else:
                        if DEBUG: traceback.print_exc()
                        interface.debug_stop(device)
                        content = term + ',' + device + ':' + 'fail'
                        print 'start debugging device {0} failed'.format(device)
                    self.send_packet(type, content)
                    continue
                elif type in [pkt.DEVICE_DEBUG_REINIT, pkt.DEVICE_DEBUG_STOP]:
                    operations = {pkt.DEVICE_DEBUG_REINIT: 'reinit', pkt.DEVICE_DEBUG_STOP: 'stop'}
                    sock = None
                    for session in debug_sessions:
                        if debug_sessions[session]['term'] != term:
                            continue
                        if debug_sessions[session]['device'] != device:
                            continue
                        sock = session
                        break
                    if sock == None:
                        print 'error: try to {0} a nonexisting debug session'.format(operations[type])
                        content = term + ',' + device + ':' + 'session nonexist'
                        self.send_packet(type, content)
                        continue

                    port = debug_sessions[sock]['port']
                    interface.debug_stop(device)
                    sock.close()
                    if 'debug_socket' in self.devices[device]:
                        self.devices[device].pop('debug_socket')
                    debug_sessions.pop(sock)
                    if type == pkt.DEVICE_DEBUG_STOP:
                        content = term + ',' + device + ':' + 'success'
                        self.send_packet(type, content)
                        print 'stop debugging {0} succeed'.format(device)
                    elif type == pkt.DEVICE_DEBUG_REINIT:
                        result = interface.debug_start(device, port)
                        if result != 'success':
                            content = term + ',' + device + ':' + 'fail'
                            self.send_packet(type, content)
                            continue
                        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                        try:
                            sock.connect(('127.0.0.1', port))
                            sock.setblocking(0)
                        except:
                            if DEBUG: traceback.print_exc()
                            sock = None
                        if sock:
                            debug_sessions[sock] = {'term': term, 'device': device, 'port': port}
                            self.devices[device]['debug_socket'] = sock
                            content = term + ',' + device + ':' + 'success'
                            self.send_packet(type, content)
                            print 'restart debugging {0} succeed'.format(device)
                        else:
                            interface.debug_stop(device)
                            if 'debug_socket' in self.devices[device]:
                                self.devices[device].pop('debug_socket')
                            content = term + ',' + device + ':' + 'fail'
                            self.send_packet(type, content)
                            print 'restart debugging {0} failed, debug session closed'.format(device)
                    continue
                else:
                    print "error: wrong command type '{0}'".format(type)
                continue

            r, w, e = select.select(list(debug_sessions), [], [], 0.02)
            for sock in r:
                term = debug_sessions[sock]['term']
                device = debug_sessions[sock]['device']
                try:
                    data = sock.recv(MAX_MSG_LENGTH)
                except:
                    data = None
                if not data:
                    print 'debug session for {0}:{1} closed'.format(term, device)
                    interface = self.devices[device]['interface']
                    interface.debug_stop(device)
                    debug_sessions.pop(sock)
                    if 'debug_socket' in self.devices[device]:
                        self.devices[device].pop('debug_socket')
                    content = term + ',' + device + ':' + 'session closed'
                    self.send_packet(pkt.DEVICE_DEBUG_STOP, content)
                    continue
                content = term + ',' + device + ':' + data
                #print 'send:', content
                self.send_packet(pkt.DEVICE_DEBUG_DATA, content)
        print 'debug daemon thread exited'
        print 'keep_running = {0}'.format(self.keep_running)

    def send_device_list(self):
        device_list = []
        for device in list(self.devices):
            if self.devices[device]['valid']:
                device_list.append(device)
        content = ':'.join(device_list)
        self.send_packet(pkt.CLIENT_DEV, content)

    def send_device_status(self):
        for device in list(self.devices):
            if self.devices[device]['valid'] == False:
                continue
            content = device + ':' + json.dumps(self.devices[device]['attributes'], sort_keys=True)
            ret = self.send_packet(pkt.DEVICE_STATUS, content)
            if ret == False:
                break

    def run_poll_command(self, device, command, lines_expect, timeout):
        filter = {}
        response = []
        while self.devices[device]['plog_queue'].empty() == False:
            self.devices[device]['plog_queue'].get()
        self.devices[device]['handle'].write(self.poll_str + command + '\r')
        start = time.time()
        while True:
            try:
                log = self.devices[device]['plog_queue'].get(False)
            except:
                log = None
            if time.time() - start >= timeout:
                break
            if log == None:
                time.sleep(0.01)
                continue
            log = log.replace('\r', '')
            log = log.replace('\n', '')
            log = log.replace(self.poll_str, '')
            if log == '':
                continue
            response.append(log)
            if len(response) > lines_expect:
                break

        if len(response) > 0:
            response.pop(0)
        if not response:
            print "device {0} run poll commad '{1}' faild".format(device, command)
        return response

    def device_cmd_process(self, device, exit_condition):
        poll_fail_num = 0
        interface = self.devices[device]['interface']
        pcmd_queue = self.devices[device]['pcmd_queue']
        if self.devices[device]['attributes'] != {}:
            content = device + ':' + json.dumps(self.devices[device]['attributes'], sort_keys=True)
            self.send_packet(pkt.DEVICE_STATUS, content)
        poll_timeout = time.time() + 3 + random.uniform(0, self.poll_interval/10)
        while interface.exist(device) and exit_condition.is_set() == False:
            try:
                if EN_STATUS_POLL == True and time.time() >= poll_timeout:
                    poll_timeout += self.poll_interval
                    queue_safeput(pcmd_queue, ['devname', 1, 0.2])
                    queue_safeput(pcmd_queue, ['mac', 1, 0.2])
                    queue_safeput(pcmd_queue, ['version', 2, 0.2])
                    queue_safeput(pcmd_queue, ['uuid', 1, 0.2])
                    queue_safeput(pcmd_queue, ['umesh status', 11, 0.2])
                    queue_safeput(pcmd_queue, ['umesh extnetid', 1, 0.2])
                    queue_safeput(pcmd_queue, ['umesh nbrs', 35, 0.3])

                block=True
                timeout=0
                try:
                    args = None
                    if self.devices[device]['ucmd_queue'].empty() == True and pcmd_queue.empty() == True:
                        args = self.devices[device]['ucmd_queue'].get(block=True, timeout=0.1)
                    elif self.devices[device]['ucmd_queue'].empty() == False:
                        args = self.devices[device]['ucmd_queue'].get()
                except Queue.Empty:
                    args = None
                    continue
                except:
                    if DEBUG: traceback.print_exc()
                    args = None
                    continue

                if args != None:
                    type = args[0]
                    term = args[1]
                    if type == pkt.DEVICE_ERASE:
                        self.device_erase(device, term)
                    elif type == pkt.DEVICE_PROGRAM:
                        address = args[2]
                        filename = args[3]
                        self.device_program(device, address, filename, term)
                    elif type in [pkt.DEVICE_RESET, pkt.DEVICE_START, pkt.DEVICE_STOP]:
                        self.device_control(device, type, term)
                    elif type == pkt.DEVICE_CMD:
                        cmd = args[2]
                        self.device_run_cmd(device, cmd, term)
                        if re.search('umesh extnetid [0-9A-Fa-f]{12}', cmd) != None:
                            queue_safeput(pcmd_queue, ['umesh extnetid', 1, 0.2])
                    else:
                        print "error: unknown operation type {0}".format(repr(type))
                    args = None
                    time.sleep(0.05)
                    continue

                if pcmd_queue.empty() == True:
                    continue

                [cmd, lines, timeout] = pcmd_queue.get()
                response = self.run_poll_command(device, cmd, lines, timeout)

                if cmd == 'devname': #poll device model
                    if len(response) == lines and response[0].startswith('device name:'):
                        poll_fail_num = 0
                        self.devices[device]['attributes']['model'] = response[0].split()[-1]
                    else:
                        poll_fail_num += 1
                elif cmd == 'mac': #poll device mac
                    if len(response) == 1 and response[0].startswith('MAC address:'):
                        poll_fail_num = 0
                        macaddr = response[0].split()[-1]
                        macaddr = macaddr.replace('-', '') + '0000'
                        self.devices[device]['attributes']['macaddr'] = macaddr
                    else:
                        poll_fail_num += 1
                elif cmd == 'version': #poll device version
                    if len(response) == lines:
                        poll_fail_num = 0
                        for line in response:
                            if 'kernel version :' in line:
                                self.devices[device]['attributes']['kernel_version'] = line.replace('kernel version :AOS-', '')
                            if 'app version :' in line:
                                line = line.replace('app version :', '')
                                line = line.replace('app-', '')
                                line = line.replace('APP-', '')
                                self.devices[device]['attributes']['app_version'] = line
                    else:
                        poll_fail_num += 1
                elif cmd == 'umesh status': #poll mesh status
                    if len(response) == lines:
                        poll_fail_num = 0
                        for line in response:
                            if 'state\t' in line:
                                self.devices[device]['attributes']['state'] = line.replace('state\t', '')
                            elif '\tnetid\t' in line:
                                self.devices[device]['attributes']['netid'] = line.replace('\tnetid\t', '')
                            elif '\tsid\t' in line:
                                self.devices[device]['attributes']['sid'] = line.replace('\tsid\t', '')
                            elif '\tnetsize\t' in line:
                                self.devices[device]['attributes']['netsize'] = line.replace('\tnetsize\t', '')
                            elif '\trouter\t' in line:
                                self.devices[device]['attributes']['router'] = line.replace('\trouter\t', '')
                            elif '\tchannel\t' in line:
                                self.devices[device]['attributes']['channel'] = line.replace('\tchannel\t', '')
                    else:
                        poll_fail_num += 1
                elif cmd == 'umesh nbrs': #poll mesh nbrs
                    if len(response) > 0 and 'num=' in response[-1]:
                        poll_fail_num = 0
                        nbrs = {}
                        for line in response:
                            if '\t' not in line or ',' not in line:
                                continue
                            line = line.replace('\t', '')
                            nbr_info = line.split(',')
                            if len(nbr_info) < 10:
                                continue
                            nbrs[nbr_info[0]] = {'relation':nbr_info[1], \
                                                 'netid':nbr_info[2], \
                                                 'sid':nbr_info[3], \
                                                 'link_cost':nbr_info[4], \
                                                 'child_num':nbr_info[5], \
                                                 'channel':nbr_info[6], \
                                                 'reverse_rssi':nbr_info[7], \
                                                 'forward_rssi':nbr_info[8], \
                                                 'last_heard':nbr_info[9]}
                            if len(nbr_info) > 10:
                                nbrs[nbr_info[0]]['awake'] = nbr_info[10]
                        self.devices[device]['attributes']['nbrs'] = nbrs
                    else:
                        poll_fail_num += 1
                elif cmd == 'umesh extnetid': #poll mesh extnetid
                    if len(response) == 1 and response[0].count(':') == 5:
                        poll_fail_num += 1
                        self.devices[device]['attributes']['extnetid'] = response[0]
                    else:
                        poll_fail_num += 1
                elif cmd == 'uuid': #poll uuid
                    if len(response) == 1:
                        if 'uuid:' in response[0]:
                            poll_fail_num = 0
                            self.devices[device]['attributes']['uuid'] = response[0].replace('uuid: ', '')
                        elif 'alink is not connected' in response[0]:
                            poll_fail_num = 0
                            self.devices[device]['attributes']['uuid'] = 'N/A'
                        else:
                            poll_fail_num += 1
                    else:
                        poll_fail_num += 1
                else:
                    print "error: unrecognized poll cmd '{0}'".format(cmd)
                    continue

                if poll_fail_num >= 7:
                    if self.devices[device]['attributes']['status'] == 'active':
                        print "device {0} become inactive".format(device)
                    self.devices[device]['attributes']['status'] = 'inactive'
                else:
                    if self.devices[device]['attributes']['status'] == 'inactive':
                        print "device {0} become active".format(device)
                    self.devices[device]['attributes']['status'] = 'active'

                if pcmd_queue.empty() == False:
                    continue
                content = device + ':' + json.dumps(self.devices[device]['attributes'], sort_keys=True)
                self.send_packet(pkt.DEVICE_STATUS, content)
            except:
                if interface.exist(device) == False:
                    exit_condition.set()
                    break
                if exit_condition.is_set() == True:
                    break
                if DEBUG: traceback.print_exc()
                try:
                    self.devices[device]['handle'].close()
                    self.devices[device]['handle'].open()
                except:
                    exit_condition.set()
                    break
        print 'devie command process thread for {0} exited'.format(device)

    def device_log_filter(self, device, log):
        pcmd_queue = self.devices[device]['pcmd_queue']
        if EN_STATUS_POLL == False:
            return
        if pcmd_queue.full() == True:
            return
        for flog in self.mesh_changed:
            if flog.search(log) == None:
                continue
            #print log
            #print "device {0} mesh status changed".format(device)
            queue_safeput(pcmd_queue, ['umesh status', 11, 0.2])
            queue_safeput(pcmd_queue, ['umesh nbrs', 33, 0.3])
            return
        for flog in self.neighbor_changed:
            if flog.search(log) == None:
                continue
            #print log
            #print "device {0} neighbors changed".format(device)
            queue_safeput(pcmd_queue, ['umesh nbrs', 33, 0.3])
            return
        for flog in self.device_uuid_changed:
            if flog not in log:
                continue
            #print log
            #print "device {0} uuid changed".format(device)
            queue_safeput(pcmd_queue, ['uuid', 1, 0.2])
            return

    def device_log_poll(self, device, exit_condition):
        log_time = time.time()
        log = ''
        if LOCALLOG:
            logfile= path.join(path.expanduser('~'), '.udclient', path.basename(device) + '.log')
            flog = open(logfile, 'a+')
        interface = self.devices[device]['interface']
        while interface.exist(device) and exit_condition.is_set() == False:
            if self.connected == False or self.devices[device]['iolock'].locked():
                time.sleep(0.01)
                continue

            newline = False
            while self.devices[device]['iolock'].acquire(False) == True:
                try:
                    c = self.devices[device]['handle'].read(1)
                except:
                    c = ''
                finally:
                    self.devices[device]['iolock'].release()

                if c == '':
                    break

                if log == '':
                    log_time = time.time()
                log += c
                if c == '\n':
                    newline = True
                    break

            if newline == True and log != '':
                if self.poll_str in log:
                    queue_safeput(self.devices[device]['plog_queue'], log)
                else:
                    self.device_log_filter(device, log)
                if LOCALLOG:
                    flog.write('{0:.3f}:'.format(log_time) + log)
                log = device + ':{0:.3f}:'.format(log_time) + log
                self.send_packet(pkt.DEVICE_LOG,log)
                log = ''
        if LOCALLOG:
            flog.close()
        print 'device {0} removed'.format(device)
        self.devices[device]['valid'] = False
        exit_condition.set()
        try:
            self.devices[device]['handle'].close()
        except:
            pass
        self.send_device_list()
        print 'device log poll thread for {0} exited'.format(device)

    def add_new_device(self, mi, device):
        handle = mi.new_device(device)
        if handle == None:
            return False
        self.devices[device] = {
            'valid':True, \
            'handle':handle, \
            'interface' : mi, \
            'iolock':threading.Lock(), \
            'attributes':{}, \
            'ucmd_queue':Queue.Queue(12), \
            'pcmd_queue':Queue.Queue(64), \
            'plog_queue':Queue.Queue(64)
            }
        self.devices[device]['attributes']['status'] = 'inactive'
        return True

    def add_old_device(self, mi, device):
        ser = mi.new_device(device)
        if ser == None:
            return False
        self.devices[device]['handle'] = ser
        if self.devices[device]['iolock'].locked():
            self.devices[device]['iolock'].release()
        while self.devices[device]['ucmd_queue'].empty() == False:
            self.devices[device]['ucmd_queue'].get()
        while self.devices[device]['pcmd_queue'].empty() == False:
            self.devices[device]['pcmd_queue'].get()
        while self.devices[device]['plog_queue'].empty() == False:
            self.devices[device]['plog_queue'].get()
        self.devices[device]['attributes']['status'] = 'inactive'
        self.devices[device]['valid'] = True
        return True

    def list_devices(self):
        os = platform.system()
        devices_new = []
        for model in self.model_interface:
            mi = self.model_interface[model]

            devices = mi.list_devices(os)
            for device in devices:
                if device in self.devices and self.devices[device]['valid'] == True:
                    continue
                if device not in self.devices:
                    ret = self.add_new_device(mi, device)
                else:
                    ret = self.add_old_device(mi, device)
                if ret == True:
                    devices_new.append(device)

        devices_new.sort()
        return devices_new

    def device_monitor(self):
        while self.keep_running:
            devices_new = self.list_devices()
            for device in devices_new:
                print 'device {0} added'.format(device)
                exit_condition = threading.Event()
                thread.start_new_thread(self.device_log_poll, (device, exit_condition,))
                thread.start_new_thread(self.device_cmd_process, (device, exit_condition,))
            if devices_new != []:
                self.send_device_list()
            time.sleep(0.5)
        print 'device monitor thread exited'
        self.keep_running = False

    def load_interfaces(self):
        board_dir = path.join(path.dirname(path.abspath(__file__)), 'board')
        candidates = os.listdir(board_dir)
        for d in candidates:
            if path.isdir(path.join(board_dir, d)) == False:
                continue
            model = path.basename(d)
            interface_file = path.join(board_dir, d, model+'.py')
            if path.isfile(interface_file) == False:
                continue
            sys.path.append(path.join(board_dir, d))
            try:
                self.model_interface[model] =  importlib.import_module(model)
            except:
                if DEBUG: traceback.print_exc()
                continue
            print 'model loaded - {0}'.format(model)

    def device_erase(self, device, term):
        interface = self.devices[device]['interface']
        self.devices[device]['iolock'].acquire()
        try:
            ret = interface.erase(device)
        except:
            if DEBUG: traceback.print_exc()
            ret = 'fail'
        finally:
            self.devices[device]['iolock'].release()
        print 'erasing', device, '...', ret
        content = term + ',' + ret
        self.send_packet(pkt.DEVICE_ERASE, content)

    def device_program(self, device, address, file, term):
        if device not in self.devices:
            print "error: progamming nonexist device {0}".format(device)
            content = term + ',' + 'device nonexist'
            self.send_packet(pkt.DEVICE_PROGRAM, content)
            return

        interface = self.devices[device]['interface']
        self.devices[device]['iolock'].acquire()
        try:
            ret = interface.program(device, address, file)
        except:
            if DEBUG: traceback.print_exc()
            ret = 'fail'
        finally:
            self.devices[device]['iolock'].release()
        print 'programming', file, 'to', device, '@', address, '...', ret
        content = term + ',' + ret
        self.send_packet(pkt.DEVICE_PROGRAM, content)

    def device_control(self, device, type, term):
        operations= {pkt.DEVICE_RESET:'reset', pkt.DEVICE_STOP:'stop', pkt.DEVICE_START:'start'}
        if device not in self.devices:
            print "error: controlling nonexist device {0}".format(device)
            content = term + ',' + 'device nonexist'
            self.send_packet(type, content)
            return

        interface = self.devices[device]['interface']
        try:
            ret = interface.control(device, operations[type])
        except:
            if DEBUG: traceback.print_exc()
            ret = 'fail'
        print operations[type], device, ret
        content = term + ',' + ret
        self.send_packet(type, content)

    def device_run_cmd(self, device, cmd, term):
        if device not in self.devices:
            print "error: run command at nonexist device {0}".format(device)
            content = term + ',' + 'device nonexist'
            self.send_packet(pkt.DEVICE_CMD, content)
            return

        try:
            self.devices[device]['handle'].write(cmd+'\r')
            result='success'
            print "run command '{0}' at {1} succeed".format(cmd, device)
        except:
            if DEBUG: traceback.print_exc()
            result='fail'
            print "run command '{0}' at {1} failed".format(cmd, device)
        content = term + ',' + result
        self.send_packet(pkt.DEVICE_CMD, content)

    def login_and_get_server(self, controller_ip, controller_port):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if ENCRYPT:
            cert_file = path.join(path.dirname(path.abspath(__file__)), 'controller_certificate.pem')
            sock = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED, ca_certs = cert_file)
        try:
            sock.connect((controller_ip, controller_port))
            sock.settimeout(3)
            msg = ''
        except:
            if DEBUG: traceback.print_exc()
            return None

        content = pkt.construct(pkt.ACCESS_LOGIN, 'client,' + self.uuid)
        try:
            sock.send(content)
        except:
            if DEBUG: traceback.print_exc()
            sock.close()
            return None

        try:
            data = sock.recv(MAX_MSG_LENGTH)
        except KeyboardInterrupt:
            sock.close()
            raise
        except:
            sock.close()
            return None

        if data == '':
            sock.close()
            return None

        type, length, value, data = pkt.parse(data)
        #print 'controller', type, value
        rets = value.split(',')
        if type != pkt.ACCESS_LOGIN or rets[0] != 'success':
            accesskey_file = path.join(path.expanduser('~'), '.udclient', '.accesskey')
            if rets[0] == 'invalid access key' and path.exists(accesskey_file):
                os.remove(accesskey_file)
            print "login failed, ret={0}".format(value)
            sock.close()
            return None

        sock.close()
        return rets[1:]

    def connect_to_server(self, server_ip, server_port, certificate):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if certificate != 'None':
            try:
                certfile = path.join(path.expanduser('~'), '.udclient', 'certificate.pem')
                f = open(certfile, 'wt')
                f.write(certificate)
                f.close()
                sock = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED, ca_certs=certfile)
            except:
                if DEBUG: traceback.print_exc()
                return 'fail'
        try:
            sock.connect((server_ip, server_port))
            while self.output_queue.empty() == False:
                self.output_queue.get()
            self.service_socket = sock
            self.connected = True
            return "success"
        except:
            if DEBUG: traceback.print_exc()
            return "fail"

    def client_func(self, contoller_ip, controller_port):
        work_dir = path.join(path.expanduser('~'), '.udclient')
        if path.exists(work_dir) == False:
            os.mkdir(work_dir)
        signal.signal(signal.SIGINT, signal_handler)

        accesskey_file = path.join(path.expanduser('~'), '.udclient', '.accesskey')
        if path.exists(accesskey_file) == True:
            try:
                file = open(accesskey_file, 'rb')
                self.uuid = json.load(file)
                file.close()
            except:
                print "read access key failed"
        while self.uuid == None:
            try:
                uuid = raw_input("please input your access key: ")
            except:
                return
            is_valid_uuid = re.match('^[0-9a-f]{10,20}$', uuid)
            if is_valid_uuid == None:
                print "invalid access key {0}, please enter a valid access key".format(uuid)
                continue
            self.uuid = uuid
            try:
                file = open(accesskey_file, 'wb')
                json.dump(self.uuid, file)
                file.close()
            except:
                print "error: save access key to file failed"

        self.load_interfaces()

        thread.start_new_thread(self.packet_send_thread, ())
        thread.start_new_thread(self.debug_daemon_thread, ())
        thread.start_new_thread(self.device_monitor, ())

        file_received = {}
        file_receiving = {}
        self.connected = False
        self.service_socket = None
        msg = ''
        while True:
            try:
                if self.connected == False:
                    raise ConnectionLost

                new_msg = self.service_socket.recv(MAX_MSG_LENGTH)
                if new_msg == '':
                    raise ConnectionLost
                    break

                msg += new_msg
                while msg != '':
                    type, length, value, msg = pkt.parse(msg)
                    if type == pkt.TYPE_NONE:
                        break

                    for hash in list(file_receiving):
                        if time.time() < file_receiving[hash]['timeout']:
                            continue
                        file_receiving[hash]['handle'].close()
                        try:
                            os.remove(file_receiving[hash]['name'])
                        except:
                            pass
                        file_receiving.pop(hash)

                    #print type, value
                    if type == pkt.FILE_BEGIN:
                        try:
                            [term, hash, fname] = value.split(':')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue
                        if hash in file_received:
                            if path.exists(file_received[hash]) == True:
                                content = term + ',' + 'exist'
                                self.send_packet(type, content)
                                continue
                            else:
                                file_received.pop(hash)

                        if hash in file_receiving:
                            content = term + ',' + 'busy'
                            self.send_packet(type, content)
                            print "busy: refused to recive {0}:{1}".format(filename, hash)
                            continue

                        filename = path.join(path.expanduser('~'), '.udclient', path.basename(fname))
                        filename += '-' + term + '@' + time.strftime('%Y%m%d-%H%M%S')
                        filehandle = open(filename, 'wb')
                        timeout = time.time() + 5
                        file_receiving[hash] = {'name':filename, 'seq':0, 'handle':filehandle, 'timeout': timeout}
                        content = term + ',' + 'ok'
                        self.send_packet(type, content)
                        if DEBUG:
                            print 'start receiving {0} as {1}'.format(fname, filename)
                    elif type == pkt.FILE_DATA:
                        try:
                            split_value = value.split(':')
                            term = split_value[0]
                            hash = split_value[1]
                            seq  = split_value[2]
                            data = value[(len(term) + len(hash) + len(seq) + 3):]
                            seq = int(seq)
                        except:
                            print "argument error: {0}".format(type)
                            continue
                        if hash not in file_receiving:
                            content = term + ',' + 'noexist'
                            self.send_packet(type, content)
                            print "error: drop data fragment {0}:{1}, hash not in receiving file".format(hash, seq)
                            continue
                        if file_receiving[hash]['seq'] != seq and file_receiving[hash]['seq'] != seq + 1:
                            content = term + ',' + 'seqerror'
                            self.send_packet(type, content)
                            print "error: drop data fragment {0}:{1}, sequence error".format(hash, seq)
                            continue
                        if file_receiving[hash]['seq'] == seq:
                            file_receiving[hash]['handle'].write(data)
                            file_receiving[hash]['seq'] += 1
                            file_receiving[hash]['timeout'] = time.time() + 5
                        content = term + ',' + 'ok'
                        self.send_packet(type, content)
                    elif type == pkt.FILE_END:
                        try:
                            [term, hash, fname] = value.split(':')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue
                        if hash not in file_receiving:
                            content = term + ',' + 'noexist'
                            self.send_packet(type, content)
                            continue
                        file_receiving[hash]['handle'].close()
                        localhash = pkt.hash_of_file(file_receiving[hash]['name'])
                        if localhash != hash:
                            response = 'hasherror'
                        else:
                            response = 'ok'
                            file_received[hash] = file_receiving[hash]['name']
                        if DEBUG:
                            print 'finished receiving {0}, result:{1}'.format(file_receiving[hash]['name'], response)
                        file_receiving.pop(hash)
                        content = term + ',' + response
                        self.send_packet(type, content)
                    elif type == pkt.DEVICE_ERASE:
                        try:
                            [term, device] = value.split(',')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue
                        if device in self.devices:
                            if self.devices[device]['ucmd_queue'].full() == False:
                                self.devices[device]['ucmd_queue'].put([type, term])
                                continue
                            else:
                                result = 'busy'
                                print 'erase', device,  'failed, device busy'
                        else:
                            result = 'nonexist'
                            print 'erase', device,  'failed, device nonexist'
                        content = term + ',' + result
                        self.send_packet(type, content)
                    elif type == pkt.DEVICE_PROGRAM:
                        try:
                            [term, device, address, hash] = value.split(',')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue
                        if hash not in file_received:
                            content = term + ',' + 'error'
                            self.send_packet(type, content)
                            continue
                        filename = file_received[hash]
                        if device in self.devices:
                            if self.devices[device]['ucmd_queue'].full() == False:
                                self.devices[device]['ucmd_queue'].put([type, term, address, filename])
                                continue
                            else:
                                result = 'busy'
                                print 'program {0} to {1} @ {2} failed, device busy'.format(filename, device, address)
                        else:
                            result = 'error'
                            print 'program {0} to {1} @ {2} failed, device nonexist'.format(filename, device, address)
                        content = term + ',' + result
                        self.send_packet(type, content)
                    elif type in [pkt.DEVICE_RESET, pkt.DEVICE_START, pkt.DEVICE_STOP]:
                        operations = {pkt.DEVICE_RESET:'reset', pkt.DEVICE_START:'start', pkt.DEVICE_STOP:'stop'}
                        try:
                            [term, device] = value.split(',')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue
                        if device in self.devices and self.devices[device]['valid'] == True:
                            if self.devices[device]['ucmd_queue'].full() == False:
                                self.devices[device]['ucmd_queue'].put([type, term])
                                continue
                            else:
                                result = 'busy'
                                print operations[type], device, 'failed, device busy'
                        else:
                            result = 'nonexist'
                            print operations[type], device, 'failed, device nonexist'
                        content = term + ',' + result
                        self.send_packet(type, content)
                    elif type in [pkt.DEVICE_DEBUG_START, pkt.DEVICE_DEBUG_REINIT, pkt.DEVICE_DEBUG_STOP]:
                        operations = {
                            pkt.DEVICE_DEBUG_START:'start debuging',
                            pkt.DEVICE_DEBUG_START:'reinit debuging',
                            pkt.DEVICE_STOP:'stop debugging'}
                        try:
                            [term, device] = value.split(',')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue
                        debug_cmd = [type, term, device]
                        try:
                            self.debug_queue.put_nowait(debug_cmd)
                        except:
                            print "error: {0} debug session failed, debug_queue full".format(operations[type])
                            content = term + ',' + device + ':' + 'daemon busy'
                            self.send_packet(type, content)
                    elif type == pkt.DEVICE_CMD:
                        term_dev = value.split(':')[0]
                        term_dev_len = len(term_dev) + 1
                        try:
                            [term, device] = term_dev.split(',')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue
                        cmd = value[term_dev_len:].replace('|', ' ')
                        if device in self.devices and self.devices[device]['valid'] == True:
                            if self.devices[device]['ucmd_queue'].full() == False:
                                self.devices[device]['ucmd_queue'].put([type, term, cmd])
                                continue
                            else:
                                result = 'busy'
                                print "run command '{0}' at {1} failed, device busy".format(cmd, device)
                        else:
                            result = 'nonexist'
                            print "run command '{0}' at {1} failed, device nonexist".format(cmd, device)
                        content = term + ',' + result
                        self.send_packet(type, content)
                    elif type == pkt.DEVICE_DEBUG_DATA:
                        #print 'recv:', value[:80]
                        term_dev = value.split(':')[0]
                        term_dev_len = len(term_dev) + 1
                        try:
                            [term, device] = term_dev.split(',')
                        except:
                            print "argument error: {0} {1}".format(type, value)
                            continue

                        data = value[term_dev_len:]
                        if device not in self.devices or 'debug_socket' not in self.devices[device]:
                            content = term + ',' + device + ':' + 'session nonexist'
                            self.send_packet(pkt.DEVICE_DEBUG_STOP, content)
                            continue

                        try:
                            self.devices[device]['debug_socket'].send(data)
                        except:
                            if DEBUG: traceback.print_exc()
                            print "forward debug data to {1} failed".format(device)
                    elif type == pkt.CLIENT_LOGIN:
                        if value == 'request':
                            print 'server request login'
                            data = self.uuid + ',' + self.poll_str + ',' + token
                            self.send_packet(pkt.CLIENT_LOGIN, data)
                        elif value == 'success':
                            print "login to server succeed"
                            self.send_device_list()
                            self.send_device_status()
                        else:
                            print "login to server failed, retry later ..."
                            try:
                                time.sleep(10)
                            except KeyboardInterrupt:
                                break
                            raise ConnectionLost
            except ConnectionLost:
                self.connected = False
                if self.service_socket != None:
                    self.service_socket.close()
                    print 'connection to server lost, try reconnecting...'

                try:
                    result = self.login_and_get_server(contoller_ip, controller_port)
                except KeyboardInterrupt:
                    break
                if result == None:
                    print 'login to controller failed, retry later...'
                    try:
                        time.sleep(5)
                    except KeyboardInterrupt:
                        break
                    continue

                try:
                    [server_ip, server_port, token, certificate] = result
                    server_port = int(server_port)
                    print 'login to controller succees, server_ip-{0} server_port-{1}'.format(server_ip, server_port)
                except:
                    print 'login to controller failed, invalid return={0}'.format(result)
                    try:
                        time.sleep(5)
                    except KeyboardInterrupt:
                        break
                    continue

                result = self.connect_to_server(server_ip, server_port, certificate)
                if result == 'success':
                    print 'connect to server {0}:{1} succeeded'.format(server_ip, server_port)
                else:
                    print 'connect to server {0}:{1} failed, retry later ...'.format(server_ip, server_port)
                    try:
                        time.sleep(5)
                    except KeyboardInterrupt:
                        break
                    continue

                data = self.uuid + ',' + self.poll_str + ',' + token
                self.send_packet(pkt.CLIENT_LOGIN, data)
            except KeyboardInterrupt:
                break
            except:
                if DEBUG: traceback.print_exc()
        print "client exiting ..."
        self.keep_running = False
        time.sleep(0.3)
        if self.service_socket: self.service_socket.close()
