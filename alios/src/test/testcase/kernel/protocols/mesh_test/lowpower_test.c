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
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "utilities/logging.h"
#include "hal/hals.h"
#include "hal/interface_context.h"
#include "hal/interfaces.h"
#include "tools/cli.h"
#include "ip/lwip_adapter.h"

#include "dda_util.h"

static uint8_t g_lowpower_sched_type = LOWPOWER_NONE_SCHEDULE;

static void subscribed_event_cb(input_event_t *eventinfo, void *priv_data)
{
    if (eventinfo->type != EV_MESH) {
        return;
    }

    if (eventinfo->code == CODE_MESH_PSCHED_UP) {
        g_lowpower_sched_type = LOWPOWER_PARENT_SCHEDULE;
    } else if (eventinfo->code == CODE_MESH_ASCHED_UP) {
        g_lowpower_sched_type = LOWPOWER_ATTACHED_SCHEDULE;
    }
}

static void one_hop_multi_lowpower(void)
{
    char ping_cmd[64];
    const ip6_addr_t *myaddr;

    set_full_rssi(11, 15);

    cmd_to_agent("stop");
    cmd_to_agent("mode ROUTER");
    cmd_to_agent("mode RX_ON");
    cmd_to_agent("start");
    check_cond_wait(umesh_get_device_state() == DEVICE_STATE_LEADER, 10);

    start_node_ext(12, 0, -1, -1);
    check_p2p_str_wait("router", 12, "testcmd state", 10);

    start_node_ext(13, 0, -1, -1);
    check_p2p_str_wait("router", 13, "testcmd state", 10);

    start_node_ext(14, 0, -1, -1);
    check_p2p_str_wait("router", 12, "testcmd state", 10);

    start_node_ext(15, 0, -1, -1);
    check_p2p_str_wait("router", 13, "testcmd state", 10);

    check_cond_wait(g_lowpower_sched_type == LOWPOWER_ATTACHED_SCHEDULE, 50);
    myaddr = ur_adapter_get_default_ipaddr();

    snprintf(ping_cmd, sizeof ping_cmd, "send 12 ping " IP6_ADDR_FMT, IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 12, "testcmd icmp_acked", 30);

    snprintf(ping_cmd, sizeof ping_cmd, "send 13 ping " IP6_ADDR_FMT, IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 13, "testcmd icmp_acked", 30);

    snprintf(ping_cmd, sizeof ping_cmd, "send 14 ping " IP6_ADDR_FMT, IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 14, "testcmd icmp_acked", 30);

    snprintf(ping_cmd, sizeof ping_cmd, "send 15 ping " IP6_ADDR_FMT, IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 15, "testcmd icmp_acked", 30);
    g_lowpower_sched_type = LOWPOWER_NONE_SCHEDULE;

    cmd_to_agent("stop");
    stop_node(12);
    stop_node(13);
    stop_node(14);
    stop_node(15);
}

static void two_hops_one_lowpower(void)
{
    char ping_cmd[64];
    const ip6_addr_t *myaddr;

    set_line_rssi(11, 13);

    cmd_to_agent("stop");
    cmd_to_agent("mode ROUTER");
    cmd_to_agent("mode RX_OFF");

    start_node_ext(13, MODE_RX_ON, -1, -1);
    check_p2p_str_wait("leader", 13, "testcmd state", 10);

    start_node_ext(12, MODE_RX_ON, -1, -1);
    check_p2p_str_wait("router", 12, "testcmd state", 10);

    cmd_to_agent("start");
    check_cond_wait(umesh_get_device_state() == DEVICE_STATE_ROUTER, 10);

    check_cond_wait(g_lowpower_sched_type == LOWPOWER_PARENT_SCHEDULE, 50);
    myaddr = ur_adapter_get_default_ipaddr();

    snprintf(ping_cmd, sizeof ping_cmd, "send 13 ping " IP6_ADDR_FMT, IP6_ADDR_DATA(((ur_ip6_addr_t *)myaddr)));
    cmd_to_master(ping_cmd);
    check_p2p_str_wait("1", 13, "testcmd icmp_acked", 30);
    g_lowpower_sched_type = LOWPOWER_NONE_SCHEDULE;

    cmd_to_agent("stop");
    cmd_to_agent("mode RX_ON");
    stop_node(12);
    stop_node(13);
}

void test_umesh_lowpower_case(void)
{
    int32_t num;
    const ur_mem_stats_t *mem_stats = ur_mem_get_stats();
    num = mem_stats->num;

    g_lowpower_sched_type = LOWPOWER_NONE_SCHEDULE;
    aos_register_event_filter(EV_MESH, subscribed_event_cb, NULL);

    run_times(one_hop_multi_lowpower(), 1);
    run_times(two_hops_one_lowpower(), 1);

    aos_msleep(300);
    mem_stats = ur_mem_get_stats();
    YUNIT_ASSERT(num == mem_stats->num);
}
