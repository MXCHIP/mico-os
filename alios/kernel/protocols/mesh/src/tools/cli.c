/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "umesh.h"
#include "umesh_hal.h"
#include "umesh_utils.h"
#include "core/mesh_mgmt.h"
#include "core/sid_allocator.h"
#include "core/link_mgmt.h"
#include "core/router_mgr.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "hal/interfaces.h"
#include "tools/cli.h"

static void process_help(int argc, char *argv[]);
#ifdef CONFIG_AOS_MESH_DEBUG
static void process_channel(int argc, char *argv[]);
static void process_init(int argc, char *argv[]);
static void process_loglevel(int argc, char *argv[]);
static void process_macaddr(int argc, char *argv[]);
static void process_meshnetid(int argc, char *argv[]);
static void process_meshnetsize(int argc, char *argv[]);
static void process_mode(int argc, char *argv[]);
static void process_networks(int argc, char *argv[]);
static void process_router(int argc, char *argv[]);
static void process_state(int argc, char *argv[]);
static void process_sids(int argc, char *argv[]);
#endif

static void process_extnetid(int argc, char *argv[]);
static void process_nbrs(int argc, char *argv[]);
static void process_start(int argc, char *argv[]);
static void process_stats(int argc, char *argv[]);
static void process_status(int argc, char *argv[]);
static void process_stop(int argc, char *argv[]);

#ifdef CONFIG_NET_LWIP
#ifdef CONFIG_AOS_MESH_DEBUG
extern void process_autotest(int argc, char *argv[]);
extern void process_testcmd(int argc, char *argv[]);
extern void process_traceroute(int argc, char *argv[]);
#endif
extern void process_ipaddr(int argc, char *argv[]);
extern void process_ping(int argc, char *argv[]);
#endif
static void process_whitelist(int argc, char *argv[]);

void response_append(const char *format, ...);
const char *routerid2str(uint8_t id);

const cli_command_t g_commands[] = {
    { "help", &process_help },
#ifdef CONFIG_AOS_MESH_DEBUG
    { "channel", &process_channel },
    { "init", &process_init },
    { "loglevel", &process_loglevel },
    { "macaddr", &process_macaddr },
    { "meshnetid", &process_meshnetid },
    { "meshnetsize", &process_meshnetsize },
    { "mode", &process_mode },
    { "networks", &process_networks },
    { "router", &process_router },
    { "state", &process_state },
    { "sids", &process_sids },
#endif
    { "extnetid", &process_extnetid },
    { "nbrs", &process_nbrs },
    { "start", &process_start },
    { "stats", &process_stats },
    { "status", &process_status },
    { "stop", &process_stop },

#ifdef CONFIG_NET_LWIP
#ifdef CONFIG_AOS_MESH_DEBUG
    { "autotest", &process_autotest },
    { "testcmd", &process_testcmd },
    { "traceroute", &process_traceroute },
#endif
    { "ipaddr", &process_ipaddr },
    { "ping", &process_ping },
#endif
    { "whitelist", &process_whitelist },
};

enum {
    MAX_ARGS_NUM = 6,
    RESP_BUF_LEN = 256,
};

typedef struct input_cli_s {
    char *data;
    int length;
    cmd_cb_t cb;
    void *priv;
} input_cli_t;
static cmd_cb_t g_cur_cmd_cb;
static void *g_cur_cmd_priv;

const char *state2str(node_state_t state)
{
    switch (state) {
        case DEVICE_STATE_DISABLED:
            return "disabled";
        case DEVICE_STATE_DETACHED:
            return "detached";
        case DEVICE_STATE_ATTACHED:
            return "attached";
        case DEVICE_STATE_LEAF:
            return "leaf";
        case DEVICE_STATE_LEADER:
            return "leader";
        case DEVICE_STATE_SUPER_ROUTER:
            return "super_router";
        case DEVICE_STATE_ROUTER:
            return "router";
        default:
            return "unknown";
    }
}

static const char *attachstate2str(attach_state_t state)
{
    switch (state) {
        case ATTACH_IDLE:
            return "idle";
        case ATTACH_REQUEST:
            return "attaching";
        case ATTACH_SID_REQUEST:
            return "sid";
        case ATTACH_DONE:
            return "done";
    }
    return "unknown";
}

