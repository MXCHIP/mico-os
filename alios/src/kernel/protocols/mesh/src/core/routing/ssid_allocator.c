/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "core/sid_allocator.h"
#include "umesh_utils.h"

#define LEADER_DEF_BITMAP 0x0ffe
#define ROUTER_DEF_BITMAP 0xfffe

allocator_t allocator_init(uint16_t sid, int sid_type)
{
    ssid_allocator_t *allocator;
    uint16_t index;

    allocator = (ssid_allocator_t *)ur_mem_alloc(sizeof(ssid_allocator_t));
    if (allocator == NULL) {
        return 0;
    }
    slist_init(&allocator->base.node_list);

    allocator->base.node_num = 0;
    allocator->pf_node_num = 0;
    allocator->base.sid_type = sid_type;

    if (sid == LEADER_SID) {
        allocator->attach_free_bits = LEADER_DEF_BITMAP;
    } else {
        allocator->attach_free_bits = ROUTER_DEF_BITMAP;
    }

    memset(allocator->mobile_free_bits, 0xff, sizeof(allocator->mobile_free_bits));
    allocator->mobile_free_bits[0] -= 1;

    for (index = 0; index < SID_LEN; index += SID_MASK_LEN) {
        uint16_t mask = (1 << SID_MASK_LEN) - 1;
        mask <<= index;
        if (sid & mask) {
            break;
        }
    }

    allocator->sid_shift = index - SID_MASK_LEN;
    allocator->sid_prefix = sid;

    return (allocator_t)allocator;
}

void allocator_deinit(allocator_t hdl)
{
    ssid_allocator_t *allocator;
    sid_node_t *node;

    allocator = (ssid_allocator_t *)hdl;
    if (allocator == NULL) {
        return;
    }
    while (!slist_empty(&allocator->base.node_list)) {
        node = slist_first_entry(&allocator->base.node_list, sid_node_t, next);
        slist_del(&node->next, &allocator->base.node_list);
        ur_mem_free(node, sizeof(sid_node_t));
    }
    allocator->base.node_num = 0;
    ur_mem_free(allocator, sizeof(ssid_allocator_t));
}

bool is_direct_child(allocator_t hdl, uint16_t sid)
{
    ssid_allocator_t *allocator;
    uint16_t sidmask = 0;
    uint8_t index;

    if (sid == LEADER_SID || is_unique_sid(sid) == false) {
        return false;
    }
    allocator = (ssid_allocator_t *)hdl;
    if (allocator == NULL || allocator->sid_shift < 0) {
        return false;
    }

    index = SID_LEN - SID_MASK_LEN;
    while (index > allocator->sid_shift) {
        sidmask |= (SID_MASK << index);
        index -= SID_MASK_LEN;
    }
    if (allocator->sid_prefix != (sid & sidmask ) ||
        allocator->sid_prefix == sid) {
        return false;
    }
    sidmask = (1 << allocator->sid_shift) - 1;
    if (sid & sidmask) {
        return false;
    }

    return true;
}

sid_node_t *get_allocated_child(allocator_t hdl, neighbor_t *nbr)
{
    ssid_allocator_t *allocator;
    sid_node_t *node;

    allocator = (ssid_allocator_t *)hdl;
    if (allocator == NULL) {
        return NULL;
    }
    slist_for_each_entry(&allocator->base.node_list, node, sid_node_t, next) {
        if (memcmp(node->node_id.uuid, nbr->mac, sizeof(nbr->mac)) == 0) {
            break;
        }
    }
    return node;
}

ur_error_t update_sid_mapping(allocator_t hdl,
                              ur_node_id_t *node_id, bool to_add)
{
    ssid_allocator_t *allocator;
    sid_node_t *node;
    sid_node_t *new_node = NULL;

    allocator = (ssid_allocator_t *)hdl;
    slist_for_each_entry(&allocator->base.node_list, node, sid_node_t, next) {
        if (memcmp(node->node_id.uuid, node_id->uuid,
                   sizeof(node->node_id.uuid)) == 0) {
            new_node = node;
            break;
        }
    }

    if (to_add == false) {
        if (node) {
            free_sid(hdl, node_id->sid);
            slist_del(&node->next, &allocator->base.node_list);
            ur_mem_free(node, sizeof(sid_node_t));
        }
        return UR_ERROR_NONE;
    }

    if (new_node) {
        if (is_unique_sid(node_id->sid) && node_id->sid != new_node->node_id.sid) {
            free_sid(hdl, new_node->node_id.sid);
        }
    } else {
        new_node = (sid_node_t *)ur_mem_alloc(sizeof(sid_node_t));
        if (!new_node) {
            return UR_ERROR_MEM;
        }
        slist_add(&new_node->next, &allocator->base.node_list);
    }

    memcpy(new_node->node_id.uuid, node_id->uuid, sizeof(new_node->node_id.uuid));
    new_node->node_id.sid = node_id->sid;
    new_node->node_id.attach_sid = node_id->attach_sid;

    return UR_ERROR_NONE;
}

