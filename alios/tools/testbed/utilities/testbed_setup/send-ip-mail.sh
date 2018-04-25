#!/bin/bash

recipients="lc122798@alibaba-inc.com"

# check network availability
while true
do
    TIMEOUT=5
    SITE_TO_CHECK="www.126.com"
    RET_CODE=`curl -I -s --connect-timeout $TIMEOUT $SITE_TO_CHECK -w %{http_code} | tail -n1`
    if [ "x$RET_CODE" = "x200" ]; then
    echo "Network OK, will send mail..."
    break
    else
    echo "Network not ready, wait..."
    sleep 1s
    fi
done

# get the IP address of eth0, e.g. "192.168.16.5"
IP_ADDR=`ifconfig`

# send the Email
echo -e "Current time: `date '+%F %T'`\n\n${IP_ADDR}" | mutt -s "`uname -n` IP infomation" -- ${recipients}
