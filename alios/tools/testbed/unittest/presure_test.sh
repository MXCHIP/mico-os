#/bin/bash

device_num=5
if [ $# -gt 0 ]; then
    device_num=$1
fi

count=1
socat_pids=""
emulator_pids=""
while [ ${count} -le ${device_num} ]; do
    if [ ${count} -lt 10 ]; then
        sudo socat pty,raw,b115200,link=/dev/emulator-"00${count}" pty,raw,b115200,link=/dev/emu${count} &
        socat_pids="${socat_pids} $!"
    elif [ ${count} -lt 100 ]; then
        sudo socat pty,raw,b115200,link=/dev/emulator-"0${count}" pty,raw,b115200,link=/dev/emu${count} &
        socat_pids="${socat_pids} $!"
    else
        sudo socat pty,raw,b115200,link=/dev/emulator-"${count}" pty,raw,b115200,link=/dev/emu${count} &
        socat_pids="${socat_pids} $!"
    fi
    sleep 0.05
    sudo chmod 660 /dev/pts/*
    python emulator.py /dev/emu${count} &
    emulator_pids="${emulator_pids} $!"
    count=$((count+1))
done

keep_running="yes"
trap 'keep_running="no"' 2
while [ "${keep_running}" = "yes" ]; do
    sleep 0.1
done
kill -2 ${emulator_pids}
sleep 0.8
sudo kill ${socat_pids}