void free_sid(allocator_t hdl, uint16_t sid)
{
    ssid_allocator_t *allocator;
    uint8_t len;

    allocator = (ssid_allocator_t *)hdl;
    if (is_partial_function_sid(sid)) {
        uint16_t idx = sid - (MOBILE_PREFIX << PF_SID_PREFIX_OFFSET);
        if (release_bit(allocator->mobile_free_bits, PF_NODE_NUM, idx)) {
            allocator->pf_node_num --;
        }
        return;
    }

    if (!is_direct_child(hdl, sid)) {
        return;
    }
    sid -= allocator->sid_prefix;
    sid >>= allocator->sid_shift;
    len = 1 << SID_MASK_LEN;
    if (allocator->sid_prefix == LEADER_SID) {
        len = 12;
    }
    if (release_bit(&allocator->attach_free_bits, len, sid)) {
        allocator->base.node_num --;
    }
}

static ur_error_t allocate_expected_sid(allocator_t hdl,
                                        ur_node_id_t *node_id)
{
    ssid_allocator_t *allocator;
    uint8_t index;
    uint8_t len;

    allocator = (ssid_allocator_t *)hdl;
    if (is_direct_child(hdl, node_id->sid)) {
        index = (node_id->sid - allocator->sid_prefix) >> allocator->sid_shift;
        len = 16;
        if (allocator->sid_prefix == LEADER_SID) {
            len = 12;
        }
        if ((index > len) ||
            (grab_free_bit(&allocator->attach_free_bits, len, index) == UR_ERROR_FAIL)) {
            return UR_ERROR_FAIL;
        }
        if (UR_ERROR_NONE != update_sid_mapping(hdl, node_id, true)) {
            free_sid(hdl, node_id->sid);
            return UR_ERROR_FAIL;
        }
        allocator->base.node_num++;
    } else if (is_partial_function_sid(node_id->sid)) {
        index = node_id->sid - (MOBILE_PREFIX << PF_SID_PREFIX_OFFSET);
        if (index > PF_NODE_NUM) {
            return UR_ERROR_FAIL;
        }
        if (grab_free_bit(allocator->mobile_free_bits, PF_NODE_NUM,
                          index) == UR_ERROR_FAIL) {
            return UR_ERROR_FAIL;
        }
        if (update_sid_mapping(hdl, node_id, true) != UR_ERROR_NONE) {
            release_bit(allocator->mobile_free_bits, PF_NODE_NUM, node_id->sid);
            return UR_ERROR_FAIL;
        }
        allocator->pf_node_num++;
    } else {
        return UR_ERROR_FAIL;
    }
    return UR_ERROR_NONE;
}

sid_node_t *get_sid_mapping(allocator_t hdl, ur_node_id_t *node_id)
{
    sid_node_t *sid_node;
    ssid_allocator_t *allocator;

    allocator = (ssid_allocator_t *)hdl;
    slist_for_each_entry(&allocator->base.node_list, sid_node, sid_node_t, next) {
        if (memcmp(sid_node->node_id.uuid, node_id->uuid,
                   sizeof(sid_node->node_id.uuid)) == 0) {
            break;
        }
    }

    if (sid_node) {
        node_id->type = allocator->sid_shift == 0 ? LEAF_NODE : ROUTER_NODE;
        if (node_id->mode & MODE_MOBILE) {
            node_id->type = LEAF_NODE;
        }
        node_id->sid = sid_node->node_id.sid;
    }

    return sid_node;
}

