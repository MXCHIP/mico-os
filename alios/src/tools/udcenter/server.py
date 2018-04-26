import os, sys, time, socket, ssl, signal, re, Queue
import thread, threading, json, traceback, shutil, uuid
import packet as pkt

MAX_MSG_LENGTH = 65536
ENCRYPT = True
DEBUG = True

def create_self_signed_cert(cert_filename, key_filename):
    """
    If datacard.crt and datacard.key don't exist in cert_dir, create a new
    self-signed cert and keypair and write them into that directory.
    """
    try:
        from OpenSSL import crypto, SSL
        from socket import gethostname
        from time import gmtime, mktime
    except:
        if DEBUG: traceback.print_exc()
        return "fail"

    # create a key pair
    k = crypto.PKey()
    k.generate_key(crypto.TYPE_RSA, 2048)

    # create a self-signed cert
    cert = crypto.X509()
    cert.get_subject().C = "CN"
    cert.get_subject().ST = "Zhejiang"
    cert.get_subject().L = "Hangzhou"
    cert.get_subject().O = "Alibaba Cloud"
    cert.get_subject().OU = "IoT"
    cert.get_subject().CN = gethostname()
    cert.set_serial_number(1000)
    cert.gmtime_adj_notBefore(0)
    cert.gmtime_adj_notAfter(10*365*24*60*60)
    cert.set_issuer(cert.get_subject())
    cert.set_pubkey(k)
    cert.sign(k, 'sha1')

    try:
        f = open(cert_filename, "wt")
        f.write(crypto.dump_certificate(crypto.FILETYPE_PEM, cert))
        f.close()
        f = open(key_filename, "wt")
        f.write(crypto.dump_privatekey(crypto.FILETYPE_PEM, k))
        f.close()
    except:
        if DEBUG: traceback.print_exc()
        return 'fail'
    else:
        return 'success'

def signal_handler(sig, frame):
    print "received SIGINT"
    raise KeyboardInterrupt

