#!/bin/bash

performance_tests="alink_5pps mesh_5pps stability_noalink"
mesh_tests="line_topology_v4_test.py tree_topology_v4_test.py multicast_v4_test.py"
recipients="apsaras73@list.alibaba-inc.com"
meshrecipients="xiaoma.mx@alibaba-inc.com gubiao.dc@alibaba-inc.com yuanlu.gl@alibaba-inc.com wanglu.luwang@alibaba-inc.com wenjunchen.cwj@alibaba-inc.com simen.cjj@alibaba-inc.com lc122798@alibaba-inc.com"
#recipients="lc122798@alibaba-inc.com"
#meshrecipients="lc122798@alibaba-inc.com"
logfile=~/auto_test_log.txt
mesh_pass_total=0
mesh_fail_total=0
mk3060firmware=""
esp32firmware=""

#Usage: bash autotest.sh mk3060_firmware esp32_firmware
if [ $# -gt 0 ]; then
    mk3060firmware=$1
fi
if [ $# -gt 1 ]; then
    esp32firmware=$2
fi

echo -e "Start Alink 5PPS/2PPS and stability tests\n" > ${logfile}
alinkpids=""
prefix=
if [ -f ${mk3060firmware} ]; then
    if [[ ${performance_tests} == *alink_5pps* ]]; then
        model=mk3060
        python alink_testrun.py --testname=${prefix}5pps --firmware=${mk3060firmware} --model=${model} > ~/${model}_5pps.txt 2>&1 &
        mk3060_5pps_pid=$!
        alinkpids="${alinkpids} ${mk3060_5pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *alink_2pps* ]]; then
        python alink_testrun.py --testname=${prefix}2pps --firmware=${mk3060firmware} --model=${model} > ~/${model}_2pps.txt 2>&1 &
        mk3060_2pps_pid=$!
        alinkpids="${alinkpids} ${mk3060_2pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *mesh_5pps* ]]; then
        python alink_mesh_testrun.py --testname=${prefix}5pps --firmware=${mk3060firmware} --model=${model} > ~/${model}_mesh_5pps.txt 2>&1 &
        mk3060_mesh_5pps_pid=$!
        alinkpids="${alinkpids} ${mk3060_mesh_5pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *mesh_2pps* ]]; then
        python alink_mesh_testrun.py --testname=${prefix}2pps --firmware=${mk3060firmware} --model=${model} > ~/${model}_mesh_2pps.txt 2>&1 &
        mk3060_mesh_2pps_pid=$!
        alinkpids="${alinkpids} ${mk3060_mesh_2pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *stability_alink* ]]; then
        python stability_test.py --firmware=${mk3060firmware} --model=${model} --withalink=1 > ~/${model}_stability_alink.txt 2>&1 &
        mk3060_stability_alink_pid=$!
        alinkpids="${alinkpids} ${mk3060_stability_alink_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *stability_noalink* ]]; then
        python stability_test.py --firmware=${mk3060firmware} --model=${model} --withalink=0 > ~/${model}_stability_noalink.txt 2>&1 &
        mk3060_stability_noalink_pid=$!
        alinkpids="${alinkpids} ${mk3060_stability_noalink_pid}"
        sleep 30
    fi
fi
if [ -f ${esp32firmware} ]; then
    if [[ ${performance_tests} == *alink_5pps* ]]; then
        model=esp32
        python alink_testrun.py --testname=${prefix}5pps --firmware=${esp32firmware} --model=${model} > ~/${model}_5pps.txt 2>&1 &
        esp32_5pps_pid=$!
        alinkpids="${alinkpids} ${esp32_5pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *alink_2pps* ]]; then
        python alink_testrun.py --testname=${prefix}2pps --firmware=${esp32firmware} --model=${model} > ~/${model}_2pps.txt 2>&1 &
        esp32_2pps_pid=$!
        alinkpids="${alinkpids} ${esp32_2pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *mesh_5pps* ]]; then
        python alink_mesh_testrun.py --testname=${prefix}5pps --firmware=${esp32firmware} --model=${model} > ~/${model}_mesh_5pps.txt 2>&1 &
        esp32_mesh_5pps_pid=$!
        alinkpids="${alinkpids} ${esp32_mesh_5pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *mesh_2pps* ]]; then
        python alink_mesh_testrun.py --testname=${prefix}2pps --firmware=${esp32firmware} --model=${model} > ~/${model}_mesh_2pps.txt 2>&1 &
        esp32_mesh_2pps_pid=$!
        alinkpids="${alinkpids} ${esp32_mesh_2pps_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *stability_alink* ]]; then
        python stability_test.py --firmware=${esp32firmware} --model=${model} --withalink=1 > ~/${model}_stability_alink.txt 2>&1 &
        esp32_stability_alink_pid=$!
        alinkpids="${alinkpids} ${esp32_stability_alink_pid}"
        sleep 30
    fi

    if [[ ${performance_tests} == *stability_noalink* ]]; then
        python stability_test.py --firmware=${esp32firmware} --model=${model} --withalink=0 > ~/${model}_stability_noalink.txt 2>&1 &
        esp32_stability_noalink_pid=$!
        alinkpids="${alinkpids} ${esp32_stability_noalink_pid}"
        sleep 30
    fi
fi

title=""
if [ -f ${mk3060firmware} ]; then
    model=mk3060
    mesh_pass=0
    mesh_fail=0
    echo -e "Start MESH functional autotest with ${model} ...\n" >> ${logfile}
    for test in ${mesh_tests}; do
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        echo -e "start ${test}\n" >> ${logfile}
        python ${test} --firmware=${mk3060firmware} --model=${model} >> ${logfile} 2>&1
        if [ $? -eq 0 ]; then
            ret="success"
            mesh_pass_total=$((mesh_pass_total+1))
            mesh_pass=$((mesh_pass+1))
        else
            ret="fail"
            mesh_fail_total=$((mesh_fail_total+1))
            mesh_fail=$((mesh_fail+1))
        fi
        echo -e "\nfinished ${test}, result=${ret}" >> ${logfile}
    done
    echo -e "\n---------------------------------------------------------\n" >> ${logfile}
    echo -e "Finished MESH functional autotest with ${model}, PASS:${mesh_pass} FAIL:${mesh_fail}\n" >> ${logfile}

    title="${title} ${model}-PASS-${mesh_pass} FAIL-${mesh_fail};"
fi
if [ -f ${esp32firmware} ]; then
    model=esp32
    mesh_pass=0
    mesh_fail=0
    echo -e "Start MESH functional autotest with ${model} ...\n" >> ${logfile}
    for test in ${mesh_tests}; do
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        echo -e "start ${test}\n" >> ${logfile}
        python ${test} --firmware=${esp32firmware} --model=${model} >> ${logfile} 2>&1
        if [ $? -eq 0 ]; then
            ret="success"
            mesh_pass_total=$((mesh_pass_total+1))
            mesh_pass=$((mesh_pass+1))
        else
            ret="fail"
            mesh_fail_total=$((mesh_fail_total+1))
            mesh_fail=$((mesh_fail+1))
        fi
        echo -e "\nfinished ${test}, result=${ret}" >> ${logfile}
    done
    echo -e "\n---------------------------------------------------------\n" >> ${logfile}
    echo -e "Finished MESH functional automactic test with ${model}, PASS:${mesh_pass} FAIL:${mesh_fail}\n" >> ${logfile}

    title="${title} ${model}-PASS:${mesh_pass} FAIL:${mesh_fail};"
fi

#send result to mesh group first
if [ -f ${mk3060firmware} ] || [ -f ${esp32firmware} ]; then
    title="Mesh Autotest:${title}"
    cat ${logfile} | mutt -s "${title}" -- ${meshrecipients}
fi

running=""
for pid in ${alinkpids}; do
    running=`ps | grep ${pid}`
    if [ "${running}" != "" ]; then
        break
    fi
done
while [ "${running}" != "" ]; do
    sleep 60
    running=""
    for pid in ${alinkpids}; do
        running=`ps | grep ${pid}`
        if [ "${running}" != "" ]; then
            break
        fi
    done
done


title=""
if [ -f ${mk3060firmware} ]; then
    model=mk3060
    title="${title} MK3060:"
    if [[ ${performance_tests} == *alink_5pps* ]]; then
        wait ${mk3060_5pps_pid}
        mk3060_5pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${mk3060_5pps_ret} -eq 0 ]; then
            echo -e "run Alink 5PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} 5PPS-PASS"
        else
            echo -e "run Alink 5PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} 5PPS-FAIL"
        fi
        cat ~/${model}_5pps.txt >> ${logfile}
        rm -f ~/${model}_5pps.txt
    fi

    if [[ ${performance_tests} == *alink_2pps* ]]; then
        wait ${mk3060_2pps_pid}
        mk3060_2pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${mk3060_2pps_ret} -eq 0 ]; then
            echo -e "run Alink 2PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} 2PPS-PASS;"
        else
            echo -e "run Alink 2PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} 2PPS-FAIL;"
        fi
        cat ~/${model}_2pps.txt >> ${logfile}
        rm -f ~/${model}_2pps.txt
    fi

    if [[ ${performance_tests} == *mesh_5pps* ]]; then
        wait ${mk3060_mesh_5pps_pid}
        mk3060_mesh_5pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${mk3060_mesh_5pps_ret} -eq 0 ]; then
            echo -e "run Alink MESH5PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} MESH5PPS-PASS"
        else
            echo -e "run Alink MESH5PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} MESH5PPS-FAIL"
        fi
        cat ~/${model}_mesh_5pps.txt >> ${logfile}
        rm -f ~/${model}_mesh_5pps.txt
    fi

    if [[ ${performance_tests} == *mesh_2pps* ]]; then
        wait ${mk3060_mesh_2pps_pid}
        mk3060_mesh_2pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${mk3060_mesh_2pps_ret} -eq 0 ]; then
            echo -e "run Alink MESH2PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} MESH2PPS-PASS;"
        else
            echo -e "run Alink MESH2PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} MESH2PPS-FAIL;"
        fi
        cat ~/${model}_mesh_2pps.txt >> ${logfile}
        rm -f ~/${model}_mesh_2pps.txt
    fi

    if [[ ${performance_tests} == *stability_alink* ]]; then
        wait ${mk3060_stability_alink_pid}
        mk3060_stability_alink_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${mk3060_stability_alink_ret} -eq 0 ]; then
            echo -e "run stability test with alink connected at ${model} succeed, log:\n" >> ${logfile}
        else
            echo -e "run stability test with alink connected at ${model} failed, log:\n" >> ${logfile}
        fi
        cat ~/${model}_stability_alink.txt >> ${logfile}
        rm -f ~/${model}_stability_alink.txt
    fi

    if [[ ${performance_tests} == *stability_noalink* ]]; then
        wait ${mk3060_stability_noalink_pid}
        mk3060_stability_noalink_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${mk3060_stability_noalink_ret} -eq 0 ]; then
            echo -e "run stability test with alink disconnected at ${model} succeed, log:\n" >> ${logfile}
        else
            echo -e "run stability test with alink disconnected at ${model} failed, log:\n" >> ${logfile}
        fi
        cat ~/${model}_stability_noalink.txt >> ${logfile}
        rm -f ~/${model}_stability_noalink.txt
    fi
    title="${title};"
