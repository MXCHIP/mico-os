/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stddef.h>

#include "umesh_hal.h"

static AOS_DLIST_HEAD(g_mesh_module);

int hal_umesh_init(void)
{
    int ret = 0;
    dlist_t *t;

    /* do low level init */
    dlist_for_each(t, &g_mesh_module) {
        umesh_hal_module_t *m = (umesh_hal_module_t *)t;

        if (m->umesh_hal_init) {
            ret = m->umesh_hal_init(m, NULL);

            if (ret < 0) {
                break;
            }
        }
    }
    return ret;
}

void hal_umesh_register_module(umesh_hal_module_t *m)
{
    dlist_t *t;

    dlist_for_each(t, &g_mesh_module) {
        umesh_hal_module_t *module = (umesh_hal_module_t *)t;
        if (module == m) {
            return;
        }
    }
    dlist_add_tail(&m->base.list, &g_mesh_module);
}

umesh_hal_module_t *hal_umesh_get_default_module(void)
{
    umesh_hal_module_t *m = NULL;

    if (dlist_empty(&g_mesh_module)) {
        return NULL;
    }

    m = dlist_first_entry(&g_mesh_module, umesh_hal_module_t, base.list);
    return m;
}

umesh_hal_module_t *hal_umesh_get_next_module(umesh_hal_module_t *cur)
{
    umesh_hal_module_t *m = NULL;

    if (cur->base.list.next == &g_mesh_module) {
        return NULL;
    }

    m = dlist_first_entry(&cur->base.list, umesh_hal_module_t, base.list);
    return m;
}

int hal_umesh_enable(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_enable != NULL)) {
        return m->umesh_hal_enable(m);
    }

    return -1;
}

int hal_umesh_disable(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_disable != NULL)) {
        return m->umesh_hal_disable(m);
    }

    return -1;
}

int hal_umesh_send_ucast_request(umesh_hal_module_t *m,
                                 frame_t *frame, mac_address_t *dest,
                                 umesh_handle_sent_ucast_t sent, void *context)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_send_ucast_request != NULL)) {
        return m->umesh_hal_send_ucast_request(m, frame, dest, sent, context);
    }

    return -1;
}

int hal_umesh_send_bcast_request(umesh_hal_module_t *m,
                                 frame_t *frame,
                                 umesh_handle_sent_bcast_t sent, void *context)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_send_bcast_request != NULL)) {
        return m->umesh_hal_send_bcast_request(m, frame, sent, context);
    }

    return -1;
}

int hal_umesh_register_receiver(umesh_hal_module_t *m,
                                umesh_handle_received_frame_t received, void *context)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_register_receiver != NULL)) {
        return m->umesh_hal_register_receiver(m, received, context);
    }

    return -1;
}

int hal_umesh_get_bcast_mtu(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_bcast_mtu != NULL)) {
        return m->umesh_hal_get_bcast_mtu(m);
    }

    return -1;
}

int hal_umesh_get_ucast_mtu(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_ucast_mtu != NULL)) {
        return m->umesh_hal_get_ucast_mtu(m);
    }

    return -1;
}

int hal_umesh_set_channel(umesh_hal_module_t *m, uint8_t channel)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_set_channel != NULL)) {
        return m->umesh_hal_set_channel(m, channel);
    }

    return -1;
}

int hal_umesh_get_channel(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_channel != NULL)) {
        return m->umesh_hal_get_channel(m);
    }

    return -1;
}

int hal_umesh_get_chnlist(umesh_hal_module_t *m,
                          const uint8_t **chnlist)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_chnlist != NULL)) {
        return m->umesh_hal_get_chnlist(m, chnlist);
    }

    return -1;
}

int hal_umesh_set_txpower(umesh_hal_module_t *m, int8_t txpower)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_set_txpower != NULL)) {
        return m->umesh_hal_set_txpower(m, txpower);
    }

    return -1;
}

int hal_umesh_get_txpower(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_txpower != NULL)) {
        return m->umesh_hal_get_txpower(m);
    }

    return -1;
}

int hal_umesh_set_extnetid(umesh_hal_module_t *m,
                           const umesh_extnetid_t *extnetid)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_set_extnetid != NULL)) {
        return m->umesh_hal_set_extnetid(m, extnetid);
    }

    return -1;
}

void hal_umesh_get_extnetid(umesh_hal_module_t *m,
                            umesh_extnetid_t *extnetid)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_extnetid != NULL)) {
        m->umesh_hal_get_extnetid(m, extnetid);
    }
}

const mac_address_t *hal_umesh_get_mac_address(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_mac_address != NULL)) {
        return m->umesh_hal_get_mac_address(m);
    }

    return NULL;
}

int hal_umesh_radio_wakeup(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_radio_wakeup != NULL)) {
        return m->umesh_hal_radio_wakeup(m);
    }

    return -1;
}

int hal_umesh_radio_sleep(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_radio_sleep != NULL)) {
        return m->umesh_hal_radio_sleep(m);
    }

    return -1;
}

const frame_stats_t *hal_umesh_get_stats(umesh_hal_module_t *m)
{
    if (m == NULL) {
        m = hal_umesh_get_default_module();
    }

    if ((m != NULL) && (m->umesh_hal_get_stats != NULL)) {
        return m->umesh_hal_get_stats(m);
    }

    return NULL;
}