static const char *mediatype2str(media_type_t media)
{
    switch (media) {
        case MEDIA_TYPE_WIFI:
            return "wifi";
        case MEDIA_TYPE_BLE:
            return "ble";
        case MEDIA_TYPE_15_4:
            return "15.4";
        default:
            return "unknown";
    }
}

static const char *nbrstate2str(neighbor_state_t state)
{
    switch (state) {
        case STATE_CANDIDATE:
            return "candidate";
        case STATE_PARENT:
            return "parent";
        case STATE_CHILD:
            return "child";
        case STATE_NEIGHBOR:
            return "nbr";
        case STATE_INVALID:
            return "invalid";
    }
    return "unknown";
}

static void get_channel(channel_t *channel)
{
    umesh_hal_module_t *ur_wifi_hal = NULL;

    if (channel) {
        ur_wifi_hal = hal_umesh_get_default_module();

        channel->wifi_channel = (uint16_t)hal_umesh_get_channel(ur_wifi_hal);
        channel->channel = channel->wifi_channel;
        channel->hal_ucast_channel = (uint16_t)hal_umesh_get_channel(ur_wifi_hal);
        channel->hal_bcast_channel = (uint16_t)hal_umesh_get_channel(ur_wifi_hal);
    }
}

void process_help(int argc, char *argv[])
{
    uint16_t index;

    for (index = 0; index < sizeof(g_commands) / sizeof(g_commands[0]); ++index) {
        response_append("%s\r\n", g_commands[index].name);
    }
}

#ifdef CONFIG_AOS_MESH_DEBUG
static int hex2bin(const char *hex, uint8_t *bin, uint16_t bin_length);

void process_channel(int argc, char *argv[])
{
    channel_t channel;

    get_channel(&channel);
    response_append("%d\r\n", channel.channel);
    response_append("wifi %d\r\n", channel.wifi_channel);
    response_append("hal ucast %d\r\n", channel.hal_ucast_channel);
    response_append("hal bcast %d\r\n", channel.hal_bcast_channel);
    response_append("done\r\n");
}

void process_init(int argc, char *argv[])
{
    umesh_init(MODE_RX_ON);
    response_append("done\r\n");
}

void process_loglevel(int argc, char *argv[])
{
    if (argc > 0) {
        ur_log_set_level(str2lvl(argv[0]));
    }

    response_append("current: %s\r\n", lvl2str(ur_log_get_level()));
}

void process_networks(int argc, char *argv[])
{
    slist_t *networks;
    network_context_t *network;

    networks = get_network_contexts();
    slist_for_each_entry(networks, network, network_context_t, next) {
        response_append("index %d\r\n", network->index);
        response_append("  hal %d\r\n", network->hal->module->type);
        response_append("  state %s\r\n", network->state == 0 ? "up" : "down");
        response_append("  meshnetid %x\r\n", network->meshnetid);
        response_append("  sid type %d\r\n", network->router->sid_type);
        response_append("  route id %d\r\n", network->router->id);
        response_append("  netdata version %d\r\n", network->network_data.version);
        response_append("  size %d\r\n", network->network_data.size);
        response_append("  channel %d\r\n", network->hal->channel);
    }
}

void process_macaddr(int argc, char *argv[])
{
    const mac_address_t *addr = umesh_get_mac_address(MEDIA_TYPE_DFL);
    if (addr) {
        response_append("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                        addr->addr[0], addr->addr[1], addr->addr[2], addr->addr[3],
                        addr->addr[4], addr->addr[5], addr->addr[6], addr->addr[7]);
    }
    response_append("done\r\n");
}

void process_meshnetid(int argc, char *argv[])
{
    response_append("0x%x\r\n", umesh_get_meshnetid());
}

void process_meshnetsize(int argc, char *argv[])
{
    response_append("%d\r\n", umesh_mm_get_meshnetsize());
}