class Server:
    def __init__(self):
        self.uuid = '{0:012x}'.format(uuid.getnode())
        self.controller_certfile = 'controller_certificate.pem'
        self.output_queue = Queue.Queue(256)
        self.client_socket = None
        self.terminal_socket = None
        self.controller_socket = None
        self.clients = {}
        self.terminals = {}
        self.conn_timeout = {}
        self.device_subscribe_map = {}
        self.keep_running = True
        self.log_preserve_period = (7 * 24 * 3600) * 3 #save log for 3 weeks

    def send_device_list_to_terminal(self, uuid):
        if uuid not in self.terminals or self.terminals[uuid]['valid'] == False:
            return
        devices = []
        if self.terminals[uuid]['devices'] == [uuid + ':all']:
            if uuid in self.clients:
                for port in self.clients[uuid]['devices']:
                    if self.clients[uuid]['devices'][port]['valid'] == False:
                        continue
                    devices.append(uuid + ',' + port + '|' + str(self.clients[uuid]['devices'][port]['using']))
        else:
            for device in self.terminals[uuid]['devices']:
                try:
                    [cuuid, port] = device.split(':')
                except:
                    continue
                if cuuid in self.clients and port in self.clients[cuuid]['devices'] and \
                        self.clients[cuuid]['devices'][port]['valid'] == True:
                    devices.append(cuuid + ',' + port + '|' + str(self.clients[cuuid]['devices'][port]['using']))
                else:
                    devices.append(cuuid + ',' + port + '|' + '-1')
        dev_str = ':'.join(devices)
        self.send_packet(self.terminals[uuid]['socket'], pkt.ALL_DEV, dev_str)

    def client_serve_thread(self, conn, addr):
        file = {}
        msg = ''
        client = None
        self.conn_timeout[conn] = {'type':'client', 'addr': addr, 'timeout': time.time() + 30}
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

                    if client == None:
                        if type != pkt.CLIENT_LOGIN:
                            self.send_packet(conn, pkt.CLIENT_LOGIN, 'request')
                            time.sleep(0.1)
                            continue
                        try:
                            [uuid, tag, token] = value.split(',')
                        except:
                            if DEBUG: traceback.print_exc()
                        if uuid not in self.clients or self.clients[uuid]['token'] != token:
                            print "warning: invalid client {0} connecting @ {1}".format(value, addr)
                            self.send_packet(conn, pkt.CLIENT_LOGIN, 'fail')
                            self.conn_timeout[conn]['timeout'] = time.time() + 1
                            break
                        else:
                            client = self.clients[uuid]
                            client['socket'] = conn
                            client['tag'] = tag
                            client['addr'] = addr
                            client['valid'] = True
                            self.send_packet(conn, pkt.CLIENT_LOGIN, 'success')
                            self.conn_timeout[conn]['timeout'] = time.time() + 30
                            self.report_status_to_controller()
                            print "client {0} connected @ {1}, tag={2}".format(uuid, addr, repr(tag))
                            all_dev_str = uuid + ':all'
                        continue

                    self.conn_timeout[conn]['timeout'] = time.time() + 30
                    if type == pkt.CLIENT_DEV:
                        new_devices = value.split(':')
                        device_list_changed = False
                        for port in new_devices:
                            if port == "":
                                continue
                            if port in client['devices'] and client['devices'][port]['valid'] == True:
                                continue
                            device_list_changed = True
                            if port not in client['devices']:
                                print "new device {0} added to client {1}".format(port, client['uuid'])
                                client['devices'][port] = {
                                        'lock':threading.Lock(),
                                        'valid':True,
                                        'using':0,
                                        'status':'{}'}
                            else:
                                print "device {0} re-added to client {1}".format(port, client['uuid'])
                                client['devices'][port]['status'] = '{}'
                                client['devices'][port]['valid'] = True

                            dev_str = client['uuid'] + ':' + port
                            uuid = None
                            if dev_str in self.device_subscribe_map:
                                uuid = self.device_subscribe_map[dev_str]
                            elif all_dev_str in self.device_subscribe_map:
                                uuid = client['uuid']
                            if uuid != None:
                                self.send_device_list_to_terminal(uuid)

                        for port in list(client['devices']):
                            if port in new_devices:
                                continue
                            if client['devices'][port]['valid'] == False:
                                continue
                            device_list_changed = True
                            client['devices'][port]['status'] = '{}'
                            client['devices'][port]['valid'] = False
                            print "device {0} removed from client {1}".format(port, client['uuid'])
                            dev_str = client['uuid'] + ':' + port
                            uuid = None
                            if dev_str in self.device_subscribe_map:
                                uuid = self.device_subscribe_map[dev_str]
                            elif all_dev_str in self.device_subscribe_map:
                                uuid = client['uuid']
                            if uuid != None:
                                self.send_device_list_to_terminal(uuid)

                        if device_list_changed:
                            self.report_status_to_controller()

                        for port in list(file):
                            if client['devices'][port]['valid'] == True:
                                continue
                            file[port]['handle'].close()
                            file.pop(port)
                    elif type == pkt.DEVICE_LOG:
                        port = value.split(':')[0]
                        if port not in client['devices']:
                            continue
                        #forwad log to subscribed devices
                        dev_str = client['uuid'] + ':' + port
                        uuid = None
                        if dev_str in self.device_subscribe_map:
                            uuid = self.device_subscribe_map[dev_str]
                        elif all_dev_str in self.device_subscribe_map:
                            uuid = client['uuid']
                        if uuid != None and client['tag'] not in value:
                            log = client['uuid'] + ',' + value
                            if self.terminals[uuid]['valid']:
                                self.send_packet(self.terminals[uuid]['socket'], type, log)

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
                        dev_str = client['uuid'] + ':' + port
                        log = client['uuid'] + ',' + value
                        uuid = None
                        if dev_str in self.device_subscribe_map:
                            uuid = self.device_subscribe_map[dev_str]
                        elif all_dev_str in self.device_subscribe_map:
                            uuid = client['uuid']
                        if uuid == None:
                            continue
                        if self.terminals[uuid]['valid'] == False:
                            continue
                        self.send_packet(self.terminals[uuid]['socket'], type, log)
                    elif type in [pkt.DEVICE_DEBUG_DATA, pkt.DEVICE_DEBUG_START, pkt.DEVICE_DEBUG_REINIT, pkt.DEVICE_DEBUG_STOP]:
                        try:
                            [uuid, device] = value.split(':')[0].split(',')
                            result = value.split(':')[1]
                        except:
                            continue
                        if uuid in self.terminals and self.terminals[uuid]['valid']:
                            value = client['uuid'] + value[len(uuid):]
                            self.send_packet(self.terminals[uuid]['socket'], type, value)
                            if device not in client['devices']:
                                continue
                            if client['devices'][device]['valid'] == False or type == pkt.DEVICE_DEBUG_STOP:
                                if 'debug_session' in client['devices'][device]:
                                    client['devices'][device].pop('debug_session')
                                continue
                            if type in [pkt.DEVICE_DEBUG_START, pkt.DEVICE_DEBUG_REINIT] and result == 'success':
                                client['devices'][device]['debug_session'] = uuid
                                continue
                        else:
                            if device not in client['devices']:
                                continue
                            if 'debug_session' not in client['devices'][device]:
                                continue
                            if uuid != client['devices'][device]['debug_session']:
                                continue
                            client['devices'][device].pop('debug_session')
                            content = uuid + ',' + device
                            self.send_packet(conn, pkt.DEVICE_DEBUG_STOP, content)
                    elif type == pkt.DEVICE_ERASE or type == pkt.DEVICE_PROGRAM or \
                         type == pkt.DEVICE_START or type == pkt.DEVICE_STOP or \
                         type == pkt.DEVICE_RESET or type == pkt.DEVICE_CMD or \
                         type == pkt.FILE_BEGIN or type == pkt.FILE_DATA or \
                         type == pkt.FILE_END:
                        values = value.split(',')
                        uuid = values[0]
                        if uuid not in self.terminals or self.terminals[uuid]['valid'] == False:
                            continue
                        sock = self.terminals[uuid]['socket']
                        if values[1] != 'success' and values[1] != 'ok':
                            type = pkt.CMD_ERROR
                            content = ','.join(values[1:])
                        else:
                            type = pkt.CMD_DONE
                            content = ','.join(values[1:])
                        self.send_packet(sock, type, content)
            except:
                if DEBUG: traceback.print_exc()
                break
        conn.close()
        if conn in self.conn_timeout: self.conn_timeout.pop(conn)
        if client:
            for port in client['devices']:
                if client['devices'][port]['valid'] == False:
                    continue
                client['devices'][port]['status'] = '{}'
                client['devices'][port]['valid'] = False
                print "device {0} removed from client {1}".format(port, client['uuid'])
                dev_str = client['uuid'] + ':' + port
                if dev_str not in self.device_subscribe_map:
                    continue
                uuid = self.device_subscribe_map[dev_str]
                self.send_device_list_to_terminal(uuid)
            if all_dev_str in self.device_subscribe_map:
                self.send_device_list_to_terminal(client['uuid'])
            client['valid'] = False
            print "client {0} @ {1} disconnected".format(client['uuid'], addr)
            self.report_status_to_controller()
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

        content = file.read(4096)
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

    def increase_device_refer(self, client, port, using_list):
        if [client['uuid'], port] in using_list:
            return
        if port not in list(client['devices']):
            return
        with client['devices'][port]['lock']:
            client['devices'][port]['using'] += 1
        using_list.append([client['uuid'], port])
        dev_str = client['uuid'] + ':' + port
        all_dev_str = client['uuid'] + ':all'
        if dev_str in self.device_subscribe_map:
            uuid = self.device_subscribe_map[dev_str]
            self.send_device_list_to_terminal(uuid)
        if all_dev_str in self.device_subscribe_map:
            self.send_device_list_to_terminal(client['uuid'])
        return

    def terminal_serve_thread(self, conn, addr):
        using_list = []
        msg = ''
        terminal = None
        self.conn_timeout[conn] = {'type': 'terminal', 'addr': addr, 'timeout': time.time() + 30}
        while self.keep_running:
            try:
                new_msg = conn.recv(MAX_MSG_LENGTH);
                if new_msg == '':
                    break

                msg += new_msg
                while msg != '':
                    type, length, value, msg = pkt.parse(msg)
                    if type == pkt.TYPE_NONE:
                        break

                    if terminal == None:
                        if type != pkt.TERMINAL_LOGIN:
                            self.send_packet(conn, pkt.CLIENT_LOGIN, 'request')
                            time.sleep(0.1)
                            continue
                        try:
                            [uuid, token] = value.split(',')
                        except:
                            if DEBUG: traceback.print_exc()
                        if uuid not in self.terminals or self.terminals[uuid]['token'] != token:
                            print "warning: invalid terminal {0} connecting @ {1}".format(value, addr)
                            self.send_packet(conn, pkt.TERMINAL_LOGIN, 'fail')
                            self.conn_timeout[conn]['timeout'] = time.time() + 1
                            break
                        else:
                            terminal = self.terminals[uuid]
                            terminal['socket'] = conn
                            terminal['addr'] = addr
                            terminal['valid'] = True
                            self.send_packet(conn, pkt.TERMINAL_LOGIN, 'success')
                            self.conn_timeout[conn]['timeout'] = time.time() + 30
                            print "terminal {0}@{1} logedin".format(uuid, addr)
                            self.send_device_list_to_terminal(terminal['uuid'])
                            if terminal['devices'] == [terminal['uuid'] + ':all']:
                                uuid = terminal['uuid']
                                if uuid in self.clients:
                                    for port in self.clients[uuid]['devices']:
                                        if self.clients[uuid]['devices'][port]['valid'] == False:
                                            continue
                                        data = uuid + ',' + port + ':' + self.clients[uuid]['devices'][port]['status']
                                        self.send_packet(conn, pkt.DEVICE_STATUS, data)
                            else:
                                for device in terminal['devices']:
                                    try:
                                        [uuid, port] = device.split(':')
                                    except:
                                        continue
                                    if uuid not in self.clients:
                                        continue
                                    if port not in self.clients[uuid]['devices']:
                                        continue
                                    if self.clients[uuid]['devices'][port]['valid'] == False:
                                        continue
                                    data = uuid + ',' + port + ':' + self.clients[uuid]['devices'][port]['status']
                                    self.send_packet(conn, pkt.DEVICE_STATUS, data)
                            self.report_status_to_controller()
                        continue

                    self.conn_timeout[conn]['timeout'] = time.time() + 30
                    if type == pkt.FILE_BEGIN or type == pkt.FILE_DATA or type == pkt.FILE_END:
                        dev_str = value.split(':')[0]
                        uuid = dev_str.split(',')[0]
                        if uuid not in self.clients or self.clients[uuid]['valid'] == False:
                            self.send_packet(conn, pkt.CMD_ERROR, 'nonexist')
                            continue
                        client = self.clients[uuid]
                        content = terminal['uuid'] + value[len(dev_str):]
                        self.send_packet(client['socket'], type, content)
                    elif type == pkt.DEVICE_DEBUG_DATA or \
                         type == pkt.DEVICE_ERASE or type == pkt.DEVICE_PROGRAM or \
                         type == pkt.DEVICE_START or type == pkt.DEVICE_STOP or \
                         type == pkt.DEVICE_RESET or type == pkt.DEVICE_CMD or \
                         type == pkt.DEVICE_DEBUG_START or type == pkt.DEVICE_DEBUG_STOP or \
                         type == pkt.DEVICE_DEBUG_REINIT:
                        dev_str_split = value.split(':')[0].split(',')[0:2]
                        if len(dev_str_split) != 2:
                            self.send_packet(conn, pkt.CMD_ERROR,'argerror')
                            continue
                        [uuid, port] = dev_str_split
                        if uuid not in self.clients or self.clients[uuid]['valid'] == False:
                            self.send_packet(conn, pkt.CMD_ERROR, 'nonexist')
                            continue
                        client = self.clients[uuid]
                        content = terminal['uuid'] + value[len(uuid):]
                        self.send_packet(client['socket'], type, content)
                        self.increase_device_refer(client, port, using_list)
                    elif type == pkt.LOG_DOWNLOAD:
                        dev_str_split = value.split(',')
                        if len(dev_str_split) != 2:
                            continue
                        [uuid, port] = dev_str_split
                        datestr = time.strftime('%Y-%m-%d')
                        filename = 'server/' + datestr + '/' + uuid + '-' + port.split('/')[-1] + '.log'
                        if uuid not in self.clients or self.clients[uuid]['valid'] == False or \
                           port not in self.clients[uuid]['devices'] or os.path.exists(filename) == False:
                            self.send_packet(conn, pkt.CMD_ERROR,'fail')
                            print "terminal {0}:{1}".format(terminal['addr'][0], terminal['addr'][1]),
                            print "downloading log of device {0}:{1} ... failed".format(uuid, port)
                            continue
                        ret = self.send_file_to_someone(terminal, filename)
                        if ret == True:
                            self.send_packet(conn, pkt.CMD_DONE, 'success')
                            result = 'succeed'
                        else:
                            self.send_packet(conn, pkt.CMD_ERROR, 'fail')
                            result = 'failed'
                        print "terminal {0}".format(terminal['uuid']),
                        print "downloading log of device {0}:{1} ... {2}".format(uuid, port, result)
            except:
                if DEBUG: traceback.print_exc()
                break
        for device in using_list:
            uuid = device[0]
            port = device[1]
            client = None
            for c in self.clients:
                c = self.clients[c]
                if c['uuid'] != uuid:
                    continue
                client = c
                break
            if client != None and port in list(client['devices']):
                with client['devices'][port]['lock']:
                    if client['devices'][port]['using'] > 0:
                        client['devices'][port]['using'] -= 1
        conn.close()
        if conn in self.conn_timeout: self.conn_timeout.pop(conn)
        if terminal:
            terminal['valid'] = False
            self.report_status_to_controller()
            for c in self.clients:
                c = self.clients[c]
                for device in c['devices']:
                    if 'debug_session' not in c['devices'][device]:
                        continue
                    if c['devices'][device]['debug_session'] != terminal['uuid']:
                        continue
                    c['devices'][device].pop('debug_session')
                    content = terminal['uuid'] + ',' + device
                    self.send_packet(c['socket'], pkt.DEVICE_DEBUG_STOP, content)
            print "terminal {0}@{1} disconnected".format(terminal['uuid'], addr)
        else:
            print "terminal {0} disconnected".format(addr)

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

    def report_status_to_controller(self):
        if self.controller_socket == None:
            return
        sock = self.controller_socket
        clients = []; devices = []; terminals = []
        for uuid in self.clients:
            if self.clients[uuid]['valid'] == False:
                continue
            clients += [uuid]
            device_list = self.clients[uuid]['devices']
            for port in device_list:
                if device_list[port]['valid'] == False:
                    continue
                dev_str = uuid + ':' + port
                devices += [dev_str]
        for uuid in self.terminals:
            if self.terminals[uuid]['valid'] == False:
                continue
            terminals += [uuid]
        status = {'clients':clients, 'devices':devices, 'terminals': terminals}
        self.send_packet(sock, pkt.ACCESS_REPORT_STATUS, json.dumps(status))

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

    def controller_interact_thread(self, controller_ip, controller_port):
        sock = None; logedin = False; heartbeat_timeout = False
        while self.keep_running:
            if sock == None: #connect to controller
                logedin = False; heartbeat_timeout = None
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                if ENCRYPT:
                    sock = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED, ca_certs=self.controller_certfile)
                try:
                    sock.connect((controller_ip, controller_port))
                    msg = '';
                    sock.settimeout(1)
                    print("connection to controller established")
                except:
                    sock = None; self.controller_socket = sock
                    time.sleep(2)
                    continue

            while sock != None:
                if logedin == False: #try to login
                    content = {}
                    content['uuid'] = self.uuid
                    content['client_port'] = self.client_socket.getsockname()[1]
                    content['terminal_port'] = self.terminal_socket.getsockname()[1]
                    if ENCRYPT:
                        content['certificate'] = open(self.certfile, 'rt').read()
                    else:
                        content['certificate'] = 'None'
                    content = 'server,' + json.dumps(content)
                    self.send_packet(sock, pkt.ACCESS_LOGIN, content)

                if heartbeat_timeout and time.time() > heartbeat_timeout:
                    self.send_packet(sock, pkt.HEARTBEAT, '')
                    heartbeat_timeout += 10

                try:
                    data = sock.recv(MAX_MSG_LENGTH)
                except socket.timeout:
                    continue
                except ssl.SSLError as e:
                    if e.message == 'The read operation timed out':
                        continue
                    else:
                        sock = None; self.controller_socket = sock
                        break
                except:
                    if DEBUG: traceback.print_exc()
                    sock = None; self.controller_socket = sock
                    break
                if not data:
                    print("error: connection to controller lost")
                    sock = None; self.controller_socket = sock
                    break

                msg += data
                while msg != '':
                    type, length, value, msg = pkt.parse(msg)
                    #print time.time(), 'controller', type, value
                    if type == pkt.TYPE_NONE:
                        break

                    if type == pkt.ACCESS_LOGIN:
                        if value == 'ok':
                            logedin = True
                            heartbeat_timeout = time.time() + 10
                            self.controller_socket = sock
                            self.report_status_to_controller()
                            print("login to controller succeed")
                        else:
                            print("login to controller failed, ret={0}, retry later...".format(value))
                            time.sleep(5)
                        continue
                    if type == pkt.ACCESS_REPORT_STATUS:
                        continue
                    if type == pkt.ACCESS_ADD_CLIENT:
                        try:
                            [uuid, token] = value.split(',')
                        except:
                            continue
                        if uuid not in self.clients:
                            client = {'uuid': uuid,
                                      'token': token,
                                      'valid':False,
                                      'devices':{}}
                            self.clients[uuid] = client
                        else:
                            client = self.clients[uuid]
                            if 'sock' in client:
                                client['sock'].close()
                            client['valid'] = False
                            client['token'] = token
                        self.send_packet(sock, pkt.ACCESS_ADD_CLIENT, 'success,' + value)
                        continue
                    if type == pkt.ACCESS_DEL_CLIENT:
                        uuid = value
                        if uuid not in self.clients:
                            self.send_packet(pkt.ACCESS_DEL_CLIENT, 'fail')
                            continue
                        if 'socket' in self.clients[uuid]:
                            self.clients[uuid]['socket'].close()
                        self.clients[uuid]['token'] = None
                        self.send_packet(sock, pkt.ACCESS_DEL_CLIENT, 'success')
                        continue
                    if type == pkt.ACCESS_ADD_TERMINAL:
                        try:
                            [uuid, token, devices] = value.split(',')
                            devices = devices.split('|')
                        except:
                            traceback.print_exc()
                            continue
                        if uuid not in self.terminals:
                            terminal = {'uuid': uuid,
                                        'token': token,
                                        'valid':False,
                                        'devices':devices}
                            self.terminals[uuid] = terminal
                        else:
                            terminal = self.terminals[uuid]
                            if 'socket' in terminal:
                                terminal['socket'].close()
                            for device in terminal['devices']:
                                if device not in self.device_subscribe_map:
                                    continue
                                self.device_subscribe_map.pop(device)
                            terminal['valid'] = False
                            terminal['token'] = token
                            terminal['devices'] = devices
                        for device in devices:
                            self.device_subscribe_map[device] = uuid
                        content = ','.join(['success', uuid, token])
                        self.send_packet(sock, pkt.ACCESS_ADD_TERMINAL, content)
                        continue
                    if type == pkt.ACCESS_UPDATE_TERMINAL:
                        try:
                            [uuid, devices] = value.split(',')
                            devices = devices.split('|')
                        except:
                            print "invalid arguments {0}".format(repr(value))
                            continue
                        if uuid not in self.terminals:
                            self.send_packet(sock, pkt.ACCESS_UPDATE_TERMINAL, 'fail')
                        else:
                            terminal = self.terminals[uuid]
                            added_devices = []
                            for device in devices:
                                if device in terminal['devices']:
                                    continue
                                self.device_subscribe_map[device] = uuid
                                terminal['devices'] += [device]
                                added_devices += [device]
                            for device in list(terminal['devices']):
                                if device in devices:
                                    continue
                                terminal['devices'].remove(device)
                                self.device_subscribe_map.pop(device)
                            self.send_device_list_to_terminal(uuid)
                            for device in added_devices:
                                try:
                                    [uuid, port] = device.split(':')
                                except:
                                    continue
                                if uuid not in self.clients:
                                    continue
                                if port not in self.clients[uuid]['devices']:
                                    continue
                                if self.clients[uuid]['devices'][port]['valid'] == False:
                                    continue
                                data = uuid + ',' + port + ':' + self.clients[uuid]['devices'][port]['status']
                                self.send_packet(terminal['socket'], pkt.DEVICE_STATUS, data)
                            self.send_packet(sock, pkt.ACCESS_UPDATE_TERMINAL, 'success')
                        continue
                    if type == pkt.ACCESS_DEL_TERMINAL:
                        uuid = value
                        if uuid not in self.terminals:
                            self.send_packet(pkt.ACCESS_DEL_TERMINAL, 'fail')
                            continue
                        if 'socket' in self.terminals[uuid]:
                            self.terminals[uuid]['socket'].close()
                        self.terminals[uuid]['token'] = None
                        self.send_packet(sock, pkt.ACCESS_DEL_TERMINAL, 'success')
                        continue

    def house_keeping_thread(self):
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
            for conn in list(self.conn_timeout):
                if now <= self.conn_timeout[conn]['timeout']:
                    continue
                conn.close()
                print self.conn_timeout[conn]['type'], self.conn_timeout[conn]['addr'], 'timeout'
                self.conn_timeout.pop(conn)

            #generate and save statistics data
            client_cnt = 0; terminal_cnt = 0
            device_cnt = 0; device_use = 0

            for uuid in self.clients:
                client = self.clients[uuid]
                if client['valid'] == False:
                    continue
                client_cnt += 1
                devices = client['devices']
                for port in devices:
                    if devices[port]['valid'] == False:
                        continue
                    device_cnt += 1
                    if devices[port]['using'] <= 0:
                        continue
                    device_use += 1
            for uuid in self.terminals:
                if self.terminals[uuid]['valid'] == False:
                    continue
                terminal_cnt += 1

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

    def init(self):
        if os.path.exists('server') == False:
            try:
                os.mkdir('server')
            except:
                print 'error: create server foler failed'
                return 'fail'
        self.keyfile = 'server/key.pem'
        self.certfile = 'server/certificate.pem'
        ret = create_self_signed_cert(self.certfile, self.keyfile)
        if ret != 'success':
            print 'error: create self signed certificate failed'
            return 'fail'
        try:
            #initilize CLIENT socket
            client_port = 2048 + ord(os.urandom(1))
            self.client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.client_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.client_socket.bind(('', client_port))
            #initilize TERMINAL socket
            terminal_port = 2048 + ord(os.urandom(1))
            while terminal_port == client_port:
                terminal_port = 2048 + ord(os.urandom(1))
            self.terminal_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.terminal_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            self.terminal_socket.bind(('', terminal_port))
        except:
            print "address still in use, try later"
            return "fail"
        return "success"

    def run(self, host_name, host_port):
        signal.signal(signal.SIGINT, signal_handler)
        try:
            thread.start_new_thread(self.packet_send_thread, ())
            thread.start_new_thread(self.client_listen_thread, ())
            thread.start_new_thread(self.terminal_listen_thread, ())
            thread.start_new_thread(self.controller_interact_thread, (host_name, host_port,))
            thread.start_new_thread(self.house_keeping_thread, ())
            while True:
                time.sleep(0.1)
        except:
            print "server exiting ..."
            self.keep_running = False

    def deinit(self):
        sockets = []
        for uuid in self.clients:
            if self.clients[uuid]['valid'] == False:
                continue
            if 'socket' not in self.clients[uuid]:
                continue
            sockets.append(self.clients[uuid]['socket'])
        for uuid in self.terminals:
            if self.terminals[uuid]['valid'] == False:
                continue
            if 'socket' not in self.terminals[uuid]:
                continue
            sockets.append(self.terminals[uuid]['socket'])
        for sock in [self.client_socket, self.terminal_socket, self.controller_socket]:
            if not sock:
                continue
            sockets.append(sock)
        for sock in sockets:
            try:
                sock.close()
            except:
                pass

    def server_func(self, host_name, host_port):
        if self.init() == "success":
            self.run(host_name, host_port)
        self.deinit()
