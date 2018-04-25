import os, sys, time, socket, ssl, signal, re, Queue
import thread, threading, json, traceback, shutil
import TBframe as pkt

MAX_MSG_LENGTH = 65536
ENCRYPT = True
DEBUG = True

def signal_handler(sig, frame):
    print "received SIGINT"
    raise KeyboardInterrupt

class Server:
    def __init__(self):
        self.keyfile = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'key.pem')
        self.certfile = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'certificate.pem')
        self.output_queue = Queue.Queue(256)
        self.client_socket = None
        self.terminal_socket = None
        self.client_list = []
        self.terminal_list = []
        self.timeouts = {}
        self.keep_running = True
        self.log_preserve_period = (7 * 24 * 3600) * 3 #save log for 3 weeks
        self.allocated = {'lock':threading.Lock(), 'devices':[], 'timeout':0}
        self.special_purpose_set = {'mk3060-alink':[], 'esp32-alink':[], 'stm32-sal':[]}
        #mk3060
        self.special_purpose_set['mk3060-alink'] += ['mk3060-DN02QRKQ', 'mk3060-DN02RDVL', 'mk3060-DN02RDVT']
        self.special_purpose_set['mk3060-alink'] += ['mk3060-DN02RDVV', 'mk3060-DN02X2ZO', 'mk3060-DN02X2ZS']
        self.special_purpose_set['mk3060-alink'] += ['mk3060-DN02X2ZX', 'mk3060-DN02X2ZZ', 'mk3060-DN02X303']
        self.special_purpose_set['mk3060-alink'] += ['mk3060-DN02X304', 'mk3060-DN02X30B', 'mk3060-DN02X30H']
        #esp32
        self.special_purpose_set['esp32-alink'] += ['esp32-3.1.1', 'esp32-3.1.2', 'esp32-3.1.3', 'esp32-3.1.4']
        self.special_purpose_set['esp32-alink'] += ['esp32-3.2.1', 'esp32-3.2.2', 'esp32-3.2.3', 'esp32-3.2.4']
        self.special_purpose_set['esp32-alink'] += ['esp32-3.3.1', 'esp32-3.3.2', 'esp32-3.3.3', 'esp32-3.3.4']
        #esp32
        self.special_purpose_set['stm32-sal'] += ['stm32l432-0670FF504955857567182119']
        self.special_purpose_set['stm32-sal'] += ['stm32l432-0672FF494851877267084108']
        self.special_purpose_set['stm32-sal'] += ['stm32l432-0672FF504955857567113026']
        self.special_purpose_set['stm32-sal'] += ['stm32l432-0672FF535750877267212458']

        #devices never allocate to do autotest
        self.alloc_exclude_set = []
        #self.alloc_exclude_set += ['/dev/mk3060-DN02RDVV']
        #self.alloc_exclude_set += ['/dev/esp32-3.x.x']

    def packet_send_thread(self):
        while self.keep_running:
            try:
                [sock, type, content] = self.output_queue.get(block=True, timeout=0.1)
            except Queue.Empty:
                type = None
                continue
            data = pkt.construct(type, content)
            try:
                sock.send(data)
            except:
                continue

    def send_packet(self, sock, type, content, timeout=0.1):
        try:
            self.output_queue.put([sock, type, content], True, timeout)
            return True
        except Queue.Full:
            print "error: ouput buffer full, drop packet [{0] {1}]".format(type, content)
        return False

    def construct_dev_list(self):
        l = []
        for client in self.client_list:
            if client['valid'] == False:
                continue
            devices = client['uuid']
            for port in client['devices']:
                if client['devices'][port]['valid'] == False:
                    continue
                devices += ',' + port + '|' + str(client['devices'][port]['using'])
            l.append(devices)
        data = ':'.join(l)
        return data

    def send_device_list_to_terminal(self, terminal):
        devs = self.construct_dev_list()
        self.send_packet(terminal['socket'], pkt.ALL_DEV, devs)

    def send_device_list_to_all(self):
        devs = self.construct_dev_list()
        for t in self.terminal_list:
            self.send_packet(t['socket'], pkt.ALL_DEV, devs)

    def client_serve_thread(self, conn, addr):
        file = {}
        msg = ''
        client = None
        self.timeouts[conn] = {'type':'client', 'addr': addr, 'timeout': time.time() + 30}
        while self.keep_running:
            try:
                new_msg = conn.recv(MAX_MSG_LENGTH)
                if new_msg == '':
                    break

                msg += new_msg
                while msg != '':
                    type, length, value, msg = pkt.parse(msg)
                    if type == pkt.TYPE_NONE:
                        break

                    if conn in self.timeouts: self.timeouts[conn]['timeout'] = time.time() + 30
                    if client == None:
                        if type != pkt.CLIENT_UUID:
                            self.send_packet(conn, pkt.CLIENT_UUID, 'give me your uuid first')
                            time.sleep(0.1)
                            continue
                        for c in self.client_list:
                            if c['uuid'] != value:
                                continue
                            client = c
                            break
                        if client == None:
                            client = {'uuid':value,
                                      'valid':True,
                                      'socket':conn,
                                      'addr':addr,
                                      'devices':{}}
                            self.client_list.append(client)
                            print "new client {0} connected @ {1}".format(value, addr)
                        else:
                            client['socket'] = conn
                            client['addr'] = addr
                            client['valid'] = True
                            print "client {0} re-connected @ {1}".format(value, addr)
                        continue

                    if type == pkt.CLIENT_DEV:
                        new_devices = value.split(':')
                        for port in new_devices:
                            if port == "":
                                continue
                            if port in client['devices'] and client['devices'][port]['valid'] == True:
                                continue
                            if port not in client['devices']:
                                print "new device {0} added to client {1}".format(port, client['uuid'])
                                client['devices'][port] = {
                                        'lock':threading.Lock(),
                                        'valid':True,
                                        'using':0,
                                        'status':'{}',
                                        'log_subscribe':[],
                                        'status_subscribe':[]
                                        }
                            else:
                                print "device {0} re-added to client {1}".format(port, client['uuid'])
                                client['devices'][port]['status'] = '{}'
                                client['devices'][port]['valid'] = True

                        for port in list(client['devices']):
                            if port in new_devices:
                                continue
                            if client['devices'][port]['valid'] == False:
                                continue
                            client['devices'][port]['status'] = '{}'
                            client['devices'][port]['valid'] = False
                            print "device {0} removed from client {1}".format(port, client['uuid'])

                        for port in list(file):
                            if client['devices'][port]['valid'] == True:
                                continue
                            file[port]['handle'].close()
                            file.pop(port)
                        self.send_device_list_to_all()
                    elif type == pkt.DEVICE_LOG:
                        port = value.split(':')[0]
                        if port not in client['devices']:
                            continue
                        #forwad log to subscribed devices
                        if client['devices'][port]['log_subscribe'] != [] and \
                           ('tag' not in client or client['tag'] not in value):
                            log = client['uuid'] + ',' + value
                            for s in client['devices'][port]['log_subscribe']:
                                self.send_packet(s, type, log)

                        #save log to files
                        try:
                            logtime = value.split(':')[1]
                            logstr = value[len(port) + 1 + len(logtime):]
                            logtime = float(logtime)
                            logtimestr = time.strftime("%Y-%m-%d@%H:%M:%S", time.localtime(logtime))
                            logtimestr += ("{0:.3f}".format(logtime-int(logtime)))[1:]
                            logstr = logtimestr + logstr
                            logdatestr = time.strftime("%Y-%m-%d", time.localtime(logtime))
                        except:
                            if DEBUG: traceback.print_exc()
                            continue
                        if (port not in file) or (file[port]['date'] != logdatestr):
                            if port in file:
                                file[port]['handle'].close()
                                file.pop(port)
                            log_dir = 'server/' + logdatestr
                            if os.path.isdir(log_dir) == False:
                                try:
                                    os.mkdir(log_dir)
                                except:
                                    print "error: can not create directory {0}".format(log_dir)
                            filename = log_dir + '/' + client['uuid'] + '-' + port.split('/')[-1]  + '.log'
                            try:
                                handle = open(filename, 'a+')
                                file[port] = {'handle':handle, 'date': logdatestr}
                            except:
                                print "error: can not open/create file ", filename
                        if port in file and file[port]['date'] == logdatestr:
                            file[port]['handle'].write(logstr)
                    elif type == pkt.DEVICE_STATUS:
                        #print value
                        port = value.split(':')[0]
                        if port not in client['devices']:
                            continue
                        client['devices'][port]['status'] = value[len(port)+1:]
                        if client['devices'][port]['status_subscribe'] != []:
                            log = client['uuid'] + ',' + port
                            log += value[len(port):]
                            for s in client['devices'][port]['status_subscribe']:
                                self.send_packet(s, type, log)
                    elif type == pkt.RESPONSE:
                        values = value.split(',')
                        addr = (values[0], int(values[1]))
                        terminal = None
                        for t in self.terminal_list:
                            if t['addr'] == addr:
                                terminal = t
                        if terminal != None:
                            self.send_packet(terminal['socket'], type, ','.join(values[2:]))
                    elif type == pkt.CLIENT_TAG:
                        client['tag'] = value
                        print 'client {0} tag: {1}'.format(client['uuid'],repr(value))
            except:
                if DEBUG: traceback.print_exc()
                break
        conn.close()
        if conn in self.timeouts: self.timeouts.pop(conn)
        if client:
            for port in client['devices']:
                if client['devices'][port]['valid'] == False:
                    continue
                client['devices'][port]['status'] = '{}'
                client['devices'][port]['valid'] = False
                print "device {0} removed from client {1}".format(port, client['uuid'])
            client['valid'] = False
            print "client {0} @ {1} disconnected".format(client['uuid'], addr)
            self.send_device_list_to_all()
        else:
            print "client @ {0} disconnected".format(addr)

    def send_file_to_someone(self, dst, filename):
        if os.path.exists(filename) == False:
            print "{0} does not exist\n".format(filename)
            return False
        print "sending {0} to {1} ...".format(filename, dst['addr']),
        file = open(filename,'r')
        content = filename.split('/')[-1]
        sock = dst['socket']
        ret = self.send_packet(sock, pkt.FILE_BEGIN, content)
        if ret == False:
            print "failed"
            return False

        content = file.read(1024)
        while(content):
            ret = self.send_packet(sock, pkt.FILE_DATA, content)
            if ret == False:
                print "failed"
                return False
            content = file.read(1024)
        file.close()
        content = filename.split('/')[-1]
        ret = self.send_packet(sock, pkt.FILE_END, content)
        if ret == False:
            print "failed"
            return False
        print "succeed"
        return True

    def get_client_by_uuid(self, uuid):
        ret = None
        for client in self.client_list:
            if client['uuid'] != uuid:
                continue
            if client['valid'] == False:
                continue
            ret = client
            break
        return ret

    def allocate_devices(self, value):
        values = value.split(',')
        if len(values) < 2:
            return ['error','argument']

        model = values[0]
        model = model.lower()

        number = values[1]
        try:
            number = int(number)
        except:
            return ['error','argument']
        if number <= 0:
            return ['error','argument']

        purpose = 'general'
        if len(values) > 2:
            purpose = values[2]
        func_set = None
        if purpose != 'general':
            func_set = model + '-' + purpose
            if func_set not in self.special_purpose_set:
                print "error: allocate {0} for {1} purpose not supported".format(model, purpose)
                return ['error','argument']
            func_set = self.special_purpose_set[func_set]
        #if DEBUG and func_set: print purpose, func_set

        allocated = []
        for client in self.client_list:
            allocated = []
            ports = list(client['devices'])
            ports.sort()
            for port in ports:
                if client['devices'][port]['valid'] == False: #no exist
                    continue

                if client['devices'][port]['using'] != 0: #busy
                    continue

                if port in self.alloc_exclude_set: # in alloc_exclude_set
                    continue

                if port in self.allocated['devices']: #in allocated buffer
                    continue

                try:
                    status = json.loads(client['devices'][port]['status'])
                except:
                    print 'parse {0} status failed'.format(port)
                    status = None
                if status and 'model' in status:
                    if model != status['model'].lower():
                        continue
                else:
                    paths = {'mk3060':'mk3060-', 'esp32':'esp32-', 'stm32':'stm32l43'}
                    if model not in paths:
                        continue
                    pathstr = paths[model]
                    if pathstr not in port:
                        continue

                if func_set:
                    match = False
                    for devicestr in func_set:
                        if devicestr not in port:
                            continue
                        match = True
                        break
                    if match == False:
                        continue

                allocated.append(port)
                if len(allocated) >= number:
                    break
            if len(allocated) >= number:
                break
        if len(allocated) >= number:
            with self.allocated['lock']:
                self.allocated['devices'] += allocated
                self.allocated['timeout'] = time.time() + 10
            if DEBUG: print "allocated", allocated
            return ['success', '|'.join(allocated)]
        else:
            if DEBUG:
                print "allocate failed, infomation:"
                print "    model    : {0}".format(model)
                print "    number   : {0}".format(number)
                print "    purpose  : {0}".format(purpose)
                print "    allocated: {0}".format(self.allocated)
                print "    func_set : {0}".format(repr(func_set))
                print "    devices:"
                for client in self.client_list:
                    ports = list(client['devices'])
                    ports.sort()
                    for port in ports:
                        if client['devices'][port]['valid'] == False: #no exist
                            continue
                        if model not in port:
                            continue
                        print "        {0} {1}".format(port, client['devices'][port])
            return ['fail', 'busy']

    def increase_device_refer(self, client, port, using_list):
        if [client['uuid'], port] not in using_list:
            if port in list(client['devices']):
                with client['devices'][port]['lock']:
                    client['devices'][port]['using'] += 1
                using_list.append([client['uuid'], port])
                self.send_device_list_to_all()

    def terminal_serve_thread(self, conn, addr):
        terminal = {'socket':conn, 'addr':addr}
        self.terminal_list.append(terminal)
        using_list = []
        msg = ''
        self.timeouts[conn] = {'type': 'terminal', 'addr': addr, 'timeout': time.time() + 30}
        self.send_device_list_to_terminal(terminal)
        while self.keep_running:
            try:
                new_msg = terminal['socket'].recv(MAX_MSG_LENGTH);
                if new_msg == '':
                    break

                msg += new_msg
                while msg != '':
                    type, length, value, msg = pkt.parse(msg)
                    if type == pkt.TYPE_NONE:
                        break

                    if conn in self.timeouts: self.timeouts[conn]['timeout'] = time.time() + 30
                    if type == pkt.FILE_BEGIN or type == pkt.FILE_DATA or type == pkt.FILE_END:
                        dev_str = value.split(':')[0]
                        uuid = dev_str.split(',')[0]
                        target_data = value[len(dev_str):]
                        client = self.get_client_by_uuid(uuid)
                        if client == None:
                            content = ','.join([type, 'nonexist'])
                            self.send_packet(terminal['socket'], pkt.RESPONSE, content)
                            continue
                        content  = terminal['addr'][0] + ',' + str(terminal['addr'][1])
                        content += target_data
                        self.send_packet(client['socket'], type, content)
                    elif type == pkt.DEVICE_ERASE or type == pkt.DEVICE_PROGRAM or \
                         type == pkt.DEVICE_START or type == pkt.DEVICE_STOP or \
                         type == pkt.DEVICE_RESET or type == pkt.DEVICE_CMD:
                        dev_str_split = value.split(':')[0].split(',')[0:2]
                        if len(dev_str_split) != 2:
                            content = ','.join([type, 'argerror'])
                            self.send_packet(terminal['socket'], pkt.RESPONSE, content)
                            continue
                        [uuid, port] = dev_str_split
                        client = self.get_client_by_uuid(uuid)
                        if client == None:
                            content = ','.join([type, 'nonexist'])
                            self.send_packet(terminal['socket'], pkt.RESPONSE, content)
                            continue
                        content = terminal['addr'][0]
                        content += ',' + str(terminal['addr'][1])
                        content += value[len(uuid):]
                        self.send_packet(client['socket'], type, content)
                        self.increase_device_refer(client, port, using_list)
                    elif type == pkt.DEVICE_ALLOC:
                        result = self.allocate_devices(value)
                        content = type + ',' + ','.join(result)
                        self.send_packet(terminal['socket'], pkt.RESPONSE, content)
                    elif type == pkt.LOG_SUB or type == pkt.LOG_UNSUB or \
                         type == pkt.STATUS_SUB or type == pkt.STATUS_UNSUB:
                        dev_str_split = value.split(',')
                        if len(dev_str_split) != 2:
                            continue
                        [uuid, port] = dev_str_split
                        client = self.get_client_by_uuid(uuid)
                        if client == None:
                            continue
                        if port not in list(client['devices']):
                            continue
                        if type == pkt.LOG_SUB:
                            if terminal['socket'] in client['devices'][port]['log_subscribe']:
                                continue
                            client['devices'][port]['log_subscribe'].append(terminal['socket'])
                            print "terminal {0}:{1}".format(terminal['addr'][0], terminal['addr'][1]),
                            print "subscribed log of device {0}:{1}".format(uuid, port)
                        elif type == pkt.LOG_UNSUB:
                            if terminal['socket'] not in client['devices'][port]['log_subscribe']:
                                continue
                            client['devices'][port]['log_subscribe'].remove(terminal['socket'])
                            print "terminal {0}:{1}".format(terminal['addr'][0], terminal['addr'][1]),
                            print "unsubscribed log of device {0}:{1}".format(uuid, port)
                        elif type == pkt.STATUS_SUB:
                            if terminal['socket'] in client['devices'][port]['status_subscribe']:
                                continue
                            client['devices'][port]['status_subscribe'].append(terminal['socket'])
                            print "terminal {0}:{1}".format(terminal['addr'][0], terminal['addr'][1]),
                            print "subscribed status of device {0}:{1}".format(uuid, port)
                            content = client['uuid'] + ',' + port
                            content += ':' + client['devices'][port]['status']
                            self.send_packet(terminal['socket'], pkt.DEVICE_STATUS, content)
                        elif type == pkt.STATUS_UNSUB:
                            if terminal['socket'] not in client['devices'][port]['status_subscribe']:
                                continue
                            client['devices'][port]['status_subscribe'].remove(terminal['socket'])
                            print "terminal {0}:{1}".format(terminal['addr'][0], terminal['addr'][1]),
                            print "unsubscribed status of device {0}:{1}".format(uuid, port)
                    elif type == pkt.LOG_DOWNLOAD:
                        dev_str_split = value.split(',')
                        if len(dev_str_split) != 2:
                            continue
                        [uuid, port] = dev_str_split
                        datestr = time.strftime('%Y-%m-%d')
                        filename = 'server/' + datestr + '/' + uuid + '-' + port.split('/')[-1] + '.log'
                        client = self.get_client_by_uuid(uuid)
                        if client == None or port not in list(client['devices']) or os.path.exists(filename) == False:
                            content = ','.join([type, 'fail'])
                            self.send_packet(terminal['socket'], pkt.RESPONSE, content)
                            print "terminal {0}:{1}".format(terminal['addr'][0], terminal['addr'][1]),
                            print "downloading log of device {0}:{1} ... failed".format(uuid, port)
                            continue
                        self.send_file_to_someone(terminal, filename)
                        heartbeat_timeout = time.time() + 30
                        content = ','.join([type, 'success'])
                        self.send_packet(terminal['socket'], pkt.RESPONSE, content)
                        print "terminal {0}:{1}".format(terminal['addr'][0], terminal['addr'][1]),
                        print "downloading log of device {0}:{1} ... succeed".format(uuid, port)
            except:
                if DEBUG: traceback.print_exc()
                break
        for client in self.client_list:
            for port in list(client['devices']):
                if terminal['socket'] in client['devices'][port]['log_subscribe']:
                    client['devices'][port]['log_subscribe'].remove(terminal['socket'])
                if terminal['socket'] in client['devices'][port]['status_subscribe']:
                    client['devices'][port]['status_subscribe'].remove(terminal['socket'])
        for device in using_list:
            uuid = device[0]
            port = device[1]
            client = None
            for c in self.client_list:
                if c['uuid'] != uuid:
                    continue
                client = c
                break
            if client != None and port in list(client['devices']):
                with client['devices'][port]['lock']:
                    if client['devices'][port]['using'] > 0:
                        client['devices'][port]['using'] -= 1
        terminal['socket'].close()
        if conn in self.timeouts: self.timeouts.pop(conn)
        print "terminal ", terminal['addr'], "disconnected"
        self.terminal_list.remove(terminal)
        self.send_device_list_to_all()

    def client_listen_thread(self):
        self.client_socket.listen(5)
        if ENCRYPT:
            self.client_socket = ssl.wrap_socket(self.client_socket, self.keyfile, self.certfile, True)
        while self.keep_running:
            try:
                (conn, addr) = self.client_socket.accept()
                thread.start_new_thread(self.client_serve_thread, (conn, addr,))
            except:
                traceback.print_exc()

    def terminal_listen_thread(self):
        self.terminal_socket.listen(5)
        if ENCRYPT:
            self.terminal_socket = ssl.wrap_socket(self.terminal_socket, self.keyfile, self.certfile, True)
        while self.keep_running:
            try:
                (conn, addr) = self.terminal_socket.accept()
                thread.start_new_thread(self.terminal_serve_thread, (conn, addr,))
                print "terminal ", addr," connected"
            except:
                traceback.print_exc()

    def statistics_thread(self):
        minute = time.strftime("%Y-%m-%d@%H:%M")
        datestr = time.strftime("%Y-%m-%d")
        statistics={ \
                'terminal_num_max':0, \
                'client_num_max':0, \
                'device_num_max':0, \
                'device_use_max':0, \
                'terminal_num_avg':0, \
                'client_num_avg':0, \
                'device_num_avg':0, \
                'device_use_avg':0 \
                }
        statistics_cnt = 0
        logname='statistics.log'
        try:
            f = open(logname, 'a+')
        except:
            print "error: unable to create/open {0}".format(logname)
            return
        while self.keep_running:
            time.sleep(3)

            #remove outdated log files
            if time.strftime("%Y-%m-%d") != datestr:
                tbefore = time.mktime(time.strptime(time.strftime('%Y-%m-%d'), '%Y-%m-%d'))
                tbefore -= self.log_preserve_period
                flist = os.listdir('server')
                for fname in flist:
                    if os.path.isdir('server/'+ fname) == False:
                        continue
                    if re.match('[0-9]{4}-[0-9]{2}-[0-9]{2}', fname) == None:
                        continue
                    ftime = time.strptime(fname, '%Y-%m-%d')
                    ftime = time.mktime(ftime)
                    if ftime >= tbefore:
                        continue
                    shutil.rmtree('server/' + fname)
                datestr = time.strftime("%Y-%m-%d")

            #disconnect timeout connections
            now = time.time()
            for conn in list(self.timeouts):
                if now <= self.timeouts[conn]['timeout']:
                    continue
                conn.close()
                print self.timeouts[conn]['type'], self.timeouts[conn]['addr'], 'timeout'
                self.timeouts.pop(conn)

            #generate and save statistics data
            client_cnt = 0
            device_cnt = 0
            device_use = 0
            terminal_cnt = len(self.terminal_list)
            for client in self.client_list:
                if client['valid'] == False:
                    continue
                client_cnt += 1
                for port in client['devices']:
                    if client['devices'][port]['valid'] == False:
                        continue
                    device_cnt += 1
                    if client['devices'][port]['using'] > 0:
                        device_use += 1
            if terminal_cnt > statistics['terminal_num_max']:
                statistics['terminal_num_max'] = terminal_cnt
            if client_cnt > statistics['client_num_max']:
                statistics['client_num_max'] = client_cnt
            if device_cnt > statistics['device_num_max']:
                statistics['device_num_max'] = device_cnt
            if device_use > statistics['device_use_max']:
                statistics['device_use_max'] = device_use
            statistics['terminal_num_avg'] += terminal_cnt
            statistics['client_num_avg'] += client_cnt
            statistics['device_num_avg'] += device_cnt
            statistics['device_use_avg'] += device_use
            statistics_cnt += 1.0

            now = time.strftime("%Y-%m-%d@%H:%M")
            if now == minute:
                continue
            statistics['terminal_num_avg'] = round(statistics['terminal_num_avg']/statistics_cnt, 2)
            statistics['client_num_avg'] = round(statistics['client_num_avg']/statistics_cnt, 2)
            statistics['device_num_avg'] = round(statistics['device_num_avg']/statistics_cnt, 2)
            statistics['device_use_avg'] = round(statistics['device_use_avg']/statistics_cnt, 2)
            data = json.dumps({minute:statistics}, sort_keys=True) + '\n'
            f.write(data)
            f.flush()
            minute = now
            statistics={ \
                    'terminal_num_max':0, \
                    'client_num_max':0, \
                    'device_num_max':0, \
                    'device_use_max':0, \
                    'terminal_num_avg':0, \
                    'client_num_avg':0, \
                    'device_num_avg':0, \
                    'device_use_avg':0 \
                    }
            statistics_cnt = 0
            if os.path.isfile(logname) == True:
                continue
            try:
                f.close()
                f = open(logname, 'a+')
            except:
                print "error: unable to create/open {0}".format(logname)
                return

    def init(self, server_port):
        try:
            #initilize CLIENT socket
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.client_socket.bind(('', server_port))
            #initilize TERMINAL socket
            self.terminal_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.terminal_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.terminal_socket.bind(('', server_port + 1))
        except:
            print "address still in use, try later"
            return "fail"
        if os.path.exists('server') == False:
            os.mkdir('server')
        return "success"

    def run(self):
        signal.signal(signal.SIGINT, signal_handler)
        try:
            thread.start_new_thread(self.packet_send_thread, ())
            thread.start_new_thread(self.client_listen_thread, ())
            thread.start_new_thread(self.terminal_listen_thread, ())
            thread.start_new_thread(self.statistics_thread, ())
            while True:
                time.sleep(0.1)
                if self.allocated['devices'] != [] and time.time() > self.allocated['timeout']:
                    with self.allocated['lock']:
                        self.allocated['devices'] = []
        except:
            print "server exiting ..."
            self.keep_running = False

    def deinit(self):
        for c in self.client_list:
            if c['valid'] == False:
                continue
            c['socket'].close()
        for t in self.terminal_list:
            t['socket'].close()
        try:
            self.client_socket.close()
            self.terminal_socket.close()
        except:
            pass

    def server_func(self, server_port):
        if self.init(server_port) == "success":
            self.run()
        self.deinit()
