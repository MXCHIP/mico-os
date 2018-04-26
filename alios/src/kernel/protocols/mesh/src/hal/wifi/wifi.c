/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>
#include <aos/kernel.h>

#include <hal/wifi.h>
#include <umesh_hal.h>
#include <umesh_80211.h>

static hal_wifi_module_t *m_wifi;
static const uint8_t wifi_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
static umesh_hal_module_t mesh_wifi_module;

enum {
    WIFI_MESH_OFFSET = 32,
    WIFI_DST_OFFSET = 4,
    WIFI_SRC_OFFSET = 10,
    WIFI_BSSID_OFFSET = 16,
    WIFI_MAC_ADDR_SIZE = 6,
    WIFI_FCS_SIZE = 4,

    DEFAULT_MTU_SIZE = 512,
};

typedef struct {
    umesh_handle_received_frame_t rxcb;
    void *rxcb_priv;
    frame_stats_t stats;
    unsigned char bssid[WIFI_MAC_ADDR_SIZE];
} mesh_hal_priv_t;

static int mesh_wifi_init(umesh_hal_module_t *module, void *something)
{
    return m_wifi->init(m_wifi);
}

static int mesh_wifi_enable(umesh_hal_module_t *module)
{
    if (m_wifi->mesh_enable)
        return m_wifi->mesh_enable(m_wifi);
    return 0;
}

static int mesh_wifi_disable(umesh_hal_module_t *module)
{
    if (m_wifi->mesh_disable)
        return m_wifi->mesh_disable(m_wifi);
    return 0;
}

static int mesh_wifi_send(umesh_hal_module_t *module, frame_t *frame,
                          mac_address_t *dest,
                          umesh_handle_sent_ucast_t sent,
                          void *context)
{
    uint8_t *pkt;
    int len = frame->len + WIFI_MESH_OFFSET;
    int result = 0;
    int ret;

    pkt = aos_malloc(len);
    if (pkt == NULL) {
        result = 1;
        goto tx_exit;
    }
    umesh_80211_make_frame(module, frame, dest, pkt);

    ret = hal_wlan_send_80211_raw_frame(m_wifi, pkt, len);

    if (ret != 0) {
        result = 1;
    } else {
        mesh_hal_priv_t *priv = module->base.priv_dev;
        priv->stats.out_frames++;
    }
    aos_free(pkt);

tx_exit:
    if (sent) {
        (*sent)(context, frame, result);
    }

    return 0;
}

static int mesh_wifi_send_bcast(umesh_hal_module_t *module, frame_t *frame,
                                umesh_handle_sent_bcast_t sent,
                                void *context)
{
    mac_address_t dest;
    dest.len = 8;
    memset(dest.addr, 0xff, sizeof(dest.addr));
    return mesh_wifi_send(module, frame, &dest, sent, context);
}

static int mesh_wifi_get_mtu(umesh_hal_module_t *module)
{
    return DEFAULT_MTU_SIZE;
}

static void mesh_wifi_data_cb(uint8_t *data, int len, hal_wifi_link_info_t *info)
{
    frame_t frm;
    frame_info_t fino;
    mesh_hal_priv_t *priv = mesh_wifi_module.base.priv_dev;

    if (priv->rxcb == NULL) {
        return;
    }

    if (umesh_80211_filter_frame(&mesh_wifi_module, data, len)) {
        return;
    }

    memset(&frm, 0, sizeof(frm));
    memset(&fino, 0, sizeof(fino));

    frm.len = len - WIFI_MESH_OFFSET - WIFI_FCS_SIZE;
    frm.data = aos_malloc(frm.len);
    if (frm.data == NULL) {
        return;
    }
    memcpy(frm.data, data + WIFI_MESH_OFFSET, frm.len);

    memcpy(fino.peer.addr, data + WIFI_SRC_OFFSET, WIFI_MAC_ADDR_SIZE);
    fino.peer.len = 8;
    fino.rssi = info ? info->rssi : 0;
    fino.channel = hal_wifi_get_channel(m_wifi);

    priv->stats.in_frames++;
    priv->rxcb(priv->rxcb_priv, &frm, &fino, 0);

    aos_free(frm.data);
}

