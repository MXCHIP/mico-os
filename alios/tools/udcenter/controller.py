import os, sys, re, time, socket, ssl, signal, tty, termios
import select, thread, Queue, json, traceback
import sqlite3 as sql
import packet as pkt
from os import path

CONFIG_TIMEOUT = 30
CONFIG_MAXMSG_LENTH = 8192
ENCRYPT = True
DEBUG = True

def signal_handler(sig, frame):
    print "received SIGINT"
    raise KeyboardInterrupt

class selector():
    EVENT_READ = 0
    EVENT_WRITE = 1
    EVENT_ERROR = 2
    def __init__(self):
        self.read_map = {}
        self.write_map = {}
        self.error_map = {}

    def register(self, fd, type, callback):
        types = {self.EVENT_READ:self.read_map, self.EVENT_WRITE:self.write_map, self.EVENT_ERROR: self.error_map}
        if type not in types:
            return
        map = types[type]
        map[fd] = callback

    def unregister(self, fd, type):
        types = {self.EVENT_READ:self.read_map, self.EVENT_WRITE:self.write_map, self.EVENT_ERROR: self.error_map}
        if type not in types:
            return
        map = types[type]
        if fd in map: map.pop(fd)

    def select(self):
        ret = []
        r, w, e = select.select(list(self.read_map), list(self.write_map), list(self.error_map), 5)
        for fd in r:
            ret.append([fd, self.read_map[fd]])
        for fd in w:
            ret.append([fd, self.write_map[fd]])
        for fd in e:
            ret.append([fd, self.error_map[fd]])
        return ret

