/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "umesh.h"
#include "umesh_utils.h"
#include "core/mesh_mgmt.h"
#include "core/mesh_mgmt_tlvs.h"
#include "core/lowpower_mgmt.h"
#include "utilities/logging.h"
#include "hal/interfaces.h"

typedef struct lowpower_state_s {
    bool up;
    uint8_t slot_num;  // indicates current wakeup time slot
    uint8_t next_slot_num; // indicates next wakeup time slot
    uint32_t timestamp; // indicates goto sleep timestamp

    ur_timer_t wakeup_timer;  // node's schedule mgmt
    ur_timer_t sleep_timer;
    schedule_type_t schedule_type;

    neighbor_t *attach_node;
    ur_timer_t parent_wakeup_timer;  // parent's schedule mgmt
    ur_timer_t parent_sleep_timer;
    time_slot_t parent_time_slot;
    int16_t parent_bufqueue_size;

    mm_cb_t interface_callback;
    slist_t lowpower_callback;

    uint32_t sleep_start_timestamp;
    uint32_t sleep_time;
} lowpower_state_t;
lowpower_state_t g_lowpower_state;

static void handle_parent_wakeup_timer(void *args);
static void handle_wakeup_timer(void *args);
static void wakeup_timer_handler(schedule_type_t type, uint32_t wakeup_interval);

static uint8_t get_next_slot_num(const uint8_t *mac, uint8_t slot_num)
{
    uint8_t next_slot_num = 1;
    struct key_t {
        uint8_t mac[EXT_ADDR_SIZE];
        uint32_t slot_num;
    } key;

    memcpy(key.mac, mac, EXT_ADDR_SIZE);
    key.slot_num = (uint32_t)slot_num;
    next_slot_num =
             (umesh_get_hashword((uint32_t *)&key, sizeof(key) / 4, 0) % SCHEDULE_SLOTS_SIZE) + 1;
    return next_slot_num;
}

static void radio_events_handler(schedule_type_t type, bool is_up)
{
    lowpower_events_handler_t *callback;
    uint8_t event_type = 0;

    if (is_up == false) {
        event_type = CODE_MESH_SCHED_DOWN;
    } else if (type == LOWPOWER_PARENT_SCHEDULE) {
        event_type = CODE_MESH_PSCHED_UP;
    } else if (type == LOWPOWER_ATTACHED_SCHEDULE) {
        event_type = CODE_MESH_ASCHED_UP;
    }
    umesh_post_event(event_type, 0);

    if (umesh_get_mode() & MODE_RX_ON) {
        return;
    }

    slist_for_each_entry(&g_lowpower_state.lowpower_callback, callback,
                         lowpower_events_handler_t, next) {
        is_up? callback->radio_up(type): callback->radio_down(type);
    }
}

static void update_schedule_timer(schedule_type_t type)
{
    uint8_t slot;
    uint8_t next_slot;
    int32_t offset;
    const uint8_t *mac;

    if (type == LOWPOWER_ATTACHING_SCHEDULE || type == LOWPOWER_ATTACHED_SCHEDULE) {
        mac = (umesh_get_mac_address(MEDIA_TYPE_DFL))->addr;
        g_lowpower_state.slot_num = g_lowpower_state.next_slot_num;
        next_slot = get_next_slot_num(mac, g_lowpower_state.slot_num);
        offset =
           (next_slot + SCHEDULE_SLOTS_SIZE - g_lowpower_state.slot_num) * SCHEDULE_SLOT_INTERVAL;
        ur_start_timer(&g_lowpower_state.wakeup_timer, offset, handle_wakeup_timer, NULL);
        g_lowpower_state.next_slot_num = next_slot;
        g_lowpower_state.timestamp = umesh_now_ms();
        g_lowpower_state.schedule_type = type;
        MESH_LOG_DEBUG("wakeup at own schedule, slot %d, at %d",
                        g_lowpower_state.next_slot_num, g_lowpower_state.timestamp);
    } else if (g_lowpower_state.attach_node) {
        slot = g_lowpower_state.parent_time_slot.slot_num;
        next_slot = get_next_slot_num(g_lowpower_state.attach_node->mac, slot);
        offset = (next_slot + SCHEDULE_SLOTS_SIZE - slot) * SCHEDULE_SLOT_INTERVAL;
        offset -= g_lowpower_state.parent_time_slot.offset;
        if (offset < 0) {
            wakeup_timer_handler(LOWPOWER_PARENT_SCHEDULE, SCHEDULE_SLOT_INTERVAL + offset);
        } else {
            ur_start_timer(&g_lowpower_state.parent_wakeup_timer, offset,
                           handle_parent_wakeup_timer, NULL);
        }
        g_lowpower_state.parent_time_slot.slot_num = next_slot;
        g_lowpower_state.parent_time_slot.offset = 0;
        MESH_LOG_DEBUG("wakeup at parent schedule, slot %d, at %d",
                        g_lowpower_state.parent_time_slot.slot_num, umesh_now_ms() + offset);
    }
}