ur_error_t allocate_sid(allocator_t hdl, ur_node_id_t *node_id)
{
    ssid_allocator_t *allocator;
    sid_node_t *sid_node = NULL;
    int newsid = -1;

    allocator = (ssid_allocator_t *)hdl;
    sid_node = get_sid_mapping(hdl, node_id);
    if (sid_node) {
        return UR_ERROR_NONE;
    }

    if (is_unique_sid(node_id->sid) &&
        allocate_expected_sid(hdl, node_id) == UR_ERROR_FAIL) {
        goto new_sid;
    }

    if (is_unique_sid(node_id->sid) == false) {
        goto new_sid;
    }

    node_id->type = allocator->sid_shift == 0 ? LEAF_NODE : ROUTER_NODE;
    return UR_ERROR_NONE;

new_sid:
    if (node_id->mode & MODE_MOBILE) {
        slist_for_each_entry(&allocator->base.node_list, sid_node, sid_node_t, next) {
            if (memcmp(sid_node->node_id.uuid, node_id->uuid,
                       sizeof(sid_node->node_id.uuid)) == 0) {
                break;
            }
        }
        node_id->type = LEAF_NODE;
        if (sid_node == NULL) {
            newsid = find_first_free_bit(allocator->mobile_free_bits, PF_NODE_NUM);
            node_id->sid = ((uint16_t)newsid) | (MOBILE_PREFIX << PF_SID_PREFIX_OFFSET);
        } else {
            newsid = 0;
            node_id->sid = sid_node->node_id.sid;
        }
    } else {
        if (allocator->sid_prefix == LEADER_SID) {
            newsid = find_first_free_bit(&allocator->attach_free_bits, 12);
        } else {
            newsid = find_first_free_bit(&allocator->attach_free_bits, 16);
        }
        node_id->sid = allocator->sid_prefix | (((uint16_t)newsid) <<
                                                allocator->sid_shift);
        node_id->type = allocator->sid_shift == 0 ? LEAF_NODE : ROUTER_NODE;
    }

    if (newsid < 0) {
        return UR_ERROR_MEM;
    }
    MESH_LOG_DEBUG("allocate 0x%04x", node_id->sid);

    if (!(node_id->mode & MODE_MOBILE)) {
        if (UR_ERROR_NONE != update_sid_mapping(hdl, node_id, true)) {
            free_sid(hdl, node_id->sid);
            return UR_ERROR_FAIL;
        }
        allocator->base.node_num++;
        goto out;
    }

    if (UR_ERROR_NONE != update_sid_mapping(hdl, node_id, true)) {
        release_bit(allocator->mobile_free_bits, PF_NODE_NUM, newsid);
        return UR_ERROR_FAIL;
    }
    if (newsid > 0) {
        allocator->pf_node_num++;
    }

out:
    return UR_ERROR_NONE;
}

uint16_t get_allocated_number(allocator_t hdl)
{
    ssid_allocator_t *allocator = (ssid_allocator_t *)hdl;
    return allocator->base.node_num;
}

uint32_t get_allocated_bitmap(allocator_t hdl)
{
    ssid_allocator_t *allocator = (ssid_allocator_t *)hdl;
    if (allocator->sid_prefix == LEADER_SID) {
        return LEADER_DEF_BITMAP & ~allocator->attach_free_bits;
    }
    return ROUTER_DEF_BITMAP & ~allocator->attach_free_bits;
}


uint16_t get_allocated_pf_number(allocator_t hdl)
{
    ssid_allocator_t *allocator = (ssid_allocator_t *)hdl;
    return allocator->pf_node_num;
}

uint16_t get_free_number(allocator_t hdl)
{
    ssid_allocator_t *allocator = (ssid_allocator_t *)hdl;
    if (allocator->sid_prefix == LEADER_SID) {
        return 11 - allocator->base.node_num;
    } else if (allocator->sid_shift == -SID_MASK_LEN) {
        return 0;
    }
    return 15 - allocator->base.node_num;
}

slist_t *get_ssid_nodes_list(allocator_t hdl)
{
    ssid_allocator_t *allocator = (ssid_allocator_t *)hdl;
    return &allocator->base.node_list;
}

bool is_partial_function_sid(uint16_t sid)
{
    if (is_unique_sid(sid) == false) {
        return false;
    }
    if (((sid >> PF_SID_PREFIX_OFFSET) & PF_SID_PREFIX_MASK) >= MOBILE_PREFIX) {
        return true;
    }
    return false;
}
