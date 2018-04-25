/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <assert.h>
#include <string.h>

#include "core/mesh_mgmt.h"
#include "core/network_data.h"
#include "hal/interfaces.h"

typedef struct network_data_state_s {
    network_data_t network_data;
    stable_network_data_t stable_network_data;
    slist_t updater_list;
} network_data_state_t;

static network_data_state_t g_nd_state;

static uint16_t generate_meshnetid(void)
{
    uint16_t meshnetid = 0x0000;
    uint8_t *uuid;

    /* TODO: replace XOR with hash function */
    uuid = umesh_mm_get_local_uuid();
    meshnetid ^= ((uint16_t)uuid[0]) << 8;
    meshnetid ^= ((uint16_t)uuid[1]) << 8;
    meshnetid ^= ((uint16_t)uuid[2]) << 8;
    meshnetid ^= ((uint16_t)uuid[3]) << 8;
    meshnetid ^= ((uint16_t)uuid[4]) << 8;
    meshnetid ^= ((uint16_t)uuid[5]) << 8;
    meshnetid ^= ((uint16_t)uuid[6]) << 8;
    meshnetid ^= ((uint16_t)uuid[7]) << 8;
    return meshnetid;
}

ur_error_t nd_init(void)
{
    slist_t *networks;
    network_context_t *network;

    networks = get_network_contexts();
    slist_for_each_entry(networks, network, network_context_t, next) {
        memset(&network->network_data, 0, sizeof(network->network_data));
    }
    memset(&g_nd_state.network_data, 0, sizeof(g_nd_state.network_data));
    memset(&g_nd_state.stable_network_data, 0,
           sizeof(g_nd_state.stable_network_data));
    if (umesh_mm_get_mode() & MODE_MOBILE) {
        g_nd_state.stable_network_data.meshnetid = INVALID_NETID;
    } else {
        g_nd_state.stable_network_data.meshnetid = generate_meshnetid();
    }
    return UR_ERROR_NONE;
}

ur_error_t nd_stable_set(stable_network_data_t *network_data)
{
    ur_error_t error = UR_ERROR_FAIL;
    int8_t diff;

    diff = network_data->minor_version -
           g_nd_state.stable_network_data.minor_version;
    if (diff >= 0 || g_nd_state.stable_network_data.minor_version == 0) {
        memcpy(&g_nd_state.stable_network_data, network_data,
               sizeof(g_nd_state.stable_network_data));
        error = UR_ERROR_NONE;
    }
    return error;
}

ur_error_t nd_set_stable_main_version(uint8_t version)
{
    ur_configs_t configs;

    if (version > 7) {
        return UR_ERROR_FAIL;
    }
    g_nd_state.stable_network_data.main_version = version;
    ur_configs_read(&configs);
    configs.main_version = g_nd_state.stable_network_data.main_version;
    ur_configs_write(&configs);
    return UR_ERROR_NONE;
}

uint8_t nd_get_stable_main_version(void)
{
    return g_nd_state.stable_network_data.main_version;
}

uint8_t nd_get_stable_minor_version(void)
{
    return g_nd_state.stable_network_data.minor_version;
}

uint16_t nd_get_stable_meshnetid(void)
{
    return g_nd_state.stable_network_data.meshnetid;
}

ur_error_t nd_set_stable_meshnetid(uint16_t meshnetid)
{
    ur_error_t error = UR_ERROR_NONE;
    stable_network_data_t network_data;

    if (umesh_mm_get_device_state() != DEVICE_STATE_LEADER) {
        return UR_ERROR_FAIL;
    }

    memcpy(&network_data, &g_nd_state.stable_network_data, sizeof(network_data));
    if (is_unique_netid(meshnetid) && network_data.meshnetid != meshnetid) {
        network_data.minor_version++;
        network_data.meshnetid = meshnetid;
        network_data.mcast_addr[0].m8[6] = (meshnetid >> 8);
        network_data.mcast_addr[0].m8[7] = meshnetid;
        error = nd_stable_set(&network_data);
    }
    return error;
}

const ur_ip6_addr_t *nd_get_subscribed_mcast(void)
{
    return g_nd_state.stable_network_data.mcast_addr;
}

bool nd_is_subscribed_mcast(const ur_ip6_addr_t *addr)
{
    return !memcmp(addr, g_nd_state.stable_network_data.mcast_addr,
                   sizeof(ur_ip6_addr_t));
}

ur_error_t nd_set(network_context_t *network, network_data_t *network_data)
{
    ur_error_t error = UR_ERROR_FAIL;
    int8_t diff;
    network_data_t *local_network_data;

    local_network_data = network? &network->network_data: &g_nd_state.network_data;
    diff = network_data->version - local_network_data->version;
    if (diff > 0) {
        memcpy(local_network_data, network_data, sizeof(network_data_t));
        error = UR_ERROR_NONE;
    }
    return error;
}

uint8_t nd_get_version(network_context_t *network)
{
    network_data_t *local_network_data;

    local_network_data = network? &network->network_data: &g_nd_state.network_data;
    return local_network_data->version;
}

uint16_t nd_get_meshnetsize(network_context_t *network)
{
    network_data_t *local_network_data;

    local_network_data = network? &network->network_data: &g_nd_state.network_data;
    return local_network_data->size;
}

ur_error_t nd_set_meshnetsize(network_context_t *network, uint32_t size)
{
    ur_error_t error = UR_ERROR_NONE;
    network_data_t network_data;
    network_data_t *local_network_data;

    if (umesh_mm_get_device_state() != DEVICE_STATE_LEADER &&
        umesh_mm_get_device_state() != DEVICE_STATE_SUPER_ROUTER) {
        return UR_ERROR_FAIL;
    }

    local_network_data = network? &network->network_data: &g_nd_state.network_data;
    memcpy(&network_data, local_network_data, sizeof(network_data));
    if (network_data.size != size) {
        network_data.version++;
        network_data.size = size;
        error = nd_set(network, &network_data);
    }
    return error;
}