static int mesh_wifi_set_rxcb(umesh_hal_module_t *module,
                          umesh_handle_received_frame_t received, void *context)
{
    mesh_hal_priv_t *priv = mesh_wifi_module.base.priv_dev;
    priv->rxcb = received;
    priv->rxcb_priv = context;
    m_wifi->mesh_register_cb(m_wifi, mesh_wifi_data_cb);
    return 0;
}

static const mac_address_t *mesh_wifi_get_mac_address(
                                        umesh_hal_module_t *module)
{
    static mac_address_t mac;
    mac.len = 8;
    hal_wifi_get_mac_addr(m_wifi, mac.addr);
    return &mac;
}

static int mesh_wifi_hal_set_channel(umesh_hal_module_t *m, uint8_t chn)
{
    return hal_wifi_set_channel(m_wifi, chn);
}

static int mesh_wifi_set_extnetid(umesh_hal_module_t *module,
                                 const umesh_extnetid_t *extnetid)
{
    mesh_hal_priv_t *priv = module->base.priv_dev;

    if (extnetid == NULL) {
        return -1;
    }
    memcpy(priv->bssid, extnetid->netid, WIFI_MAC_ADDR_SIZE);

    if (m_wifi->mesh_set_bssid)
        m_wifi->mesh_set_bssid(m_wifi, extnetid->netid);
    return 0;
}

static void mesh_wifi_get_extnetid(umesh_hal_module_t *module,
                                  umesh_extnetid_t *extnetid)
{
    mesh_hal_priv_t *priv = module->base.priv_dev;

    if (extnetid == NULL) {
        return;
    }
    extnetid->len = WIFI_MAC_ADDR_SIZE;
    memcpy(extnetid->netid, priv->bssid, extnetid->len);
}

static int mesh_wifi_get_channel_list(umesh_hal_module_t *module, const uint8_t **chnlist)
{
    if (m_wifi->get_channel_list)
        return hal_wifi_get_channel_list(m_wifi, chnlist);

    *chnlist = wifi_channels;
    return sizeof(wifi_channels);
}

static int mesh_wifi_get_channel(umesh_hal_module_t *module)
{
    return hal_wifi_get_channel(m_wifi);
}

static int mesh_wifi_radio_wakeup(struct umesh_hal_module_s *module)
{
    if (m_wifi->mesh_radio_wakeup)
        return m_wifi->mesh_radio_wakeup(m_wifi);
    return 0;
}

static int mesh_wifi_radio_sleep(struct umesh_hal_module_s *module)
{
    if (m_wifi->mesh_radio_sleep)
        return m_wifi->mesh_radio_sleep(m_wifi);
    return 0;
}

static mesh_hal_priv_t g_wifi_priv = {
    .bssid = {0xb0, 0xf8, 0x93, 0x00, 0x00, 0x07},
};

static umesh_hal_module_t mesh_wifi_module = {
    .base.name = "mesh_wifi_module",
    .base.priv_dev = &g_wifi_priv,
    .type = MEDIA_TYPE_WIFI,
    .umesh_hal_init = mesh_wifi_init,
    .umesh_hal_enable = mesh_wifi_enable,
    .umesh_hal_disable = mesh_wifi_disable,
    .umesh_hal_send_ucast_request = mesh_wifi_send,
    .umesh_hal_send_bcast_request = mesh_wifi_send_bcast,
    .umesh_hal_register_receiver = mesh_wifi_set_rxcb,
    .umesh_hal_get_bcast_mtu = mesh_wifi_get_mtu,
    .umesh_hal_get_ucast_mtu = mesh_wifi_get_mtu,
    .umesh_hal_get_mac_address = mesh_wifi_get_mac_address,
    .umesh_hal_set_channel = mesh_wifi_hal_set_channel,
    .umesh_hal_get_chnlist = mesh_wifi_get_channel_list,
    .umesh_hal_get_channel = mesh_wifi_get_channel,
    .umesh_hal_get_extnetid = mesh_wifi_get_extnetid,
    .umesh_hal_set_extnetid = mesh_wifi_set_extnetid,
    .umesh_hal_radio_wakeup = mesh_wifi_radio_wakeup,
    .umesh_hal_radio_sleep = mesh_wifi_radio_sleep,
};

void hal_umesh_register_wifi(hal_wifi_module_t *m)
{
    if (m_wifi != NULL)
        return;

    m_wifi = m;
    hal_umesh_register_module(&mesh_wifi_module);
}