void process_mode(int argc, char *argv[])
{
    uint8_t mode, index;
    ur_error_t error;
    char modestr[64];

    mode = umesh_get_mode();
    modestr[0] = '\x0';

    for (index = 0; index < argc; index++) {
        if (strcmp(argv[index], "none") == 0) {
            mode = 0;
            break;
        }

        if (strcmp(argv[index], "LEADER") == 0) {
            mode |= MODE_LEADER;
            continue;
        }

#ifdef CONFIG_AOS_MESH_SUPER
        if (strcmp(argv[index], "SUPER") == 0) {
            mode |= MODE_SUPER;
            continue;
        }
#endif

        if (strcmp(argv[index], "ROUTER") == 0) {
            mode &= ~MODE_SUPER;
            continue;
        }

        if (strcmp(argv[index], "RX_ON") == 0) {
            mode |= MODE_RX_ON;
            continue;
        }

        if (strcmp(argv[index], "RX_OFF") == 0) {
            mode &= ~MODE_RX_ON;
            continue;
        }

        if (strcmp(argv[index], "MOBILE") == 0) {
            mode |= MODE_MOBILE;
            continue;
        }
        if (strcmp(argv[index], "FIXED") == 0) {
            mode &= ~MODE_MOBILE;
            continue;
        }

        /* or specify number */
        mode = atoi(argv[index]);
    }
    error = umesh_set_mode(mode);

    if (error != UR_ERROR_NONE) {
        response_append("none\r\n");
        return;
    }

    if (mode & MODE_LEADER) {
        strcat(modestr, "LEADER |");
    }
    if (mode & MODE_SUPER) {
        strcat(modestr, "SUPER");
    } else {
        strcat(modestr, "NORMAL");
    }

    if (mode & MODE_RX_ON) {
        strcat(modestr, " | RX_ON");
    } else {
        strcat(modestr, " | RX_OFF");
    }

    if (mode & MODE_MOBILE) {
        strcat(modestr, " | MOBILE");
    } else {
        strcat(modestr, " | FIXED");
    }

    response_append("%s\r\n", modestr);
}

void process_router(int argc, char *argv[])
{
    uint8_t id = 0, index;
    ur_error_t error = UR_ERROR_NONE;

    for (index = 0; index < argc; index++) {
        if (strcmp(argv[index], "SID_ROUTER") == 0) {
            id = SID_ROUTER;
            continue;
        }

        if (strcmp(argv[index], "VECTOR_ROUTER") == 0) {
            id = VECTOR_ROUTER;
            continue;
        }

        /* or specify number */
        id = atoi(argv[index]);
    }

    if (id == 0) {
        response_append("%s\r\n", routerid2str(ur_router_get_default_router()));
    } else {
        if (id != ur_router_get_default_router()) {
            error = ur_router_set_default_router(id);
        }

        response_append("switch to %s %s\r\n", routerid2str(id),
                        error == UR_ERROR_NONE ? "successfully" : "failed");
    }
}

void process_state(int argc, char *argv[])
{
    node_state_t state;

    state = umesh_get_device_state();

    response_append("%s\r\n", state2str(state));
}

void process_sids(int argc, char *argv[])
{
    sid_node_t *node;
    slist_t *nodes_list;
    slist_t *networks;
    network_context_t *network;

    response_append("me=%04x\r\n", umesh_get_sid());
    networks = get_network_contexts();
    slist_for_each_entry(networks, network, network_context_t, next) {
        nodes_list = get_ssid_nodes_list(network->sid_base);
        if (nodes_list == NULL) {
            continue;
        }
        slist_for_each_entry(nodes_list, node, sid_node_t, next) {
            response_append(EXT_ADDR_FMT ", %04x\r\n",
                            EXT_ADDR_DATA(node->node_id.uuid), node->node_id.sid);
        }
    }
}
#endif

static int hex2bin(const char *hex, uint8_t *bin, uint16_t bin_length)
{
    uint16_t hex_length = strlen(hex);
    const char *hex_end = hex + hex_length;
    uint8_t *cur = bin;
    uint8_t num_chars = hex_length & 1;
    uint8_t byte = 0;

    if ((hex_length + 1) / 2 > bin_length) {
        return -1;
    }

    while (hex < hex_end) {
        if ('A' <= *hex && *hex <= 'F') {
            byte |= 10 + (*hex - 'A');
        } else if ('a' <= *hex && *hex <= 'f') {
            byte |= 10 + (*hex - 'a');
        } else if ('0' <= *hex && *hex <= '9') {
            byte |= *hex - '0';
        } else {
            return -1;
        }
        hex++;
        num_chars++;

        if (num_chars >= 2) {
            num_chars = 0;
            *cur++ = byte;
            byte = 0;
        } else {
            byte <<= 4;
        }
    }
    return cur - bin;
}

