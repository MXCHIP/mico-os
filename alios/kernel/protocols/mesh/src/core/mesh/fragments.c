/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "umesh.h"
#include "umesh_utils.h"
#include "core/mesh_mgmt.h"
#include "core/fragments.h"
#include "core/network_data.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "ip/ip.h"

typedef struct reass_s {
    slist_t next;
    message_t *message;
    uint16_t sender_addr;
    uint16_t datagram_size;
    uint16_t datagram_tag;
    uint8_t timer;
} reass_t;

typedef struct fragments_state_s {
    slist_t reass_list;
    ur_timer_t reass_timer;
    mm_cb_t interface_callback;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_events_handler_t lowpower_callback;
#endif
} fragments_state_t;
fragments_state_t g_frags_state;

void del_reass(reass_t *reass) {
    slist_del(&reass->next, &g_frags_state.reass_list);
    message_free(reass->message);
    ur_mem_free(reass, sizeof(reass_t));
}

ur_error_t frags_reassemble(message_t *p, message_t **reass_p)
{
    frag_header_t *frag_header;
    frag_header_t frag_header_content;
    uint16_t datagram_size, datagram_tag, datagram_offset;
    reass_t *lrh;
    message_info_t *info;
    slist_t *list_tmp;

    if (p == NULL) {
        return UR_ERROR_FAIL;
    }

    info = p->info;
    *reass_p = NULL;

    message_copy_to(p, 0, (uint8_t *)&frag_header_content, sizeof(frag_header_t));
    frag_header = &frag_header_content;
    *((uint16_t *)frag_header) = ntohs(*(uint16_t *)frag_header);
    datagram_size = frag_header->size;
    datagram_tag = ntohs(frag_header->tag);
    /* Check dispatch. */
    if (frag_header->dispatch == FRAG_1_DISPATCH) {
        /* check for duplicate */
        slist_for_each_entry_safe(&g_frags_state.reass_list, list_tmp, lrh, reass_t, next) {
            if (lrh->sender_addr != info->src.addr.short_addr) {
                continue;
            }
            /* address match with packet in reassembly. */
            if ((datagram_tag == lrh->datagram_tag) &&
                (datagram_size == lrh->datagram_size)) {
                /* duplicate fragment. */
                MESH_LOG_DEBUG("frags: received duplicated FRAG_1 from"
                               " %04hx (tag=%u tot_len=%u), drop it",
                               info->src.addr.short_addr, datagram_tag, datagram_size);
                return UR_ERROR_FAIL;
            } else {
                del_reass(lrh);
            }
        }
        message_set_payload_offset(p, -4); /* hide FRAG_1 header */

        MESH_LOG_DEBUG("frags: received new FRAG_1 from %04hx, tag=%u tot_len=%u len=%u offset=0",
                       info->src.addr.short_addr, datagram_tag, datagram_size, message_get_msglen(p));

        lrh = (reass_t *) ur_mem_alloc(sizeof(reass_t));
        if (lrh == NULL) {
            return UR_ERROR_FAIL;
        }

        lrh->sender_addr = info->src.addr.short_addr;
        lrh->datagram_size = datagram_size;
        lrh->datagram_tag = datagram_tag;
        lrh->message = p;
        lrh->timer = 5;
        slist_add(&lrh->next, &g_frags_state.reass_list);
        return UR_ERROR_NONE;
    } else if (frag_header->dispatch == FRAG_N_DISPATCH) {
        /* FRAGN dispatch, find packet being reassembled. */
        datagram_offset = ((uint16_t)frag_header->offset) << 3;
        message_set_payload_offset(p, -5);
        slist_for_each_entry(&g_frags_state.reass_list, lrh, reass_t, next) {
            if ((lrh->sender_addr == info->src.addr.short_addr) &&
                (lrh->datagram_tag == datagram_tag) &&
                (lrh->datagram_size == datagram_size)) {
                break;
            }
        }

        if (lrh == NULL) {
            /* rogue fragment */
            return UR_ERROR_FAIL;
        }

        if (message_get_msglen(lrh->message) > datagram_offset) {
            /* duplicate, ignore. */
            MESH_LOG_DEBUG("frags: received duplicated FRAG_N from"
                           " %04hx, tag=%u len=%u offset=%u, drop it",
                           info->src.addr.short_addr, datagram_tag, message_get_msglen(p),
                           datagram_offset);
            return UR_ERROR_FAIL;
        } else if (message_get_msglen(lrh->message) < datagram_offset) {
            /* We have missed a fragment. Delete whole reassembly. */
            MESH_LOG_DEBUG("frags: received disordered FRAG_N from %04hx,"
                           " tag=%u len=%u offset=%u, drop the whole fragment packets",
                           info->src.addr.short_addr, datagram_tag, message_get_msglen(p),
                           datagram_offset);
            del_reass(lrh);
            return UR_ERROR_FAIL;
        }

        MESH_LOG_DEBUG("frags: received FRAG_N from %04hx, tag=%u len=%u offset=%u",
                       info->src.addr.short_addr, datagram_tag, message_get_msglen(p),
                       datagram_offset);
        message_concatenate(lrh->message, p, false);

        /* is packet now complete?*/
        if (message_get_msglen(lrh->message) >= lrh->datagram_size) {
            *reass_p = message_alloc(message_get_msglen(lrh->message), LOWPAN6_2);
            message_copy(*reass_p, lrh->message);
            del_reass(lrh);
            return UR_ERROR_NONE;
        }
    } else {
        MESH_LOG_DEBUG("frags: unrecognized FRAG packet, drop it");
        return UR_ERROR_FAIL;
    }

    return UR_ERROR_NONE;
}

