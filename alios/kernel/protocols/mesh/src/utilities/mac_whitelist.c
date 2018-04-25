/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>
#include <stdbool.h>

#include "umesh_utils.h"

typedef struct mac_whitelist_state_s {
    bool enabled;
    whitelist_entry_t entries[WHITELIST_ENTRY_NUM];
} mac_whitelist_state_t;

static mac_whitelist_state_t g_mw_state = {.enabled = false};

bool is_whitelist_enabled(void)
{
    return g_mw_state.enabled;
}

void whitelist_enable(void)
{
    g_mw_state.enabled = true;
}

void whitelist_disable(void)
{
    g_mw_state.enabled = false;
}

bool whitelist_is_enabled(void)
{
    return g_mw_state.enabled;
}

const whitelist_entry_t *whitelist_get_entries(void)
{
    return g_mw_state.entries;
}

whitelist_entry_t *whitelist_add(const mac_address_t *address)
{
    whitelist_entry_t *entry = NULL;
    uint8_t index;

    entry = whitelist_find(address);
    if (entry) {
        return entry;
    }
    for (index = 0; index < WHITELIST_ENTRY_NUM; index++) {
        if (g_mw_state.entries[index].valid) {
            continue;
        }
        memcpy(&g_mw_state.entries[index].address.addr, address->addr,
               sizeof(g_mw_state.entries[index].address.addr));
        g_mw_state.entries[index].address.len = address->len;
        g_mw_state.entries[index].valid = true;
        g_mw_state.entries[index].constant_rssi = false;
        entry = &g_mw_state.entries[index];
        break;
    }
    return entry;
}

void whitelist_remove(const mac_address_t *address)
{
    whitelist_entry_t *entry;

    entry = whitelist_find(address);
    if (entry) {
        memset(entry, 0, sizeof(*entry));
    }
}

whitelist_entry_t *whitelist_find(const mac_address_t *address)
{
    whitelist_entry_t *entry = NULL;
    uint8_t index;

    for (index = 0; index < WHITELIST_ENTRY_NUM; index++) {
        if (g_mw_state.entries[index].valid == false) {
            continue;
        }
        if ((g_mw_state.entries[index].address.len == address->len) &&
            (memcmp(&g_mw_state.entries[index].address.addr, address->addr,
                    sizeof(g_mw_state.entries[index].address.addr)) == 0)) {
            entry = &g_mw_state.entries[index];
            break;
        }
    }
    return entry;
}

void whitelist_clear(void)
{
    uint8_t index;

    for (index = 0; index < WHITELIST_ENTRY_NUM; index++) {
        g_mw_state.entries[index].valid = false;
    }
}

void whitelist_clear_constant_rssi(whitelist_entry_t *entry)
{
    entry->constant_rssi = false;
}

ur_error_t whitelist_get_constant_rssi(const whitelist_entry_t *entry,
                                       int8_t *rssi)
{
    ur_error_t error = UR_ERROR_FAIL;

    if (entry->valid == true && entry->constant_rssi) {
        *rssi = entry->rssi;
        error = UR_ERROR_NONE;
    }
    return error;
}

void whitelist_set_constant_rssi(whitelist_entry_t *entry, int8_t rssi)
{
    entry->constant_rssi = true;
    entry->rssi = rssi;
}
