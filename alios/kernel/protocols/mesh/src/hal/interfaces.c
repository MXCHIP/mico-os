/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "core/sid_allocator.h"
#include "core/router_mgr.h"
#include "core/mesh_mgmt.h"
#include "core/link_mgmt.h"
#include "umesh.h"
#include "umesh_utils.h"
#include "hal/interfaces.h"
#include "hal/interface_context.h"
#include "hal/hals.h"

AOS_SLIST_HEAD(g_networks_list);
AOS_SLIST_HEAD(g_hals_list);

static network_context_t *new_network_context(hal_context_t *hal, uint8_t index,
                                              int router_id)
{
    network_context_t *network;

    network = (network_context_t *)ur_mem_alloc(sizeof(network_context_t));
    assert(network);
    memset(network, 0, sizeof(network_context_t));
    network->index = index;
    network->hal = hal;

    network->router = ur_get_router_by_id(router_id);
    network->router->network = network;
    if (index == 0) {
        ur_router_set_default_router(router_id);
    }

    slist_add_tail(&network->next, &g_networks_list);
    return network;
}

static hal_context_t *new_hal_context(umesh_hal_module_t *module, media_type_t type)
{
    hal_context_t *hal;
    int i;
    int mtu;

    mtu = hal_umesh_get_bcast_mtu(module);
    if (mtu < 0) {
        mtu = 127;
    }
    if (hal_umesh_get_ucast_mtu(module) > mtu) {
        mtu = hal_umesh_get_ucast_mtu(module);
    }

    hal = get_hal_context(type);
    if (hal == NULL) {
        hal = (hal_context_t *)ur_mem_alloc(sizeof(hal_context_t));
        assert(hal);
        memset(hal, 0, sizeof(hal_context_t));
        slist_add_tail(&hal->next, &g_hals_list);

        for (i = 0; i < QUEUE_SIZE; i++) {
            dlist_init(&hal->send_queue[i]);
        }
        dlist_init(&hal->recv_queue);
        hal->frame.data = (uint8_t *)ur_mem_alloc(mtu);
        assert(hal->frame.data);
    }
    hal->module = module;
    slist_init(&hal->neighbors_list);
    hal->neighbors_num  = 0;

    hal->channel_list.num =
        hal_umesh_get_chnlist(module, &hal->channel_list.channels);
    memcpy(&hal->mac_addr, hal_umesh_get_mac_address(module),
           sizeof(hal->mac_addr));

    memset(hal->frame.data, 0 , mtu);
    memset(&hal->link_stats, 0, sizeof(hal->link_stats));
    hal->channel = hal_umesh_get_channel(module);

    if (type == MEDIA_TYPE_WIFI) {
        hal->def_channel = 1;
        hal->discovery_interval = WIFI_DISCOVERY_TIMEOUT;
        hal->auth_request_interval = WIFI_AUTH_REQUEST_TIMEOUT;
        hal->auth_relay_interval = WIFI_AUTH_RELAY_TIMEOUT;
        hal->auth_response_interval = WIFI_AUTH_RESPONSE_TIMEOUT;
        hal->link_quality_update_interval = (umesh_get_mode() & MODE_MOBILE)? \
                          WIFI_LINK_QUALITY_MOBILE_TIMEOUT: WIFI_LINK_QUALITY_TIMEOUT;
        hal->neighbor_alive_interval = WIFI_NEIGHBOR_ALIVE_TIMEOUT;
        hal->advertisement_interval = WIFI_ADVERTISEMENT_TIMEOUT;
    } else if (module->type == MEDIA_TYPE_BLE) {
        hal->def_channel = hal->channel_list.channels[0];
        hal->discovery_interval = BLE_DISCOVERY_TIMEOUT;
        hal->auth_request_interval = BLE_AUTH_REQUEST_TIMEOUT;
        hal->auth_relay_interval = BLE_AUTH_RELAY_TIMEOUT;
        hal->auth_response_interval = BLE_AUTH_RESPONSE_TIMEOUT;
        hal->link_quality_update_interval = (umesh_get_mode() & MODE_MOBILE)? \
                           BLE_LINK_QUALITY_MOBILE_TIMEOUT: BLE_LINK_QUALITY_TIMEOUT;
        hal->neighbor_alive_interval = BLE_NEIGHBOR_ALIVE_TIMEOUT;
        hal->advertisement_interval = BLE_ADVERTISEMENT_TIMEOUT;
    } else if (module->type == MEDIA_TYPE_15_4) {
        hal->def_channel = hal->channel_list.channels[0];
        hal->discovery_interval = IEEE154_DISCOVERY_TIMEOUT;
        hal->auth_request_interval = IEEE154_AUTH_REQUEST_TIMEOUT;
        hal->auth_relay_interval = IEEE154_AUTH_RELAY_TIMEOUT;
        hal->auth_response_interval = IEEE154_AUTH_RESPONSE_TIMEOUT;
        hal->link_quality_update_interval = (umesh_get_mode() & MODE_MOBILE)? \
                     IEEE154_LINK_QUALITY_MOBILE_TIMEOUT: IEEE154_LINK_QUALITY_TIMEOUT;
        hal->neighbor_alive_interval = IEEE154_NEIGHBOR_ALIVE_TIMEOUT;
        hal->advertisement_interval = IEEE154_ADVERTISEMENT_TIMEOUT;
    }
    return hal;
}