void frags_cleanup(bool force)
{
    reass_t *lrh;
    slist_t *list_tmp;

    slist_for_each_entry_safe(&g_frags_state.reass_list, list_tmp, lrh, reass_t, next) {
        if (force || (--lrh->timer) == 0) {
            del_reass(lrh);
        }
    }
    if (force) {
        ur_stop_timer(&g_frags_state.reass_timer, NULL);
    }
}

static void frags_handle_timer(void *args)
{
    frags_cleanup(false);
    ur_start_timer(&g_frags_state.reass_timer, REASSEMBLE_TICK_INTERVAL, frags_handle_timer, NULL);
}

static ur_error_t mesh_interface_up(void)
{
    frags_cleanup(true);
    if (umesh_get_mode() & MODE_RX_ON) {
        ur_start_timer(&g_frags_state.reass_timer, REASSEMBLE_TICK_INTERVAL,
                       frags_handle_timer, NULL);
    }
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_down(interface_state_t state)
{
    frags_cleanup(true);
    return UR_ERROR_NONE;
}

#ifdef CONFIG_AOS_MESH_LOWPOWER
static void lowpower_radio_down_handler(schedule_type_t type)
{
    if ((umesh_get_mode() & MODE_RX_ON) == 0) {
        frags_cleanup(true);
    }
}

static void lowpower_radio_up_handler(schedule_type_t type)
{
    if ((umesh_get_mode() & MODE_RX_ON) == 0) {
        ur_start_timer(&g_frags_state.reass_timer, REASSEMBLE_TICK_INTERVAL,
                       frags_handle_timer, NULL);
    }
}
#endif

void frags_init(void) {
#ifdef CONFIG_AOS_MESH_LOWPOWER
    g_frags_state.lowpower_callback.radio_down = lowpower_radio_down_handler;
    g_frags_state.lowpower_callback.radio_up = lowpower_radio_up_handler;
    lowpower_register_callback(&g_frags_state.lowpower_callback);
#endif
    g_frags_state.interface_callback.interface_up = mesh_interface_up;
    g_frags_state.interface_callback.interface_down = mesh_interface_down;
    umesh_mm_register_callback(&g_frags_state.interface_callback);
}