class Controller():
    def __init__(self, host, port):
        self.host = host
        self.port = port
        self.connections = {}
        self.timeouts = {}
        self.selector = None
        self.user_cmd = ''
        self.cur_pos = 0
        self.serve_funcs = {
                'none':    self.login_message_process, \
                'client':  self.client_message_process, \
                'server':  self.server_message_process, \
                'terminal':self.terminal_message_process \
                }

        work_dir = path.join(path.expanduser('~'), '.udcontroller')
        if path.exists(work_dir) == False:
            try:
                os.mkdir(work_dir)
            except:
                print "error: create directory {0} failed".format(work_dir)

        self.keyfile = path.join(path.dirname(path.abspath(__file__)), 'controller_key.pem')
        self.certfile = path.join(path.dirname(path.abspath(__file__)), 'controller_certificate.pem')
        dbfile = path.join(path.expanduser('~'), '.udcontroller', 'controller.db')
        self.dbase = sql.connect(dbfile, check_same_thread = False)
        #sqlcmd = 'CREATE TABLE IF NOT EXISTS Users(uuid TEXT, name TEXT, email TEXT, devices TEXT)' #v0.1
        sqlcmd = 'CREATE TABLE IF NOT EXISTS Users(uuid TEXT, name TEXT, info TEXT)' #v0.2
        self.database_excute_sqlcmd(sqlcmd)

        sqlcmd = 'CREATE TABLE IF NOT EXISTS Clients(uuid TEXT, name TEXT, info TEXT)'
        self.database_excute_sqlcmd(sqlcmd)

        sqlcmd = 'CREATE TABLE IF NOT EXISTS Developers(uuid TEXT, name TEXT, info TEXT)'
        self.database_excute_sqlcmd(sqlcmd)

        sqlcmd = 'CREATE TABLE IF NOT EXISTS Devices(device TEXT, uuid TEXT, timeout REAL)'
        self.database_excute_sqlcmd(sqlcmd)

        sqlcmd = 'SELECT * FROM Devices'
        rows = self.database_excute_sqlcmd(sqlcmd)
        self.devices = {}
        if rows == None:
            print "database error: polling Devices table failed"
        else:
            for row in rows:
                try:
                    self.devices[row[0]] = {'uuid': row[1], 'timeout': row[2]}
                except:
                    print "database error detected while initializing"

        self.cmd_history = []
        cmd_history_path = path.join(path.expanduser('~'), '.udcontroller', 'cmd_history')
        if path.exists(cmd_history_path) == True:
            try:
                file = open(cmd_history_path, 'rb')
                self.cmd_history = json.load(file)
                file.close()
            except:
                print "read command history failed"

        log_path = path.join(path.expanduser('~'), '.udcontroller', 'runlog.txt')
        try:
            self.flog = open(log_path, 'a+')
        except:
            self.flog = None
            print "create or open log file {0} failed".format(log_path)

    def database_excute_sqlcmd(self, sqlcmd):
        with self.dbase:
            cur = self.dbase.cursor()
            try:
                cur.execute(sqlcmd)
            except:
                #traceback.print_exc()
                ret = None
            else:
                ret = cur.fetchall()
        return ret

    def debug_log(self, str):
        if DEBUG and self.flog:
            try:
                self.flog.write(time.strftime("%Y-%m-%d %H:%M:%S : ") + str + '\n')
            except:
                pass

    def usercmd_print(self):
        cmd_str = '\r# ' + ' ' * (len(self.user_cmd) + 1)
        cmd_str += '\r# ' + self.user_cmd
        cmd_str += '\b' * (len(self.user_cmd) - self.cur_pos)
        sys.stdout.write(cmd_str)
        sys.stdout.flush()

    def netlog_print(self, log):
        if len(self.user_cmd) >= len(log):
            sys.stdout.write('\r' + ' ' * len(self.user_cmd))
        sys.stdout.write('\r' + log + '\r\n')
        self.usercmd_print()
        if DEBUG and self.flog:
            try:
                self.flog.write(time.strftime("%Y-%m-%d %H:%M:%S : ") + str + '\n')
            except:
                pass

    def clilog_print(self, log):
        sys.stdout.write(log + '\r\n')

    def net_conn_accept(self, sock):
        try:
            conn, addr = sock.accept()
        except:
            self.debug_log(traceback.format_exc())
            return
        self.debug_log('{0} connected'.format(addr))
        conn.setblocking(False)
        self.selector.register(conn, selector.EVENT_READ, self.net_receive_data)
        self.connections[conn] = {'role':'none', 'addr': addr, 'inbuff':'', 'outbuff':Queue.Queue(256)}
        self.timeouts[conn] = time.time() + 0.2

    def net_send_data(self, sock):
        if sock not in self.connections:
            return
        queue = self.connections[sock]['outbuff']
        try:
            data = queue.get(False)
        except:
            pass
        else:
            #self.netlog_print(str(time.time()' + ' ' + data)
            sock.send(data)
        if queue.empty():
            self.selector.unregister(sock, selector.EVENT_WRITE)

    def net_receive_data(self, sock):
        try:
            data = sock.recv(CONFIG_MAXMSG_LENTH)
        except:
            data = None
        role = self.connections[sock]['role']
        if not data:
            addr = self.connections[sock]['addr']
            self.debug_log("{0} {1} disconnect".format(role, addr))
            self.selector.unregister(sock, selector.EVENT_READ)
            self.selector.unregister(sock, selector.EVENT_WRITE)
            sock.close()
            self.timeouts.pop(sock)
            self.connections.pop(sock)
            return
        msg = self.connections[sock]['inbuff']
        msg += data
        while msg != '':
            type, length, value, msg = pkt.parse(msg)
            if type == pkt.TYPE_NONE:
                break
            self.debug_log("{0}: enter {1} function".format(time.time(), self.serve_funcs[role].__name__))
            self.serve_funcs[role](sock, type, value)
            self.debug_log("{0}: exit {1} function".format(time.time(), self.serve_funcs[role].__name__))

    def schedule_data_send(self, sock, content):
        if sock not in self.connections:
            try:
                self.debug_log("error: sending data to invalid connection {0}".format(sock.getpeername()))
            except:
                self.debug_log("error: sending data to invalid connection {0}".format(repr(sock)))
            return False
        queue = self.connections[sock]['outbuff']
        if queue.full():
            addr = self.connections[sock]['addr']
            self.netlog_print("warning: output buffer for {0} full, discard packet".format(addr))
            return False
        queue.put_nowait(content)
        self.selector.register(sock, selector.EVENT_WRITE, self.net_send_data)

    def choose_random_server(self):
        server_list = []
        for conn in self.connections:
            if self.connections[conn]['role'] != 'server':
                continue
            if 'valid' not in self.connections[conn]:
                continue
            if self.connections[conn]['valid'] == False:
                continue
            server_list.append(conn)
        if server_list == []:
            return None

        bytes = os.urandom(2)
        rand_num = ord(bytes[0]) * ord(bytes[1])
        rand_num = rand_num % len(server_list)
        return server_list[rand_num]

    def generate_random_hexstr(self, len):
        bytes = os.urandom(int(round(len/2.0))); hexstr = ''
        for byte in bytes: hexstr += '{0:02x}'.format(ord(byte))
        return hexstr[:len]

    def find_server_for_target(self, target, type):
        type = type + 's'
        for conn in self.connections:
            if self.connections[conn]['role'] != 'server':
                continue
            if self.connections[conn]['valid'] == False:
                continue
            if type not in self.connections[conn]['status']:
                continue
            if target not in self.connections[conn]['status'][type]:
                continue
            return conn
        return None

    def login_message_process(self, sock, type, value):
        if type != pkt.ACCESS_LOGIN:
            content = pkt.construct(pkt.ACCESS_LOGIN, 'request')
            self.schedule_data_send(sock, content)
            return

        try:
            role = value.split(',')[0]
        except:
            role = None

        if role == None or role not in ['client', 'server', 'terminal']:
            content = pkt.construct(pkt.ACCESS_LOGIN, 'argerror')
            self.schedule_data_send(sock, content)
            return

        self.connections[sock]['role'] = role
        self.serve_funcs[role](sock, type, value)

    def client_message_process(self, sock, type, value):
        #self.netlog_print('client {0} {1}'.format(type, value))
        if type == pkt.ACCESS_LOGIN:
            if value.startswith('client,'):
                uuid = value[len('client,'):]
                is_valid_uuid = re.match('^[0-9a-f]{16}$', uuid)
                server_sock = self.choose_random_server()
            else:
                is_valid_uuid = None
            if is_valid_uuid == None:
                content = pkt.construct(pkt.ACCESS_LOGIN, 'invalid access key')
                self.schedule_data_send(sock, content)
                self.netlog_print('denied invalid client {0} login'.format(uuid))
                return

            sqlcmd = "SELECT * FROM Clients WHERE uuid = '{0}'".format(uuid)
            client = self.database_excute_sqlcmd(sqlcmd)
            sqlcmd = "SELECT * FROM Developers WHERE uuid = '{0}'".format(uuid)
            developer = self.database_excute_sqlcmd(sqlcmd)
            if (client == None or len(client) != 1) and (developer == None or len(developer) != 1):
                content = pkt.construct(pkt.ACCESS_LOGIN, 'invalid access key')
                self.schedule_data_send(sock, content)
                self.netlog_print("denied invalid client '{0}' login".format(uuid))
                return

            if server_sock == None:
                content = pkt.construct(pkt.ACCESS_LOGIN, 'noserver')
                self.schedule_data_send(sock, content)
                self.netlog_print('denied client {0} login: no server available'.format(uuid))
                return

            token = self.generate_random_hexstr(16)
            content = '{0},{1}'.format(uuid, token)
            content = pkt.construct(pkt.ACCESS_ADD_CLIENT, content)
            self.schedule_data_send(server_sock, content)
            self.connections[sock]['uuid'] = uuid
            self.timeouts[sock] = time.time() + CONFIG_TIMEOUT
            self.netlog_print('client {0} login ... informing server'.format(uuid))
            return

    def server_message_process(self, sock, type, value):
        self.timeouts[sock] = time.time() + CONFIG_TIMEOUT
        if type in [pkt.HEARTBEAT, pkt.ACCESS_UPDATE_TERMINAL, pkt.ACCESS_DEL_TERMINAL, pkt.ACCESS_DEL_CLIENT]:
            return
        #self.netlog_print('server {0} {1}'.format(type, value))
        if type == pkt.ACCESS_LOGIN:
            if value.startswith('server,'):
                value = value[len('server,'):]
                try:
                    server_info = json.loads(value)
                except:
                    server_info = {}
            else:
                server_info = {}

            fields = ['uuid', 'client_port', 'terminal_port', 'certificate']
            is_valid_server = True
            for info in fields:
                if info in server_info:
                    continue
                is_valid_server = False
                break
            if is_valid_server and ENCRYPT and server_info['certificate'] == 'None':
                is_valid_server = False

            if is_valid_server == False:
                content = pkt.construct(pkt.ACCESS_LOGIN, 'fail')
                self.connections[sock]['valid'] = False
                self.netlog_print('denied server {0} login, info: {1}'.format(self.connections[sock]['addr']), server_info)
            else:
                content = pkt.construct(pkt.ACCESS_LOGIN, 'ok')
                self.connections[sock]['uuid'] = server_info['uuid']
                self.connections[sock]['client_port'] = server_info['client_port']
                self.connections[sock]['terminal_port'] = server_info['terminal_port']
                if ENCRYPT:
                    self.connections[sock]['certificate'] = server_info['certificate']
                else:
                    self.connections[sock]['certificate'] = 'None'
                self.connections[sock]['valid'] = True
                self.netlog_print('accepted server login, info: {0}'.format(self.connections[sock]))
            self.schedule_data_send(sock, content)
            return
        if type == pkt.ACCESS_REPORT_STATUS:
            try:
                status = json.loads(value)
            except:
                if DEBUG: traceback.print_exc()
                status = {}
            if not status:
                content = pkt.construct(pkt.ACCESS_REPORT_STATUS, 'fail')
            else:
                self.connections[sock]['status'] = status
                content = pkt.construct(pkt.ACCESS_REPORT_STATUS, 'ok')
            self.schedule_data_send(sock, content)
            return
        if type == pkt.ACCESS_ADD_CLIENT:
            try:
                [ret, uuid, token] = value.split(',')
            except:
                self.netlog_print('error: invalid return value {0}'.format(repr(value)))
                return
            if ret != 'success':
                self.netlog_print('client {0} login failed: server rejected'.format(uuid))
                return #TODO: choose another server for the client

            client_sock = None
            for conn in self.connections:
                if self.connections[conn]['role'] != 'client':
                    continue
                if 'uuid' not in self.connections[conn]:
                    continue
                if self.connections[conn]['uuid'] != uuid:
                    continue
                client_sock = conn
                break
            if client_sock == None:
                return

            server_addr = self.connections[sock]['addr'][0]
            server_port = self.connections[sock]['client_port']
            certificate = self.connections[sock]['certificate']
            content = 'success,{0},{1},{2},{3}'.format(server_addr, server_port, token, certificate)
            content = pkt.construct(pkt.ACCESS_LOGIN, content)
            self.schedule_data_send(client_sock, content)
            now = time.strftime('%Y-%m-%d %H:%M:%S : ')
            self.netlog_print('client {0} login succeed'.format(uuid))
            return
        if type == pkt.ACCESS_ADD_TERMINAL:
            try:
                [ret, uuid, token] = value.split(',')
            except:
                self.netlog_print('error: invalid return value {0}'.format(repr(value)))
                return
            if ret != 'success':
                self.netlog_print('terminal {0} login failed: server rejected'.format(uuid))

            terminal_sock = None
            for conn in self.connections:
                if self.connections[conn]['role'] != 'terminal':
                    continue
                if 'uuid' not in self.connections[conn]:
                    continue
                if self.connections[conn]['uuid'] != uuid:
                    continue
                terminal_sock = conn
                break
            if terminal_sock == None:
                return

            if ret != 'success':
                content = pkt.construct(pkt.ACCESS_LOGIN, 'fail')
                self.schedule_data_send(terminal_sock, content)
                return

            server_addr = self.connections[sock]['addr'][0]
            server_port = self.connections[sock]['terminal_port']
            certificate = self.connections[sock]['certificate']
            content = 'success,{0},{1},{2},{3}'.format(server_addr, server_port, token, certificate)
            content = pkt.construct(pkt.ACCESS_LOGIN, content)
            self.schedule_data_send(terminal_sock, content)
            self.netlog_print('terminal {0} login succeed'.format(uuid))
            return

    def terminal_message_process(self, sock, type, value):
        #self.netlog_print('terminal {0} {1}'.format(type, value))
        if type == pkt.ACCESS_LOGIN:
            if value.startswith('terminal,'):
                uuid = value[len('terminal,'):]
                is_valid_uuid = re.match('^[0-9a-f]{16}$', uuid)
            else:
                is_valid_uuid = None
            if is_valid_uuid == None:
                content = pkt.construct(pkt.ACCESS_LOGIN, 'invalid access key')
                self.schedule_data_send(sock, content)
                self.netlog_print("denied invalid terminal {0} login".format(uuid))
                return

            sqlcmd = "SELECT * FROM Users WHERE uuid = '{0}'".format(uuid)
            user = self.database_excute_sqlcmd(sqlcmd)
            sqlcmd = "SELECT * FROM Developers WHERE uuid = '{0}'".format(uuid)
            developer = self.database_excute_sqlcmd(sqlcmd)
            if (user == None or len(user) != 1) and (developer == None or len(developer) != 1):
                content = pkt.construct(pkt.ACCESS_LOGIN, 'invalid access key')
                self.schedule_data_send(sock, content)
                self.netlog_print("denied invalid terminal {0} login".format(uuid))
                return

            if user:
                devices = []; client_uuids = []; server_sock = None
                for device in self.devices:
                    if self.devices[device]['uuid'] != uuid:
                        continue
                    devices.append(device)
                    client_uuid = device[0:16]
                    if client_uuid in client_uuids:
                        continue
                    client_uuids.append(client_uuid)
                    if server_sock != None:
                        continue
                    server_sock = self.find_server_for_target(client_uuid, 'client')
            else:
                devices = [uuid + ':all']; client_uuids = [uuid]; server_sock = None
                server_sock = self.find_server_for_target(uuid, 'client')

            #self.netlog_print("{0} {1}".format(client_uuids, server_sock))
            if devices == []:
                content = pkt.construct(pkt.ACCESS_LOGIN, 'no allocated device')
                self.schedule_data_send(sock, content)
                self.netlog_print('denied terminal {0} login: no device allocated'.format(uuid))
                return
            if server_sock == None:
                content = pkt.construct(pkt.ACCESS_LOGIN, 'allocated devices not connected')
                self.schedule_data_send(sock, content)
                self.netlog_print('denied terminal {0} login: allocated devices not connected'.format(uuid))
                return

            token = self.generate_random_hexstr(16)
            devices = '|'.join(devices)
            content = '{0},{1},{2}'.format(uuid, token, devices)
            content = pkt.construct(pkt.ACCESS_ADD_TERMINAL, content)
            self.schedule_data_send(server_sock, content)
            self.connections[sock]['uuid'] = uuid
            self.netlog_print('terminal {0} login ... informing server'.format(uuid))
            return

    def print_user_info(self, uuid=None):
        if uuid == None:
            sqlcmd = "SELECT * FROM Users"
        else:
            sqlcmd = "SELECT * FROM Users where uuid = '{0}'".format(uuid)

        rows = self.database_excute_sqlcmd(sqlcmd)

        if rows == None:
            self.clilog_print("error: poll database failed")
            return

        if uuid == None:
            user_num = len(rows)
            self.clilog_print("users({0}):".format(user_num))
            for row in rows:
                key = row[0]; name = row[1]; info=row[2]
                devices = []
                for device in self.devices:
                    if self.devices[device]['uuid'] != key:
                        continue
                    devices.append(device)
                device_num = len(devices)
                if self.find_server_for_target(key, 'terminal') == None:
                    status = 'offline'
                else:
                    status = 'online'
                self.clilog_print("|--{0} ({1} {2}) {3} {4}".format(key, device_num, status, name, info))
                for device in devices:
                    try:
                        timeout = time.strftime("%Y-%m-%d@%H:%M:%S", time.localtime(self.devices[device]['timeout']))
                    except:
                        timeout = 'None'
                    self.clilog_print("|  |--{0} valid till {1}".format(device, timeout))
        else:
            if len(rows) == 0:
                self.clilog_print("error: uuid {0} does not exist in database".format(repr(uuid)))
                return
            self.clilog_print("users:")
            for row in rows:
                key = row[0]; name = row[1]; info=row[2]
                devices = []
                for device in self.devices:
                    if self.devices[device]['uuid'] != key:
                        continue
                    devices.append(device)
                device_num = len(devices)
                if self.find_server_for_target(key, 'terminal') == None:
                    status = 'offline'
                else:
                    status = 'online'
                self.clilog_print("|--{0} ({1} {2}) {3} {4}".format(key, device_num, status, name, info))
                for device in devices:
                    self.clilog_print("|  |--{0}".format(device))

    def print_server_info(self, uuid=None):
        self.clilog_print("servers:")
        tab = '  |'
        for conn in self.connections:
            if self.connections[conn]['valid'] == False:
                continue
            if self.connections[conn]['role'] != 'server':
                continue
            if uuid != None and uuid != self.connections[conn]['uuid']:
                continue
            server_uuid = self.connections[conn]['uuid']
            server_port = self.connections[conn]['addr'][1]
            self.clilog_print("|" + tab*0 + '--' + '{0}-{1}:'.format(server_uuid, server_port))
            if 'clients' not in self.connections[conn]['status']:
                continue
            terminal_num = len(self.connections[conn]['status']['terminals'])
            self.clilog_print("|" + tab*1 + '--' + 'terminals({0}):'.format(terminal_num))
            for terminal in self.connections[conn]['status']['terminals']:
                self.clilog_print('|' + tab*2 + '--' + terminal)
            if len(self.connections[conn]['status']['clients']) == 0:
                continue
            client_num = len(self.connections[conn]['status']['clients'])
            self.clilog_print("|" + tab*1 + '--' + 'clients({0}):'.format(client_num))
            for client in self.connections[conn]['status']['clients']:
                self.clilog_print('|' + tab*2 + '--' + client)
            if len(self.connections[conn]['status']['devices']) == 0:
                continue
            device_num = len(self.connections[conn]['status']['devices'])
            self.clilog_print("|" + tab*1 + '--' + 'devices({0}):'.format(device_num))
            for device in self.connections[conn]['status']['devices']:
                self.clilog_print('|     |--' + device)

    def print_client_info(self):
        sqlcmd = "SELECT * FROM Clients"
        rows = self.database_excute_sqlcmd(sqlcmd)
        if rows == None:
            self.clilog_print("error: poll database failed")
            return

        num = len(rows)
        self.clilog_print("clients({0}):".format(num))
        for row in rows:
            key = row[0]; name = row[1]; info=row[2]
            if self.find_server_for_target(key, 'client') == None:
                status = 'offline'
            else:
                status = 'online'
            self.clilog_print("|--{0} ({1}) {2} {3}".format(key, status, name, info))

    def print_developer_info(self):
        sqlcmd = "SELECT * FROM Developers"
        rows = self.database_excute_sqlcmd(sqlcmd)
        if rows == None:
            self.clilog_print("error: poll database failed")
            return

        num = len(rows)
        self.clilog_print("developers({0}):".format(num))
        for row in rows:
            key = row[0]; name = row[1]; info=row[2]
            if self.find_server_for_target(key, 'terminal') == None:
                terminal_status = 'offline'
            else:
                terminal_status = 'online'
            if self.find_server_for_target(key, 'client') == None:
                client_status = 'offline'
            else:
                client_status = 'online'
            self.clilog_print("|--{0} (terminal:{1}, client:{2}) {3} {4}".format(key, terminal_status, client_status, name, info))

    def inform_server_of_updates(self, uuid, type):
        types = ['delete_terminal', 'delete_client', 'update_terminal']
        if type not in types:
            return
        if 'terminal' in type:
            conn = self.find_server_for_target(uuid, 'terminal')
        elif 'client' in type:
            conn = self.find_server_for_target(uuid, 'client')
        else:
            conn = None
        if conn == None:
            return

        if type == 'delete_terminal':
            content = pkt.construct(pkt.ACCESS_DEL_TERMINAL, uuid)
        if type == 'delete_client':
            content = pkt.construct(pkt.ACCESS_DEL_CLIENT, uuid)
        elif type == 'update_terminal':
            devices = []
            for device in self.devices:
                if self.devices[device]['uuid'] != uuid:
                    continue
                devices.append(device)
            devices = '|'.join(devices)
            content = pkt.construct(pkt.ACCESS_UPDATE_TERMINAL, uuid + ',' + devices)
        self.schedule_data_send(conn, content)

    def command_add_handler(self, args):
        if len(args) < 2:
            self.clilog_print('usages: add user name [info1 info2 info3 ... infoN]')
            self.clilog_print('        add client name [info1 info2 info3 ... infoN]')
            self.clilog_print('        add developer name [info1 info2 info3 ... infoN]')
            self.clilog_print('        add device uuid device1|device2|...|deviceN [days]')
            return

        if args[0] in ['user', 'client', 'developer']:
            Tables = {'user':'Users', 'client':'Clients', 'developer':'Developers'}
            uuid = self.generate_random_hexstr(16)
            name = args[1]
            info = ' '.join(args[2:])
            table = Tables[args[0]]
            sqlcmd = "INSERT INTO {0} VALUES('{1}', '{2}', '{3}')".format(table, uuid, name, info)
            ret = self.database_excute_sqlcmd(sqlcmd)
            if ret == None:
                self.clilog_print("add {0} '{1}' failed".format(args[0], name))
            else:
                self.clilog_print("add {0} '{1}' succeed, uuid={2}".format(args[0], name, uuid))
        elif args[0] == 'device':
            if len(args) < 3:
                self.clilog_print('usage error, usage: add device uuid devices [days]')
                return
            uuid = args[1]
            dev_str = args[2]
            if len(args) > 3:
                try:
                    days = float(args[3])
                except:
                    self.clilog_print('error: invalid input {0}'.format(repr(args[3])))
                    return
            else:
                days = 7

            timeout = time.time() + 3600 * 24 * days

            sqlcmd = "SELECT * FROM Users WHERE uuid = '{0}'".format(uuid)
            rows = self.database_excute_sqlcmd(sqlcmd)
            if len(rows) < 1:
                self.clilog_print("error: user '{0}' does not exist".format(uuid))
                return

            devs = dev_str.split('|')
            devices = []
            for dev in devs:
                if re.match("^[0-9a-f]{16}:.", dev) == None:
                    self.clilog_print("error: invalid device {0}".format(dev))
                    return
                if dev in self.devices and self.devices[dev]['uuid'] != uuid:
                    self.clilog_print("error: device {0} already alloated".format(dev))
                    return
                devices += [dev]
            for device in devices:
                if device not in self.devices:
                    sqlcmd = "INSERT INTO Devices VALUES('{0}', '{1}', '{2}')".format(device, uuid, timeout)
                    ret = self.database_excute_sqlcmd(sqlcmd)
                else:
                    sqlcmd = "UPDATE Devices SET timeout = '{0}' WHERE device = '{1}'".format(timeout, device)
                    ret = self.database_excute_sqlcmd(sqlcmd)
                if ret == None:
                    self.clilog_print("add device '{0}' failed: error adding device to database".format(device))
                    return
                self.devices[device] = {'uuid': uuid, 'timeout': timeout}

            self.clilog_print("succeed")
            self.inform_server_of_updates(uuid, 'update_terminal')
        else:
            self.clilog_print("error: invalid command option {0}".format(repr(args[0])))
        return

    def command_del_handler(self, args):
        if len(args) < 2:
            self.clilog_print('usage: del user uuid')
            self.clilog_print('       del client uuid')
            self.clilog_print('       del device uuid device1|device2|...|deviceN')
            return

        option = args[0]
        uuid   = args[1]
        if len(args) > 2:
            dev_str = args[2]
        else:
            dev_str = None
        if option == 'client':
            sqlcmd = "SELECT * FROM Clients WHERE uuid = '{0}'".format(uuid)
            rows = self.database_excute_sqlcmd(sqlcmd)
            if len(rows) < 1:
                self.clilog_print("error: client '{0}' does not exist".format(uuid))
                return
        else:
            sqlcmd = "SELECT * FROM Users WHERE uuid = '{0}'".format(uuid)
            rows = self.database_excute_sqlcmd(sqlcmd)
            if len(rows) < 1:
                self.clilog_print("error: user '{0}' does not exist".format(uuid))
                return

        if option == 'user':
            sqlcmd = "DELETE FROM Devices WHERE uuid = '{0}'".format(uuid)
            ret = self.database_excute_sqlcmd(sqlcmd)
            if ret == None:
                self.clilog_print("warning: delete devices of {0} from database failed".format(uuid))
            for device in list(self.devices):
                if self.devices[device]['uuid'] != uuid:
                    continue
                self.devices.pop(device)
            sqlcmd = "DELETE FROM Users WHERE uuid = '{0}'".format(uuid)
            ret = self.database_excute_sqlcmd(sqlcmd)
            if ret == None:
                self.clilog_print("failed")
            else:
                self.clilog_print("succeed")
            self.inform_server_of_updates(uuid, 'delete_terminal')
            return
        elif option == 'client':
            sqlcmd = "DELETE FROM Clients WHERE uuid = '{0}'".format(uuid)
            ret = self.database_excute_sqlcmd(sqlcmd)
            if ret == None:
                self.clilog_print("failed")
            else:
                self.clilog_print("succeed")
            self.inform_server_of_updates(uuid, 'delete_client')
            return
        elif option == 'device':
            if dev_str == None:
                self.clilog_print("error: please input the devices you want to delete")
                return

            del_devices = dev_str.split('|')
            for device in del_devices:
                if re.match("^[0-9a-f]{12,16}:.", device) == None:
                    self.clilog_print("error: invalid device {0}".format(device))
                    return
                if device not in self.devices or self.devices[device]['uuid'] != uuid:
                    self.clilog_print("error: user {0} does not own device {1} ".format(uuid, device))
                    return
            if del_devices == []:
                return
            for device in del_devices:
                sqlcmd = "DELETE FROM Devices WHERE device = '{0}'".format(device)
                ret = self.database_excute_sqlcmd(sqlcmd)
                if ret == None:
                    self.clilog_print("warning: delete device {0} from database failed".format(device))
                self.devices.pop(device)
            self.clilog_print("succeed")
            self.inform_server_of_updates(uuid, 'update_terminal')
            return
        else:
            self.clilog_print('usage error, invalid argument {0}'.format(repr(option)))
            return
        return

    def command_alloc_handler(self, args):
        if len(args) < 3:
            self.clilog_print('usage: allocate uuid nubmer model [days]')
            return

        uuid = args[0]
        number = args[1]
        model = args[2]
        if len(args) > 3:
            try:
                days = float(args[3])
            except:
                self.clilog_print('error: invalid input {0}'.format(repr(args[3])))
                return
        else:
            days = 7

        sqlcmd = "SELECT * FROM Users WHERE uuid = '{0}'".format(uuid)
        rows = self.database_excute_sqlcmd(sqlcmd)
        if len(rows) < 1:
            self.clilog_print("error: user '{0}' does not exist".format(uuid))
            return

        try:
            number = int(number)
        except:
            number = 0
        if number <= 0:
            self.clilog_print('error: invalid input {0}, input a positive integer'.format(args[1]))
            return

        ext_server = None
        ext_devices = []
        for device in self.devices:
            if self.devices[device]['uuid'] != uuid:
                continue
            ext_devices.append(device)
            conn = self.find_server_for_target(device, 'device')
            if conn == None:
                continue
            ext_server = conn
            break
        if ext_devices != [] and ext_server == None:
            self.clilog_print('error: can not locate the exist server for {0}'.format(uuid))
            return
        if ext_server == None:
            allocated = []
            for conn in self.connections:
                if self.connections[conn]['role'] != 'server':
                    continue
                if self.connections[conn]['valid'] == False:
                    continue
                if 'devices' not in self.connections[conn]['status']:
                    continue
                allocated = []
                for dev in self.connections[conn]['status']['devices']:
                    if model not in dev:
                        continue
                    if dev in self.devices:
                        continue
                    allocated.append(dev)
                    if len(allocated) >= number:
                        break
                if len(allocated) >= number:
                    break
        else:
            allocated = []
            for dev in self.connections[ext_server]['status']['devices']:
                if model not in dev:
                    continue
                if dev in self.devices:
                    continue
                allocated.append(dev)
                if len(allocated) >= number:
                    break

        if len(allocated) < number:
            self.clilog_print('failed')
            return
        timeout = time.time() + 3600 * 24 * days
        for device in allocated:
            sqlcmd = "INSERT INTO Devices VALUES('{0}', '{1}', '{2}')".format(device, uuid, timeout)
            ret = self.database_excute_sqlcmd(sqlcmd)
            if ret == None:
                self.clilog_print("add device '{0}' failed: error adding device to database".format(device))
                return
            self.devices[device] = {'uuid': uuid, 'timeout': timeout}
        self.clilog_print('succeed, allocat: {0}'.format('|'.join(allocated)))
        self.inform_server_of_updates(uuid, 'update_terminal')

    def command_list_handler(self, args):
        if len(args) == 0:
            self.print_user_info(None)
            self.clilog_print('')
            self.print_client_info()
            self.clilog_print('')
            self.print_developer_info()
            self.clilog_print('')
            self.print_server_info()

        if len(args) >= 1:
            if args[0] == 'user':
                uuid = None
                if len(args) >= 2:
                    uuid = args[1]
                self.print_user_info(uuid)
            elif args[0] == 'server':
                uuid = None
                if len(args) >= 2:
                    uuid = args[1]
                self.print_server_info(uuid)
            elif args[0] == 'client':
                self.print_client_info()
            elif args[0] == 'developer':
                self.print_developer_info()
        return

    def command_help_handler(self, args):
        self.clilog_print("Usages:")
        self.clilog_print("    add [ad]:")
        self.clilog_print("        |-add user name info")
        self.clilog_print("        |-add client name info")
        self.clilog_print("        |-add developer name info")
        self.clilog_print("        |-add device uuid device1|deice2|...|deviceN [days(7)]")
        self.clilog_print("    delete [dl]: user uuid")
        self.clilog_print("        |-delete user uuid")
        self.clilog_print("        |-delete device uuid devic1|device2|...|deviceN")
        self.clilog_print("    allocate [al]: uuid number model")
        self.clilog_print("        |-allocate uuid number model")
        self.clilog_print("    list [ls]:")
        self.clilog_print("        |-list")
        self.clilog_print("        |-list user [uuid]")
        self.clilog_print("        |-list server [uuid]")
        self.clilog_print("        |-list client [uuid]")
        return

    def process_cmd(self):
        cmds = self.user_cmd.split()
        if cmds[0] == 'add' or cmds[0] == 'ad':
            self.command_add_handler(cmds[1:])
        elif cmds[0] == 'del' or cmds[0] == 'dl':
            self.command_del_handler(cmds[1:])
        elif cmds[0] == 'alloc' or cmds[0] == 'al':
            self.command_alloc_handler(cmds[1:])
        elif cmds[0] == 'list' or cmds[0] == 'ls':
            self.command_list_handler(cmds[1:])
        elif cmds[0] == 'help' or cmds[0] == 'h':
            self.command_help_handler(cmds[1:])
        else:
            sys.stdout.write("unknow command {0}\r\n".format(repr(self.user_cmd)))
        sys.stdout.flush()
        return

    def sock_interact_thread(self, port):
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        sock.bind((self.host, port))
        if ENCRYPT:
            sock = ssl.wrap_socket(sock, self.keyfile, self.certfile, True)
        sock.setblocking(False)
        sock.listen(100)
        self.selector = selector()
        self.connections = {}
        self.timeouts = {}
        self.selector.register(sock, selector.EVENT_READ, self.net_conn_accept)
        while self.keep_running:
            events = self.selector.select()
            for [sock, callback] in events:
                self.debug_log("{0}: enter {1} function".format(time.time(), callback.__name__))
                callback(sock)
                self.debug_log("{0}: exit {1} function".format(time.time(), callback.__name__))

            #close timeout connections
            now = time.time()
            self.debug_log("{0}: start proccessing timeout".format(time.time()))
            for conn in list(self.timeouts):
                if now < self.timeouts[conn]:
                    continue
                role = self.connections[conn]['role']
                addr = self.connections[conn]['addr']
                self.netlog_print("{0} {1} timeout, close connection".format(role, addr))
                self.selector.unregister(conn, selector.EVENT_READ)
                self.selector.unregister(conn, selector.EVENT_WRITE)
                conn.close()
                self.timeouts.pop(conn)
                self.connections.pop(conn)
            self.debug_log("{0}: end proccessing timeout".format(time.time()))
            os.system('touch ~/.udcontroller/running')

    def house_keeping_thread(self):
        while self.keep_running:
            time.sleep(1)

            #remove timeouted devices
            now = time.time()
            for device in list(self.devices):
                if now <= self.devices[device]['timeout']:
                    continue
                uuid = self.devices[device]['uuid']
                self.devices.pop(device)
                sqlcmd = "DELETE FROM Devices WHERE device = '{0}'".format(device)
                ret = self.database_excute_sqlcmd(sqlcmd)
                if ret == None:
                    self.clilog_print("warning: delete device {0} from Devices database failed".format(device))
                self.inform_server_of_updates(uuid, 'update_terminal')

    def user_cli_thread(self):
        self.user_cmd = ''
        self.cur_pos = 0
        saved_cmd = ""
        history_index = -1
        escape = None
        old_settings = termios.tcgetattr(sys.stdin.fileno())
        tty.setraw(sys.stdin.fileno())
        self.usercmd_print()
        while self.keep_running:
            try:
                c = sys.stdin.read(1)
            except:
                break
            #sys.stdout.write("\rkeycode {0}\r\n".format(ord(c)))
            if escape != None:
                escape += c
                if len(escape) == 2:
                    if ord(c) == 91:
                        continue
                    else:
                        escape = None

                if len(escape) == 3:
                    if ord(c) == 65: #KEY_UP
                        if history_index == -1:
                            saved_cmd = self.user_cmd
                        if history_index < (len(self.cmd_history) - 1):
                            history_index += 1
                        sys.stdout.write("\r  " + " " * len(self.user_cmd))
                        self.user_cmd = self.cmd_history[history_index]
                        self.cur_pos = len(self.user_cmd)
                        self.usercmd_print()
                        continue
                    if ord(c) == 66: #KEY_DOWN
                        if history_index <= -1:
                            history_index = -1
                            continue
                        history_index -= 1
                        sys.stdout.write("\r  " + " " * len(self.user_cmd))
                        if history_index >= 0:
                            self.user_cmd = self.cmd_history[history_index]
                        else:
                            self.user_cmd = saved_cmd
                        self.cur_pos = len(self.user_cmd)
                        self.usercmd_print()
                        continue
                    if ord(c) == 68: #KEY_LEFT
                        if self.cur_pos <= 0:
                            continue
                        self.cur_pos -= 1
                        self.usercmd_print()
                        continue
                    if ord(c) == 67: #KEY_RIGHT
                        if self.cur_pos >= len(self.user_cmd):
                            continue
                        self.cur_pos += 1
                        self.usercmd_print()
                        continue

            if ord(c) == 27: #ESCAPE
                escape = c
                continue
            if ord(c) == 13: #RETURN
                if self.user_cmd == "q" :
                    self.keep_running = False
                    time.sleep(0.2)
                    break
                elif self.user_cmd != "":
                    sys.stdout.write('\r# ' + self.user_cmd + '\r\n')
                    sys.stdout.flush()
                    self.process_cmd()
                    self.cmd_history = [self.user_cmd] + self.cmd_history
                self.user_cmd = ""
                saved_cmd = ""
                history_index = -1
                self.cur_pos = 0
                self.usercmd_print()
                continue
            if c == '\x08' or c == '\x7f': #DELETE
                if self.user_cmd[0:self.cur_pos] == "":
                    continue
                newcmd = self.user_cmd[0:self.cur_pos-1] + self.user_cmd[self.cur_pos:]
                self.user_cmd = newcmd
                self.cur_pos -= 1
                self.usercmd_print()
                continue
            if ord(c) == 3: #CTRL+C
                self.keep_running = False
                time.sleep(0.2)
                break

            try:
                newcmd = self.user_cmd[0:self.cur_pos] + c + self.user_cmd[self.cur_pos:]
                self.user_cmd = newcmd
                self.cur_pos += 1
                self.usercmd_print()
            except:
                if DEBUG: traceback.print_exc()
                sys.stdout.write("\rError: unsupported unicode character {0}\n".format(c))
                self.usercmd_print()
                continue
        termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, old_settings)
        try:
            if len(self.cmd_history) > 0:
                cmd_history_path = path.join(path.expanduser('~'), '.udcontroller', 'cmd_history')
                file = open(cmd_history_path,'wb')
                json.dump(self.cmd_history, file)
                file.close()
        except:
            print("error: save command history failed")
        print ''
        return

    def main(self):
        signal.signal(signal.SIGINT, signal_handler)
        self.keep_running = True
        thread.start_new_thread(self.sock_interact_thread, (self.port,))
        thread.start_new_thread(self.house_keeping_thread, ())
        self.user_cli_thread()
