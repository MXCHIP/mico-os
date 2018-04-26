/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "yunit.h"

#include "aos/aos.h"
#include "aos/kernel.h"

#include "umesh.h"
#include "core/link_mgmt.h"
#include "core/sid_allocator.h"
#include "core/router_mgr.h"
#include "utilities/logging.h"
#include "tools/cli.h"
#include "ip/lwip_adapter.h"

#include "dda_util.h"

static void leader_discover_case(void)
{
    char ping_cmd[64];

    set_full_rssi(11, 15);
    cmd_to_agent("stop");
    cmd_to_agent("mode FIXED");
    cmd_to_agent("mode LEADER");

    cmd_to_agent("start");
    check_cond_wait((DEVICE_STATE_LEADER == umesh_mm_get_device_state()), 15);

    start_node_ext(12, MODE_RX_ON, -1, -1);
    check_p2p_str_wait("router", 12, "testcmd state", 10);

    start_node_ext(13, MODE_RX_ON, -1, -1);
    check_p2p_str_wait("router", 13, "testcmd state", 10);

    start_node_ext(14, MODE_RX_ON, -1, -1);
    check_p2p_str_wait("router", 14, "testcmd state", 10);

    start_node_ext(15, MODE_RX_ON, -1, -1);
    check_p2p_str_wait("router", 15, "testcmd state", 10);

    const ip6_addr_t *myaddr;
    myaddr = ur_adapter_get_mcast_ipaddr();
    snprintf(ping_cmd, sizeof ping_cmd, "send 12 autotest " IP6_ADDR_FMT, IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("4", 12, "testcmd autotest_acked", 10);

    myaddr = ur_adapter_get_default_ipaddr();

    snprintf(ping_cmd, sizeof ping_cmd, "send 12 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 12, "testcmd autotest_acked", 10);

    snprintf(ping_cmd, sizeof ping_cmd, "send 13 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 13, "testcmd autotest_acked", 10);

    snprintf(ping_cmd, sizeof ping_cmd, "send 14 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 14, "testcmd autotest_acked", 10);

    snprintf(ping_cmd, sizeof ping_cmd, "send 15 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 15, "testcmd autotest_acked", 10);

    cmd_to_agent("stop");
    aos_msleep(120 * 1000);

    char *state;
    uint8_t index;
    for (index = 12; index < 16; index++) {
        state = dda_p2p_req_and_wait(12, "testcmd state", 5);
        if (strcmp(state, "leader")) {
            aos_free(state);
            break;
        }
        aos_free(state);
    }
    YUNIT_ASSERT(index != 16);

    char *ipaddr;
    ipaddr = dda_p2p_req_and_wait(12, "testcmd ipaddr", 5);
    YUNIT_ASSERT(ipaddr != NULL);

    snprintf(ping_cmd, sizeof ping_cmd, "send 13 ping %s", ipaddr);
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 13, "testcmd icmp_acked", 5);

    snprintf(ping_cmd, sizeof ping_cmd, "send 14 ping %s", ipaddr);
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 14, "testcmd icmp_acked", 5);

    snprintf(ping_cmd, sizeof ping_cmd, "send 15 ping %s", ipaddr);
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 15, "testcmd icmp_acked", 5);
    aos_free(ipaddr);

    hal_umesh_set_channel(NULL, 6);
    cmd_to_agent("start");
    aos_msleep(120 * 1000);

    check_p2p_str_wait("router", 12, "testcmd state", 10);
    check_p2p_str_wait("router", 13, "testcmd state", 10);
    check_p2p_str_wait("router", 14, "testcmd state", 10);
    check_p2p_str_wait("router", 15, "testcmd state", 10);

    snprintf(ping_cmd, sizeof ping_cmd, "send 12 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 12, "testcmd autotest_acked", 10);

    snprintf(ping_cmd, sizeof ping_cmd, "send 13 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 13, "testcmd autotest_acked", 10);

    snprintf(ping_cmd, sizeof ping_cmd, "send 14 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 14, "testcmd autotest_acked", 10);

    snprintf(ping_cmd, sizeof ping_cmd, "send 15 autotest " IP6_ADDR_FMT " 1 1200", IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 15, "testcmd autotest_acked", 10);

    cmd_to_agent("stop");
    hal_umesh_set_channel(NULL, 1);
    stop_node(12);
    stop_node(13);
    stop_node(14);
    stop_node(15);
}

void test_leader_discover_case(void)
{
    int32_t num;
    const ur_mem_stats_t *mem_stats = ur_mem_get_stats();
    num = mem_stats->num;

    leader_discover_case();

    aos_msleep(500);
    mem_stats = ur_mem_get_stats();
    YUNIT_ASSERT(num == mem_stats->num);
}
