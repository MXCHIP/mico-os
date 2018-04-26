This folder contains all board support files.

To add a new borad, you need to:
##### 1. add a new folder to ./board with its name to be 'boardname'
```bash
mkdir ./board/boardname && cd ./boadr/boardname
```
##### 2. write a python script named boardname.py, put it in ./boardname folder
```bash
vim boardname.py
```
implement the following functions in boardname.py:
```python
def list_devices(host_os):
"""return pluged devices of this board type"""
return available_device_list

def exist(device):
"""return if a device exist"""
return True/False

def new_device(device):
"""add a new device, retun a file handle connected to device's shell I/O"""
return shell_handle/None

def erase(device):
"""erase device flash"""
return 'success/fail'

def program(device, address, file):
"""program file to device @ address"""
return 'success/fail'

def control(device, operation):
"""control device, operation can be 'start', 'stop' or 'reset'"""
return 'success/fail'
```

Should you have any questions, please contact lc122798@alibaba-inc.com.
