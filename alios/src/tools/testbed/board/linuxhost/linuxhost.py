import os, sys, time, serial, subprocess, traceback, pty

def list_devices(host_os):
    return []
    return ['/dev/tty20', '/dev/tty21']

def exist(device):
    return os.path.exists(device)

def new_device(device):
    return open(device, 'rb+')

def erase(device):
    error = 'fail'
    return error

def program(device, address, file):
    error = 'fail'
    return error

def control(device, operation):
    ret = 'fail'
    return ret
