/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <umesh.h>
#include <umesh_hal.h>
#include <umesh_pal.h>

#include <stdlib.h>
#include <string.h>

void linux_wifi_register(void);
int main(int argc, char **argv)
{
    linux_wifi_register();

    umesh_pal_task_init();
    umesh_init(MODE_RX_ON);
    umesh_start();
    umesh_pal_task_entry();
    return 0;
}

static int linux_80211_mesh_init(umesh_hal_module_t *module, void *something)
{
    return 0;
}

static int linux_80211_mesh_send_ucast(umesh_hal_module_t *module, frame_t *frame,
                                   mac_address_t *dest,
                                   umesh_handle_sent_ucast_t sent,
                                   void *context)
{
    return 0;
}

static int linux_80211_mesh_send_bcast(umesh_hal_module_t *module, frame_t *frame,
                                   umesh_handle_sent_bcast_t sent,
                                   void *context)
{
    return 0;
}

static int linux_80211_mesh_get_u_mtu(umesh_hal_module_t *module)
{
    return 1024;
}

static int linux_80211_mesh_get_b_mtu(umesh_hal_module_t *module)
{
    return 1024;
}

static int linux_80211_mesh_set_rxcb(umesh_hal_module_t *module,
                          umesh_handle_received_frame_t received, void *context)
{
    return 0;
}

static const mac_address_t *linux_80211_mesh_get_mac_address(
                                        umesh_hal_module_t *module)
{
    static mac_address_t addr;

    memset(addr.addr, 6, 6);
    addr.len = 8;
    return &addr;
}

static int linux_80211_mesh_set_key(struct umesh_hal_module_s *module,
                                uint8_t index, uint8_t *key, uint8_t length)
{
    return 0;
}

static int linux_80211_mesh_is_sec_enabled(struct umesh_hal_module_s *module)
{
    return 0;
}

static int linux_80211_mesh_hal_get_channel(umesh_hal_module_t *module)
{
    return 6;
}

static int linux_80211_mesh_hal_set_channel(umesh_hal_module_t *module, uint8_t channel)
{
    return 0;
}

static int linux_80211_mesh_get_channel_list(umesh_hal_module_t *module, const uint8_t **chnlist)
{
    static uint8_t chns[1];
    *chnlist = chns;
    return sizeof(chns);
}

static const frame_stats_t *linux_80211_mesh_get_stats(struct umesh_hal_module_s *module)
{
    static frame_stats_t stats;
    return &stats;
}

static int linux_80211_wifi_mesh_set_extnetid(umesh_hal_module_t *module,
                                 const umesh_extnetid_t *extnetid)
{
    return 0;
}

static void linux_80211_wifi_mesh_get_extnetid(umesh_hal_module_t *module,
                                  umesh_extnetid_t *extnetid)
{
    if (extnetid == NULL) {
        return;
    }
    memset(extnetid->netid, 0x5, extnetid->len);
}

static umesh_hal_module_t linux_80211_mesh_wifi_module = {
    .base.name = "linux_80211_mesh_wifi_module",
    .type = MEDIA_TYPE_WIFI,
    .umesh_hal_init = linux_80211_mesh_init,
    .umesh_hal_send_ucast_request = linux_80211_mesh_send_ucast,
    .umesh_hal_send_bcast_request = linux_80211_mesh_send_bcast,
    .umesh_hal_register_receiver = linux_80211_mesh_set_rxcb,
    .umesh_hal_get_bcast_mtu = linux_80211_mesh_get_b_mtu,
    .umesh_hal_get_ucast_mtu = linux_80211_mesh_get_u_mtu,
    .umesh_hal_get_mac_address = linux_80211_mesh_get_mac_address,
    .umesh_hal_get_channel = linux_80211_mesh_hal_get_channel,
    .umesh_hal_set_channel = linux_80211_mesh_hal_set_channel,
    .umesh_hal_get_chnlist = linux_80211_mesh_get_channel_list,
    .umesh_hal_set_key = linux_80211_mesh_set_key,
    .umesh_hal_is_sec_enabled = linux_80211_mesh_is_sec_enabled,
    .umesh_hal_get_stats = linux_80211_mesh_get_stats,
    .umesh_hal_set_extnetid = linux_80211_wifi_mesh_set_extnetid,
    .umesh_hal_get_extnetid = linux_80211_wifi_mesh_get_extnetid,
};

void linux_wifi_register(void)
{
    hal_umesh_register_module(&linux_80211_mesh_wifi_module);
}
