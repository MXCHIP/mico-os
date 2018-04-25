import os, re, time, thread, threading, traceback

DEBUG = False

class emulator:
    def __init__(self, port, log_interval):
        self.port = port
        self.log_interval = log_interval
        self.tag = ''

        #generate mac address
        self.macaddr = ''
        bytes = os.urandom(6)
        for byte in bytes:
            self.macaddr += '{0:02x}-'.format(ord(byte))

        #generate uuid
        bytes = os.urandom(16)
        if ord(bytes[0]) < 51:
            self.uuid = 'alink is not connected'
        else:
            self.uuid = 'uuid: '
            for byte in bytes:
                self.uuid += '{0:02X}'.format(ord(byte))
        self.macaddr = self.macaddr[:-1]
        self.kernel_version = 'AOS-R-1.2.1'
        self.app_version = 'app-1.1.0-' + time.strftime("%Y%m%d.%H%M")
        self.ser = None
        try:
            import serial
        except:
            print 'error: pyserial is NOT installed'
            return
        self.ser = serial.Serial(port, 115200, timeout = 0.02)
        self.running = False
        self.builtin_cmds = ['help', 'devname', 'mac', 'version', 'uuid', 'umesh']
        self.umesh_cmds = ['help', 'status', 'nbrs', 'extnetid']

    def put_line(self, content):
        self.ser.write(self.tag + content + '\r\n')
        if DEBUG: print self.tag + content

    def put_prompt(self):
        self.ser.write(self.tag + '\r\n# ')
        if DEBUG: print self.tag+'\r\n#',

    def cmd_process(self, args):
        self.put_line(' '.join(args))
        if args[0] not in self.builtin_cmds:
            self.put_line('unkonwn command: {0}'.format(' '.join(args)))
            self.put_prompt()
            return

        if args[0] == 'help':
            self.put_line('built-in commands:')
            for cmd in self.builtin_cmds:
                self.put_line(cmd)
            self.put_prompt()
            return
        if args[0] == 'devname':
            self.put_line('emulator')
            self.put_prompt()
            return
        if args[0] == 'mac':
            self.put_line(self.macaddr)
            self.put_prompt()
            return
        if args[0] == 'version':
            self.put_line('kernel version :' + self.kernel_version)
            self.put_line('app versin :' + self.app_version)
            self.put_prompt()
            return
        if args[0] == 'uuid':
            self.put_line(self.uuid)
            self.put_prompt()
            return
        if args[0] == 'umesh':
            if len(args) < 2 or args[1] not in self.umesh_cmds:
                self.put_line('')
                self.put_prompt()
                return
            if args[1] == 'help':
                for cmd in self.umesh_cmds:
                    self.put_line(cmd)
                self.put_prompt()
                return
            if args[1] == 'status':
                self.put_line('state\tdetatched')
                self.put_line('\tattach\tidle')
                self.put_line('<<network wifi 0>>')
                self.put_line('\tnetid\t0xfffe')
                self.put_line('\tmac\t' + self.macaddr.replace('-', '') + '0000')
                self.put_line('\tsid\t0xfffe')
                self.put_line('\tnetsize\t0')
                self.put_line('\trouter\tSID_ROUTER')
                self.put_line('\tbcast_mut\t512')
                self.put_line('\tucast_mut\t512')
                self.put_line('\tuptime\t' + str(int(time.time() - self.start_time)))
                self.put_line('\tchannel\t0')
                self.put_prompt()
                return
            if args[1] == 'nbrs':
                self.put_line('\t<<hal type wifi>>')
                self.put_line('\tnum=0')
                self.put_prompt()
                return
            if args[1] == 'extnetid':
                self.put_line('01:02:03:04:05:06')
                self.put_line('done')
                self.put_prompt()
                return

    def main_loop(self):
        print 'emulator thread started'
        self.start_time = time.time()
        tag_seq = re.compile(r"\x1b\[t[0-9a-f]+m")
        cmd = ''
        try:
            self.ser.write('# ')
        except:
            traceback.print_exc()
            self.running = False
            return
        log_time = self.start_time + ord(os.urandom(1)) * self.log_interval / 128
        log_count = 0
        while self.running and os.path.exists(self.port):
            if time.time() >= log_time:
                self.ser.write('random log {0}\r\n'.format(log_count))
                if DEBUG: print 'random log {0}'.format(log_count)
                log_count += 1
                log_time += ord(os.urandom(1)) * self.log_interval / 128
            try:
                c = self.ser.read(1)
                if c == '':
                    continue
                elif c != '\r' and c != '\n':
                    cmd += c
                    continue
            except:
                if os.path.exists(self.port) == False:
                    break
                traceback.print_exc()

            if cmd == '':
                self.ser.write('\r\n')
                continue
            tag_match = tag_seq.match(cmd)
            if tag_match == None:
                self.tag = ''
            else:
                self.tag = cmd[tag_match.start():tag_match.end()]
                cmd = tag_seq.sub('', cmd)
            if cmd == '':
                self.ser.write('error: empty command\r\n')
                continue
            args = cmd.split()
            self.cmd_process(args)
            cmd = ''
        self.running = False
        print 'emulator thread exited'

    def start(self):
        if self.ser == None:
            return False
        self.running = True
        thread.start_new_thread(self.main_loop, ())

    def stop(self):
        if self.running == False:
            return False
        self.running = False
        time.sleep(0.1)
        return True

if __name__ == '__main__':
    import sys
    if len(sys.argv) < 2:
        print "Usage: {0} port [log_interval]".format(sys.argv[0])
        sys.exit(1)
    if os.path.exists(sys.argv[1]) == False:
        print "error: port {0} does not exist".format(sys.argv[1])
        sys.exit(1)
    port = sys.argv[1]
    if len(sys.argv) > 2:
        try:
            interval = float(sys.argv[2])
        except:
            print "error: invalid log interval {0}".format(sys.argv[2])
            sys.exit(1)
    else:
        interval = 2
    emu = emulator(port, interval)
    emu.start()
    while os.path.exists(port):
        try:
            time.sleep(1)
        except:
            break
    emu.stop()
    time.sleep(0.1)
    sys.exit(0)

