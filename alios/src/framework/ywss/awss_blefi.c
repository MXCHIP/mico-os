/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "blefi_config.h"
#include "awss_blefi.h"
#include "os.h"

static bool m_ble_dev_init;
static ble_dev_config m_blefi_dev;

void blefi_dev_config_cb(uint8_t *buf, int len)
{
    ali_blefi_config_handler(buf, len);
}

void blefi_dev_status_set_cb(uint8_t *buf, int len)
{
}

void blefi_dev_status_get_cb(uint8_t *buf, int len)
{
}

void blefi_dev_status_changed(alink_ble_event_t event)
{
}

void blefi_dev_sniffer_cb(uint8_t *buf, int len)
{
}

int alink_ble_init()
{
    if (m_ble_dev_init)
        return 0;

    m_ble_dev_init = 1; 
    memset(&m_blefi_dev, 0, sizeof(m_blefi_dev));
    os_wifi_get_mac(m_blefi_dev.bd_addr);
    os_product_get_model(m_blefi_dev.model);
    os_get_device_secret(m_blefi_dev.secret);
    os_product_get_version(m_blefi_dev.version);
    m_blefi_dev.fmsk.enable_blefi = 1;
    m_blefi_dev.fmsk.blefi_type = BLE_SLAVE;
    m_blefi_dev.fmsk.enable_ota = 0;
    m_blefi_dev.fmsk.enable_auth = 0;
    m_blefi_dev.status_changed_cb = blefi_dev_status_changed;
    m_blefi_dev.set_cb = blefi_dev_status_set_cb;
    m_blefi_dev.get_cb = blefi_dev_status_get_cb;
    m_blefi_dev.config_cb = blefi_dev_config_cb;
    m_blefi_dev.sniffer_cb = blefi_dev_sniffer_cb;

    ali_blefi_config_init();

    os_ble_start(&m_blefi_dev);   

    return 0;
}

void alink_ble_deinit()
{
    memset(&m_blefi_dev, 0, sizeof(m_blefi_dev));
    os_ble_end();
    ali_blefi_config_deinit();
    m_ble_dev_init = 0;
}