void response_append(const char *format, ...)
{
    va_list list;
    char res_buf[CMD_LINE_SIZE];
    uint16_t len;

    va_start(list, format);
    len = vsnprintf(res_buf, sizeof(res_buf) - 1, format, list);
    va_end(list);
    if (len >= sizeof(res_buf)) {
        len = sizeof(res_buf) - 1;
        res_buf[len] = 0;
    }

    if (g_cur_cmd_cb) {
        g_cur_cmd_cb(res_buf, len, g_cur_cmd_priv);
    } else {
#ifdef CONFIG_AOS_DDA
        extern int dda_cli_log(char *str);
        dda_cli_log((char *)res_buf);
#endif
        if (!g_cli_silent) {
            umesh_log("%s", res_buf);
        }
    }
}

void process_extnetid(int argc, char *argv[])
{
    umesh_extnetid_t extnetid;
    uint8_t length;

    if (argc > 0) {
        length = hex2bin(argv[0], extnetid.netid, 6);
        if (length != 6) {
            return;
        }
        extnetid.len = length;
        umesh_set_extnetid(&extnetid);
        umesh_kv_set("extnetid", extnetid.netid, extnetid.len, 1);
    }

    memset(&extnetid, 0, sizeof(extnetid));
    umesh_get_extnetid(&extnetid);
    if (extnetid.len >= 6) {
        response_append(EXT_NETID_FMT "\r\n", EXT_NETID_DATA(extnetid.netid));
    }
    response_append("done\r\n");
}

void process_nbrs(int argc, char *argv[])
{
    neighbor_t *nbr;
    slist_t *hals;
    hal_context_t *hal;
    slist_t *nbrs;

    response_append("neighbors:\r\n");
    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal, hal_context_t, next) {
        uint16_t num = 0;
        response_append("\t<<hal type %s>>\r\n", mediatype2str(hal->module->type));
        nbrs = umesh_get_nbrs(hal->module->type);
        slist_for_each_entry(nbrs, nbr, neighbor_t, next) {
            response_append("\t" EXT_ADDR_FMT ",%s,0x%04x,0x%04x,%d,%d,%d,%d,%d,%d,%d\r\n", \
                            EXT_ADDR_DATA(nbr->mac), nbrstate2str(nbr->state), \
                            nbr->netid, nbr->sid, nbr->stats.link_cost, nbr->ssid_info.child_num, \
                            nbr->channel, nbr->stats.reverse_rssi, nbr->stats.forward_rssi, \
                            nbr->last_heard, (nbr->flags & NBR_WAKEUP)? 1: 0);
            num++;
        }
        response_append("\tnum=%d\r\n", num);
    }
}

const char *routerid2str(uint8_t id)
{
    switch (id) {
        case SID_ROUTER:
            return "SID_ROUTER";
            break;
        case VECTOR_ROUTER:
            return "VECTOR_ROUTER";
            break;
        default:
            return "UNKNOWN_ROUTER";
            break;
    }
}

void process_start(int argc, char *argv[])
{
    umesh_start();
    response_append("done\r\n");
}

