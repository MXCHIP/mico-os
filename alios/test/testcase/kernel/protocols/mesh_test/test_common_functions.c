/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "yunit.h"

#include "umesh.h"
#include "aos/aos.h"
#include "aos/kernel.h"
#include "core/mesh_mgmt.h"
#include "ip/lwip_adapter.h"
#include "lwip/ip.h"

#include "dda_util.h"

extern void ur_ut_send_cmd_to_ddm(const char *cmd);
void topo_test_function(uint16_t first_node, uint16_t num, uint32_t timeout)
{
    uint16_t index;
    char number[5];
    int32_t mem_num;
    const ur_mem_stats_t *mem_stats = ur_mem_get_stats();

    aos_msleep(2 * 1000);
    set_full_rssi(first_node, first_node + num - 1);

    mem_num = mem_stats->num;

    for (index = 1; index < num; index++) {
        start_node_ext(index + first_node, MODE_RX_ON, -1, -1);
    }
    cmd_to_agent("stop");
    cmd_to_agent("mode RX_ON");
    cmd_to_agent("start");
    check_cond_wait(num == umesh_mm_get_meshnetsize(), timeout);

    ur_ut_send_cmd_to_ddm("sendall sids");
    aos_msleep(10 * 1000);

    check_cond_wait(num == umesh_mm_get_meshnetsize(), 2000);

    for (index = 1; index < num; index++) {
        stop_node(index + first_node);
    }
    cmd_to_agent("stop");
    aos_msleep(1000);
    mem_stats = ur_mem_get_stats();
    YUNIT_ASSERT(mem_num == mem_stats->num);
}