void interface_init(void)
{
    umesh_hal_module_t *module;

    module = hal_umesh_get_default_module();
    while (module) {
        assert(module->type >= MEDIA_TYPE_DFL && module->type <= MEDIA_TYPE_15_4);
        new_hal_context(module, module->type);
        module = hal_umesh_get_next_module(module);
#ifndef CONFIG_AOS_MESH_SUPER
        break;
#endif
    }
}

void interface_start(void)
{
    hal_context_t *hal;
    uint8_t index;

    index = 0;
    slist_for_each_entry(&g_hals_list, hal, hal_context_t, next) {
        bool is_wifi = hal->module->type == MEDIA_TYPE_WIFI;

        if (is_wifi) {
            if (umesh_mm_get_mode() & MODE_SUPER) {
                new_network_context(hal, index++, VECTOR_ROUTER);
                new_network_context(hal, index++, SID_ROUTER);
            } else {
                new_network_context(hal, index++, SID_ROUTER);
            }
        } else {
            new_network_context(hal, index++, SID_ROUTER);
        }
    }
}

static void cleanup_one_queue(message_queue_t *queue)
{
    message_t *message;

    while ((message = message_queue_get_head(queue))) {
        message_queue_dequeue(message);
        message_free(message);
    }
}

static void cleanup_queues(hal_context_t *hal)
{
    int i;

    for (i = 0; i < QUEUE_SIZE; i++) {
        cleanup_one_queue(&hal->send_queue[i]);
    }

    cleanup_one_queue(&hal->recv_queue);
}

void interface_stop(void)
{
    hal_context_t *hal;
    neighbor_t *node;
    reset_network_context();

    slist_for_each_entry(&g_hals_list, hal, hal_context_t, next) {
        cleanup_queues(hal);
        hal->send_message = NULL;

        while (!slist_empty(&hal->neighbors_list)) {
            node = slist_first_entry(&hal->neighbors_list, neighbor_t, next);
            remove_neighbor(hal, node);
        }
        slist_init(&hal->neighbors_list);
        hal->neighbors_num  = 0;
    }

    while (!slist_empty(&g_networks_list)) {
        network_context_t *network;
        network = slist_first_entry(&g_networks_list, network_context_t, next);
        slist_del(&network->next, &g_networks_list);
        ur_mem_free(network, sizeof(*network));
    }
}

void reset_network_context(void)
{
    slist_t *networks;
    network_context_t *network;

    networks = get_network_contexts();
    slist_for_each_entry(networks, network, network_context_t, next) {
        network->state = INTERFACE_DOWN;
        ur_stop_timer(&network->advertisement_timer, network);
        network->meshnetid = BCAST_NETID;
    }
}

slist_t *get_network_contexts(void)
{
    return &g_networks_list;
}

network_context_t *get_default_network_context(void)
{
    return slist_first_entry(&g_networks_list, network_context_t, next);
}

network_context_t *get_sub_network_context(hal_context_t *hal)
{
    network_context_t *network = get_default_network_context();

    if (slist_entry_number(&g_networks_list) < 2) {
        return network;
    }
    slist_for_each_entry(&g_networks_list, network, network_context_t, next) {
        if (network->hal == hal) {
            break;
        }
    }
    if (network->hal->module->type == MEDIA_TYPE_WIFI) {
        return slist_first_entry(&network->next, network_context_t, next);
    } else {
        return network;
    }
}

network_context_t *get_hal_default_network_context(hal_context_t *hal)
{
    network_context_t *network = NULL;

    slist_for_each_entry(&g_networks_list, network, network_context_t, next) {
        if (network->hal == hal) {
            break;
        }
    }

    return network;
}

network_context_t *get_network_context_by_meshnetid(uint16_t meshnetid, bool def)
{
    network_context_t *network = NULL;

    slist_for_each_entry(&g_networks_list, network, network_context_t, next) {
        if (network->meshnetid == meshnetid) {
            break;
        }
    }
    if (network == NULL && def) {
        network = get_default_network_context();
    }
    return network;
}

slist_t *get_hal_contexts(void)
{
    return &g_hals_list;
}

uint8_t get_hal_contexts_num(void)
{
    return slist_entry_number(&g_hals_list);
}

hal_context_t *get_default_hal_context(void)
{
    return slist_first_entry(&g_hals_list, hal_context_t, next);
}

hal_context_t *get_hal_context(media_type_t type)
{
    hal_context_t *hal = NULL;

    slist_for_each_entry(&g_hals_list, hal, hal_context_t, next) {
        if (hal->module->type == type) {
            break;
        }
    }
    return hal;
}