void process_stats(int argc, char *argv[])
{
    slist_t *hals;
    hal_context_t *hal;
    const ur_message_stats_t *message_stats;
    const frame_stats_t *hal_stats;
    const ur_mem_stats_t *mem_stats;

    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal, hal_context_t, next) {
#ifdef CONFIG_AOS_MESH_DEBUG
        const ur_link_stats_t    *link_stats;
        link_stats = mf_get_stats(hal);
        if (link_stats) {
            response_append("\t<<hal type %s>>\r\n", mediatype2str(hal->module->type));
            response_append("link stats\r\n");
            response_append("  in_frames %d\r\n", link_stats->in_frames);
            response_append("  in_command %d\r\n", link_stats->in_command);
            response_append("  in_data %d\r\n", link_stats->in_data);
            response_append("  in_filterings %d\r\n", link_stats->in_filterings);
            response_append("  in_drops %d\r\n", link_stats->in_drops);
            response_append("  out_frames %d\r\n", link_stats->out_frames);
            response_append("  out_command %d\r\n", link_stats->out_command);
            response_append("  out_data %d\r\n", link_stats->out_data);
            response_append("  out_errors %d\r\n", link_stats->out_errors);
            response_append("  send_queue_size %d\r\n", link_stats->send_queue_size);
            response_append("  recv_queue_size %d\r\n", link_stats->recv_queue_size);
            response_append("  sending %s\r\n", link_stats->sending ? "true" : "false");
            response_append("  sending_timeouts %d\r\n", link_stats->sending_timeouts);
        }
#endif

        hal_stats = hal_umesh_get_stats(hal->module);
        if (hal_stats) {
            response_append("\t<<hal type %s>>\r\n", mediatype2str(hal->module->type));
            response_append("hal stats\r\n");
            response_append("  in_frames %d\r\n", hal_stats->in_frames);
            response_append("  out_frames %d\r\n", hal_stats->out_frames);
        }
    }

    message_stats = message_get_stats();
    if (message_stats) {
        response_append("message stats\r\n");
        response_append("  nums %d\r\n", message_stats->num);
        response_append("  queue_fulls %d\r\n", message_stats->queue_fulls);
        response_append("  mem_fails %d\r\n", message_stats->mem_fails);
        response_append("  pbuf_fails %d\r\n", message_stats->pbuf_fails);
        response_append("  size %d\r\n", message_stats->size);

        uint8_t index;
        response_append("  msg debug info\r\n  ");
        for (index = 0; index < MSG_DEBUG_INFO_SIZE; index++) {
            response_append("%d:", message_stats->debug_info[index]);
        }
        response_append("\r\n");
    }

    mem_stats = ur_mem_get_stats();
    if (mem_stats) {
        response_append("memory stats\r\n");
        response_append("  nums %d\r\n", mem_stats->num);
    }
}

static void process_status(int argc, char *argv[])
{
    slist_t *networks;
    network_context_t *network;
    channel_t channel;

    response_append("state\t%s\r\n", state2str(umesh_get_device_state()));
    response_append("attach\t%s\r\n", attachstate2str(umesh_mm_get_attach_state()));
    networks = get_network_contexts();
    slist_for_each_entry(networks, network, network_context_t, next) {
        response_append("<<network %s %d>>\r\n",
                        mediatype2str(network->hal->module->type), network->index);
        response_append("\tnetid\t0x%x\r\n", umesh_mm_get_meshnetid(network));
        response_append("\tmac\t" EXT_ADDR_FMT "\r\n",
                        EXT_ADDR_DATA(network->hal->mac_addr.addr));
        response_append("\tsid\t%04x\r\n", umesh_mm_get_local_sid());
        response_append("\tnetsize\t%d\r\n", umesh_mm_get_meshnetsize());
        response_append("\trouter\t%s\r\n", routerid2str(network->router->id));
        response_append("\tbcast_mtu\t%d\r\n",
                        hal_umesh_get_bcast_mtu(network->hal->module));
        response_append("\tucast_mtu\t%d\r\n",
                        hal_umesh_get_ucast_mtu(network->hal->module));
    }
    response_append("uptime\t%d\r\n", umesh_now_ms());
#ifdef CONFIG_AOS_MESH_LOWPOWER
    response_append("sleetime\t%d, ratio %d\%\r\n",
            lowpower_get_sleep_time(), (lowpower_get_sleep_time() * 100) / umesh_now_ms());
#endif
    get_channel(&channel);
    response_append("channel\t%d\r\n", channel.channel);
}

void process_stop(int argc, char *argv[])
{
    umesh_stop();
    response_append("done\r\n");
}