static void set_child_state_sleep(void)
{
    slist_t *nbrs;
    neighbor_t *nbr;
    slist_t *hals;
    hal_context_t *hal;

    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal, hal_context_t, next) {
        nbrs = umesh_get_nbrs(hal->module->type);
        slist_for_each_entry(nbrs, nbr, neighbor_t, next) {
            if (nbr->state == STATE_CHILD && (nbr->mode & MODE_RX_ON) == 0) {
                nbr->flags &= (~NBR_WAKEUP);
            }
        }
    }
}

static void radio_sleep(void)
{
    g_lowpower_state.up = false;
    g_lowpower_state.sleep_start_timestamp = umesh_now_ms();
    umesh_pal_radio_sleep();
}

static void radio_wakeup(void)
{
    if (g_lowpower_state.up == false) {
        g_lowpower_state.up = true;
        g_lowpower_state.sleep_time += (umesh_now_ms() - g_lowpower_state.sleep_start_timestamp);
        umesh_pal_radio_wakeup();
    }
}

static void sleep_timer_handler(schedule_type_t type)
{
    if (type == LOWPOWER_ATTACHED_SCHEDULE) {
        set_child_state_sleep();
    }
    update_schedule_timer(type);
    if (type != LOWPOWER_ATTACHING_SCHEDULE &&
        g_lowpower_state.sleep_timer == NULL && g_lowpower_state.parent_sleep_timer == NULL) {
        radio_events_handler(type, false);
        if ((umesh_get_mode() & MODE_RX_ON) == 0) {
            radio_sleep();
            MESH_LOG_DEBUG("radio goto sleep at %d, type %d", umesh_now_ms(), type);
        }
    }
}

static void handle_parent_sleep_timer(void *args)
{
    g_lowpower_state.parent_sleep_timer = NULL;
    sleep_timer_handler(LOWPOWER_PARENT_SCHEDULE);
}

static void handle_sleep_timer(void *args)
{
    g_lowpower_state.sleep_timer = NULL;
    sleep_timer_handler(LOWPOWER_ATTACHED_SCHEDULE);
}

static void wakeup_timer_handler(schedule_type_t type, uint32_t wakeup_interval)
{
    radio_events_handler(type, true);
    if ((umesh_get_mode() & MODE_RX_ON) == 0) {
        MESH_LOG_DEBUG("radio wakeup at %d", umesh_now_ms());
        radio_wakeup();
    }

    if (type == LOWPOWER_ATTACHED_SCHEDULE) {
        ur_start_timer(&g_lowpower_state.sleep_timer, wakeup_interval, handle_sleep_timer, NULL);
    } else if (g_lowpower_state.attach_node && type == LOWPOWER_PARENT_SCHEDULE) {
        ur_start_timer(&g_lowpower_state.parent_sleep_timer, wakeup_interval,
                       handle_parent_sleep_timer, NULL);
    }
}

static void handle_parent_wakeup_timer(void *args)
{
    g_lowpower_state.parent_wakeup_timer = NULL;
    wakeup_timer_handler(LOWPOWER_PARENT_SCHEDULE, SCHEDULE_SLOT_INTERVAL);
}

static void handle_wakeup_timer(void *args)
{
    g_lowpower_state.wakeup_timer = NULL;
    wakeup_timer_handler(g_lowpower_state.schedule_type, SCHEDULE_SLOT_INTERVAL);
}

static void stop_timers(void)
{
    ur_stop_timer(&g_lowpower_state.parent_wakeup_timer, NULL);
    ur_stop_timer(&g_lowpower_state.wakeup_timer, NULL);
    ur_stop_timer(&g_lowpower_state.parent_sleep_timer, NULL);
    ur_stop_timer(&g_lowpower_state.sleep_timer, NULL);
}

