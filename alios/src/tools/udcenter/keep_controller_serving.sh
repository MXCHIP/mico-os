#!/bin/bash

while true; do
    now=`date +'%s'`
    last=`stat -c %Y ~/.udcontroller/running`
    diff=$((now-last))
    if [ ${diff} -gt 30 ]; then
        stoptime=`date +'%Y-%m-%d@%H-%M-%S'`
        echo "detected controller halt at ${stoptime}, restart it"
        udcontroller_stop
        echo "controller stop at ${stoptime}" >> ~/.udcontroller/runlog.txt
        udcontroller_start
    fi
    sleep 2
done