void process_whitelist(int argc, char *argv[])
{
    uint8_t arg_index = 0;
    int length;
    mac_address_t addr;
    int8_t rssi;
    whitelist_entry_t *entry;

    if (arg_index >= argc) {
        int i = 0;
        bool enabled = is_whitelist_enabled();
        const whitelist_entry_t *whitelist = whitelist_get_entries();
        response_append("whitelist is %s, entries:\r\n", enabled ? "enabled" : "disabled");
        for (i = 0; i < WHITELIST_ENTRY_NUM; i++) {
            if (whitelist[i].valid == false) {
                continue;
            }
            response_append("%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\r\n",
                            whitelist[i].address.addr[0], whitelist[i].address.addr[1],
                            whitelist[i].address.addr[2], whitelist[i].address.addr[3],
                            whitelist[i].address.addr[4], whitelist[i].address.addr[5],
                            whitelist[i].address.addr[6], whitelist[i].address.addr[7]);
        }
        return;
    }

    if (strcmp(argv[arg_index], "add") == 0) {
        if (++arg_index >= argc) {
            return;
        }
        addr.len = 8;
        length = hex2bin(argv[arg_index], addr.addr, sizeof(addr.addr));
        if (length != sizeof(addr.addr)) {
            return;
        }
        entry = whitelist_add(&addr);
        if (++arg_index < argc && entry) {
            rssi = (int8_t)strtol(argv[arg_index], NULL, 0);
            whitelist_set_constant_rssi(entry, rssi);
        }
    } else if (strcmp(argv[arg_index], "clear") == 0) {
        whitelist_clear();
    } else if (strcmp(argv[arg_index], "disable") == 0) {
        whitelist_disable();
    } else if (strcmp(argv[arg_index], "enable") == 0) {
        whitelist_enable();
    } else if (strcmp(argv[arg_index], "remove") == 0) {
        if (++arg_index >= argc) {
            return;
        }
        addr.len = 8;
        length = hex2bin(argv[arg_index], addr.addr, sizeof(addr.addr));
        if (length != sizeof(addr.addr)) {
            return;
        }
        whitelist_remove(&addr);
    }
    response_append("done\r\n");
}

static void do_cli(void *arg)
{
    input_cli_t *buf = arg;
    char *argv[MAX_ARGS_NUM];
    char *cmd;
    int argc;
    char *last;
    uint16_t index;

    g_cur_cmd_cb = buf->cb;
    g_cur_cmd_priv = buf->priv;

    cmd = strtok_r(buf->data, " ", &last);
    if (!cmd)
        goto out;

    for (argc = 0; argc < MAX_ARGS_NUM; ++argc) {
        if ((argv[argc] = strtok_r(NULL, " ", &last)) == NULL) {
            break;
        }
    }

    if (umesh_is_initialized() == false && strcmp(cmd, "init") != 0) {
        goto out;
    }

    for (index = 0; index < sizeof(g_commands) / sizeof(g_commands[0]); index++) {
        if (strcmp(cmd, g_commands[index].name) == 0) {
            g_commands[index].function(argc, argv);
            break;
        }
    }

out:
    if (g_cur_cmd_cb) {
        g_cur_cmd_cb(NULL, 0, buf->priv);
    }
    g_cur_cmd_cb = NULL;
    g_cur_cmd_priv = NULL;

    ur_mem_free(buf->data, buf->length);
    ur_mem_free(buf, sizeof(input_cli_t));
}

void umesh_cli_cmd(char *buf, int length, cmd_cb_t cb, void *priv)
{
    input_cli_t *input_cli = NULL;

    if (length == 0) {
        goto err_out;
    }
    input_cli = (input_cli_t *)ur_mem_alloc(sizeof(input_cli_t));
    if (input_cli == NULL) {
        goto err_out;
    }

    input_cli->length = length + 1;
    input_cli->data = ur_mem_alloc(input_cli->length);
    if (input_cli->data == NULL) {
        goto err_out;
    }

    memcpy(input_cli->data, buf, input_cli->length);
    input_cli->cb = cb;
    input_cli->priv = priv;

    if (umesh_task_schedule_call(do_cli, input_cli) == 0)
        return;

err_out:
    if (cb) {
        cb(NULL, 0, priv);
    }

    if (input_cli == NULL)
        return;

    if (input_cli->data) {
        ur_mem_free(input_cli->data, input_cli->length);
    }

    ur_mem_free(input_cli, sizeof(input_cli_t));
}

int g_cli_silent;
extern void mesh_cli_ip_init(void);
ur_error_t mesh_cli_init(void)
{
#ifdef CONFIG_NET_LWIP
    mesh_cli_ip_init();
#endif
    return UR_ERROR_NONE;
}