fi

if [ -f ${esp32firmware} ]; then
    model=esp32
    title="${title} ESP32:"
    if [[ ${performance_tests} == *alink_5pps* ]]; then
        wait ${esp32_5pps_pid}
        esp32_5pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${esp32_5pps_ret} -eq 0 ]; then
            echo -e "run Alink 5PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} 5PPS-PASS"
        else
            echo -e "run Alink 5PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} 5PPS-FAIL"
        fi
        cat ~/${model}_5pps.txt >> ${logfile}
        rm -f ~/${model}_5pps.txt
    fi

    if [[ ${performance_tests} == *alink_2pps* ]]; then
        wait ${esp32_2pps_pid}
        esp32_2pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${esp32_2pps_ret} -eq 0 ]; then
            echo -e "run Alink 2PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} 2PPS-PASS;"
        else
            echo -e "run Alink 2PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} 2PPS-FAIL;"
        fi
        cat ~/${model}_2pps.txt >> ${logfile}
        rm -f ~/${model}_2pps.txt
    fi

    if [[ ${performance_tests} == *mesh_5pps* ]]; then
        wait ${esp32_mesh_5pps_pid}
        esp32_mesh_5pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${esp32_mesh_5pps_ret} -eq 0 ]; then
            echo -e "run Alink MESH5PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} MESH5PPS-PASS"
        else
            echo -e "run Alink MESH5PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} MESH5PPS-FAIL"
        fi
        cat ~/${model}_mesh_5pps.txt >> ${logfile}
        rm -f ~/${model}_mesh_5pps.txt
    fi

    if [[ ${performance_tests} == *mesh_2pps* ]]; then
        wait ${esp32_mesh_2pps_pid}
        esp32_mesh_2pps_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${esp32_mesh_2pps_ret} -eq 0 ]; then
            echo -e "run Alink MESH2PPS test with ${model} succeed, log:\n" >> ${logfile}
            title="${title} MESH2PPS-PASS"
        else
            echo -e "run Alink MESH2PPS test with ${model} failed, log:\n" >> ${logfile}
            title="${title} MESH2PPS-FAIL"
        fi
        cat ~/${model}_mesh_2pps.txt >> ${logfile}
        rm -f ~/${model}_mesh_2pps.txt
    fi

    if [[ ${performance_tests} == *stability_alink* ]]; then
        wait ${esp32_stability_alink_pid}
        esp32_stability_alink_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${esp32_stability_alink_ret} -eq 0 ]; then
            echo -e "run stability test with alink connected at ${model} succeed, log:\n" >> ${logfile}
        else
            echo -e "run stability test with alink connected at ${model} failed, log:\n" >> ${logfile}
        fi
        cat ~/${model}_stability_alink.txt >> ${logfile}
        rm -f ~/${model}_stability_alink.txt
    fi

    if [[ ${performance_tests} == *stability_noalink* ]]; then
        wait ${esp32_stability_noalink_pid}
        esp32_stability_noalink_ret=$?
        echo -e "\n---------------------------------------------------------\n" >> ${logfile}
        if [ ${esp32_stability_noalink_ret} -eq 0 ]; then
            echo -e "run stability test with alink disconnected at ${model} succeed, log:\n" >> ${logfile}
        else
            echo -e "run stability test with alink disconnected at ${model} failed, log:\n" >> ${logfile}
        fi
        cat ~/${model}_stability_noalink.txt >> ${logfile}
        rm -f ~/${model}_stability_noalink.txt
    fi
    title="${title};"
fi

#send email
title="Autotest: MESH: PASS-${mesh_pass_total} FAIL-${mesh_fail_total};${title}"
cat ${logfile} | mutt -s "${title}" -- ${recipients}
rm -f ${logfile}