static ur_error_t mesh_interface_up(void)
{
    uint8_t state = umesh_get_device_state();

    MESH_LOG_DEBUG("lowpower mgmt interface up handler");

    stop_timers();
    g_lowpower_state.attach_node = umesh_mm_get_attach_node();
    if ((umesh_get_mode() & MODE_RX_ON) == 0) {
        radio_sleep();
        if (state != DEVICE_STATE_LEADER && state != DEVICE_STATE_SUPER_ROUTER) {
            update_schedule_timer(LOWPOWER_PARENT_SCHEDULE);
        }
    }
    g_lowpower_state.next_slot_num = (((uint8_t)umesh_get_random()) % SCHEDULE_SLOTS_SIZE + 1);
    g_lowpower_state.timestamp = umesh_now_ms();
    update_schedule_timer(LOWPOWER_ATTACHED_SCHEDULE);
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_down(interface_state_t state)
{
    MESH_LOG_DEBUG("lowpower mgmt interface down handler, reason %d", state);
    stop_timers();
    g_lowpower_state.attach_node = NULL;
    g_lowpower_state.schedule_type = LOWPOWER_NONE_SCHEDULE;
    g_lowpower_state.up = true;

    if ((umesh_get_mode() & MODE_RX_ON) == 0) {
        g_lowpower_state.up = false;
        if (state == INTERFACE_DOWN_MESH_START) {
            g_lowpower_state.sleep_time = 0;
            g_lowpower_state.sleep_start_timestamp = umesh_now_ms();
            radio_wakeup();
        } else {
            radio_sleep();
        }
        if (state != INTERFACE_DOWN_MESH_STOP) {
            g_lowpower_state.slot_num = (((uint8_t)umesh_get_random()) % SCHEDULE_SLOTS_SIZE + 1);
            g_lowpower_state.timestamp = umesh_now_ms();
            update_schedule_timer(LOWPOWER_ATTACHING_SCHEDULE);
        }
    }
    return UR_ERROR_NONE;
}

void lowpower_init(void)
{
    g_lowpower_state.up = (umesh_get_mode() & MODE_RX_ON)? true: false;
    g_lowpower_state.interface_callback.interface_up = mesh_interface_up;
    g_lowpower_state.interface_callback.interface_down = mesh_interface_down;
    g_lowpower_state.sleep_time = 0;
    umesh_mm_register_callback(&g_lowpower_state.interface_callback);
    g_lowpower_state.schedule_type = LOWPOWER_NONE_SCHEDULE;
}

void lowpower_update_info(neighbor_t *nbr, uint8_t *tlvs, uint16_t length)
{
    mm_time_slot_tv_t *time_slot;
    mm_bufqueue_size_tv_t *bufqueue_size;

    time_slot = (mm_time_slot_tv_t *)umesh_mm_get_tv(tlvs, length, TYPE_TIME_SLOT);
    bufqueue_size = (mm_bufqueue_size_tv_t *)umesh_mm_get_tv(tlvs, length, TYPE_BUFQUEUE_SIZE);
    if (time_slot) {
        g_lowpower_state.parent_time_slot.slot_num = time_slot->slot_num;
        g_lowpower_state.parent_time_slot.offset = time_slot->offset;
    }
    if (bufqueue_size) {
        g_lowpower_state.parent_bufqueue_size = bufqueue_size->size;
        if (g_lowpower_state.parent_bufqueue_size == 0) {
            sleep_timer_handler(LOWPOWER_PARENT_SCHEDULE);
        }
    }
}

uint16_t lowpower_set_info(uint8_t type, uint8_t *data, void *context)
{
    uint16_t size = 0;
    mm_time_slot_tv_t *time_slot;
    neighbor_t *nbr = (neighbor_t *)context;
    mm_bufqueue_size_tv_t *bufqueue_size;

    switch (type) {
        case TYPE_TIME_SLOT:
            time_slot = (mm_time_slot_tv_t *)data;
            umesh_mm_init_tv_base((mm_tv_t *)time_slot, TYPE_TIME_SLOT);
            time_slot->slot_num = g_lowpower_state.slot_num;
            time_slot->offset = umesh_now_ms() - g_lowpower_state.timestamp;
            size = sizeof(mm_time_slot_tv_t);
            break;
        case TYPE_BUFQUEUE_SIZE:
            bufqueue_size = (mm_bufqueue_size_tv_t *)data;
            umesh_mm_init_tv_base((mm_tv_t *)bufqueue_size, TYPE_BUFQUEUE_SIZE);
            bufqueue_size->size = dlist_entry_number(&nbr->buffer_queue);
            size = sizeof(mm_bufqueue_size_tv_t);
            break;
        default:
            assert(0);
    }

    return size;
}

void lowpower_register_callback(lowpower_events_handler_t *handler)
{
    slist_add(&handler->next, &g_lowpower_state.lowpower_callback);
}

bool lowpower_is_radio_up(void)
{
    return g_lowpower_state.up;
}

uint32_t lowpower_get_sleep_time(void)
{
    return g_lowpower_state.sleep_time;
}
