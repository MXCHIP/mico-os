/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "umesh.h"
#include "umesh_utils.h"
#include "core/mesh_mgmt.h"
#include "core/mcast.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "hal/interfaces.h"

enum {
    MCAST_CACHE_ENTRIES_SIZE = 32,
    MCAST_CACHE_ENTRY_LIFETIME = 10,
    MCAST_CACHE_LIFETIME_UNIT = 1000,
};

typedef struct mcast_entry_s {
    uint8_t subnetid;
    uint16_t sid;
    uint8_t sequence;
    uint8_t lifetime;
} mcast_entry_t;

typedef struct mcast_state_s {
    uint8_t sequence;
    ur_timer_t timer;
    mcast_entry_t entry[MCAST_CACHE_ENTRIES_SIZE];
    mm_cb_t interface_callback;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_events_handler_t lowpower_callback;
#endif
} mcast_state_t;
mcast_state_t g_mcast_state;

static void mcast_cleanup(void)
{
    g_mcast_state.sequence = 0;
    memset(g_mcast_state.entry, 0, sizeof(g_mcast_state.entry));
    ur_stop_timer(&g_mcast_state.timer, NULL);
}

ur_error_t insert_mcast_header(uint8_t *message)
{
    mcast_header_t *header = (mcast_header_t *)message;

    assert(header);
    header->dispatch = MCAST_HEADER_DISPATCH;
    header->control = 0;
    header->subnetid = get_sub_netid(umesh_get_meshnetid());
    header->sid = umesh_mm_get_local_sid();
    header->sequence = g_mcast_state.sequence++;
    return UR_ERROR_NONE;
}

static void handle_mcast_timer(void *args)
{
    bool start_timer = false;
    uint8_t index;

    g_mcast_state.timer = NULL;
    for (index = 0; index < MCAST_CACHE_ENTRIES_SIZE; index++) {
        if (g_mcast_state.entry[index].lifetime > 0) {
            g_mcast_state.entry[index].lifetime--;
            start_timer = true;
        }
    }

    if (start_timer) {
        ur_start_timer(&g_mcast_state.timer, MCAST_CACHE_LIFETIME_UNIT, handle_mcast_timer, NULL);
    }
}

ur_error_t process_mcast_header(uint8_t *message)
{
    uint16_t index;
    mcast_header_t *header = (mcast_header_t *)message;
    mcast_entry_t *entry = NULL;
    int8_t diff;
    bool from_same_sub = false;

    assert(message);
    if (header->subnetid == get_sub_netid(umesh_get_meshnetid())) {
        from_same_sub = true;
    }
    if (header->sid == umesh_mm_get_local_sid() && from_same_sub) {
        return UR_ERROR_DROP;
    }

    for (index = 0; index < MCAST_CACHE_ENTRIES_SIZE; index++) {
        if (g_mcast_state.entry[index].lifetime == 0) {
            entry = &g_mcast_state.entry[index];
        } else if (g_mcast_state.entry[index].sid == header->sid &&
                   g_mcast_state.entry[index].subnetid == header->subnetid) {
            entry = &g_mcast_state.entry[index];
            diff = header->sequence - entry->sequence;
            if (diff <= 0) {
                entry = NULL;
            }
            break;
        }
    }

    if (entry == NULL) {
        return UR_ERROR_DROP;
    }

    entry->subnetid = header->subnetid;
    entry->sid = header->sid;
    entry->sequence = header->sequence;
    entry->lifetime = MCAST_CACHE_ENTRY_LIFETIME;
    if (g_mcast_state.timer == NULL) {
        ur_start_timer(&g_mcast_state.timer, MCAST_CACHE_LIFETIME_UNIT, handle_mcast_timer, NULL);
    }
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_up(void)
{
    MESH_LOG_DEBUG("mcast mesh interface up handler");
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_down(interface_state_t state)
{
    MESH_LOG_DEBUG("mcast mesh interface down handler, reason %d", state);
    mcast_cleanup();
    return UR_ERROR_NONE;
}

#ifdef CONFIG_AOS_MESH_LOWPOWER
static void lowpower_radio_down_handler(schedule_type_t type)
{
    if (umesh_get_mode() & MODE_RX_ON) {
        return;
    }
    mcast_cleanup();
}

static void lowpower_radio_up_handler(schedule_type_t type)
{
}
#endif

void mcast_init(void)
{
#ifdef CONFIG_AOS_MESH_LOWPOWER
    g_mcast_state.lowpower_callback.radio_down = lowpower_radio_down_handler;
    g_mcast_state.lowpower_callback.radio_up = lowpower_radio_up_handler;
    lowpower_register_callback(&g_mcast_state.lowpower_callback);
#endif
    g_mcast_state.interface_callback.interface_up = mesh_interface_up;
    g_mcast_state.interface_callback.interface_down = mesh_interface_down;
    umesh_mm_register_callback(&g_mcast_state.interface_callback);
}
