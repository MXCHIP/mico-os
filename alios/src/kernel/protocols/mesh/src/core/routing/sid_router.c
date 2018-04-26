/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#include "core/router_mgr.h"
#include "core/mesh_mgmt.h"

typedef struct sid_router_state_s {
    uint16_t local_sid;
    router_t router;
} sid_router_state_t;

static sid_router_state_t g_sr_state;

static uint16_t get_common_parent(uint16_t dest)
{
    uint16_t common_parent = 0x0000;
    uint16_t mask = 0xf000;
    uint16_t sid;

    sid = g_sr_state.local_sid;
    while (mask) {
        if ((sid & mask) == (dest & mask)) {
            common_parent |= (sid & mask);
            mask >>= 4;
        } else {
            break;
        }
    }
    return common_parent;
}

uint16_t router_get_next_hop_shortid(uint16_t dest)
{
    uint16_t nxtid;
    uint16_t common_parent;
    uint16_t mask;
    uint16_t sid;

    if (is_unique_sid(g_sr_state.local_sid) == false) {
        return INVALID_SID;
    }

    sid = g_sr_state.local_sid;
    if (dest == sid) {
        return sid;
    }

    common_parent = get_common_parent(dest);
    if (common_parent == sid) {
        /* packet to parent */
        mask = 0xf000;
        nxtid = sid;
        while (mask) {
            if ((nxtid & mask) != (dest & mask)) {
                nxtid |= (dest & mask);
                break;
            }
            mask >>= 4;
        }
    } else {
        /* packet to child */
        nxtid = sid;
        mask = 0x000f;
        while (mask) {
            if ((nxtid & mask) != 0) {
                nxtid &= (~mask);
                break;
            }
            mask <<= 4;
        }
    }
    return nxtid;
}

ur_error_t sid_router_init(void)
{
    g_sr_state.local_sid = INVALID_SID;
    return UR_ERROR_NONE;
}

ur_error_t sid_router_neighbor_updated(neighbor_t *head)
{
    return UR_ERROR_NONE;
}

ur_error_t sid_router_message_received(const uint8_t *data, uint16_t length)
{
    return UR_ERROR_NONE;
}

ur_error_t sid_router_event_triggered(uint8_t event, uint8_t *data, uint8_t len)
{
    netids_t *netids;

    if (event == EVENT_SID_UPDATED) {
        netids = (netids_t *)data;
        g_sr_state.local_sid = netids->sid;
    }
    return UR_ERROR_NONE;
}

void sid_router_register(void)
{
    g_sr_state.router.id = SID_ROUTER;
    g_sr_state.router.sid_type = STRUCTURED_SID;
    g_sr_state.router.cb.start = sid_router_init;
    g_sr_state.router.cb.stop  = NULL;
    g_sr_state.router.cb.handle_neighbor_updated = sid_router_neighbor_updated;
    g_sr_state.router.cb.handle_message_received = sid_router_message_received;
    g_sr_state.router.cb.handle_subscribe_event = sid_router_event_triggered;
    g_sr_state.router.cb.get_next_hop_sid = router_get_next_hop_shortid;
    g_sr_state.router.events.events[0] = EVENT_SID_UPDATED;
    g_sr_state.router.events.num = 1;

    register_router(&g_sr_state.router);
}

