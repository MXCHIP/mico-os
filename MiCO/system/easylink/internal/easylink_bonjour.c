/**
 ******************************************************************************
 * @file    easylink_bonjour.c
 * @author  William Xu
 * @version V1.0.0
 * @date    20-July-2015
 * @brief   This file provide the easylink function for quick provisioning and
 *          first time configuration.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico.h"
#include "mdns.h"

#include "StringUtils.h"
#include "system_internal.h"

#define EASYLINK_BONJOUR_TXT_LEN      (256)

static struct mdns_service easylink_service;
static char keyvals[EASYLINK_BONJOUR_TXT_LEN];
/*   service: model#xxxxxx.   */
static char srv_name[sizeof(MODEL)+9];

static void _bonjour_generate_txt_record( char *txt_record, uint16_t txt_record_len, uint32_t easyLink_id,
                                          WiFi_Interface interface, system_context_t * const inContext )
{
    char *val = NULL;

    val = __strdup_trans_dot( FIRMWARE_REVISION );
    snprintf( txt_record, txt_record_len, "FW=%s.", val );
    free( val );

    val = __strdup_trans_dot( HARDWARE_REVISION );
    snprintf( txt_record, txt_record_len,"%sHD=%s.", txt_record, val );
    free( val );

    val = __strdup_trans_dot( PROTOCOL );
    snprintf( txt_record, txt_record_len, "%sPO=%s.", txt_record, val );
    free( val );

    val = __strdup_trans_dot( inContext->micoStatus.rf_version );
    snprintf( txt_record, txt_record_len, "%sRF=%s.", txt_record, val );
    free( val );

    val = __strdup_trans_dot( inContext->micoStatus.mac );
    snprintf( txt_record, txt_record_len, "%sMAC=%s.", txt_record, val );
    free( val );

    val = __strdup_trans_dot( MicoGetVer( ) );
    snprintf( txt_record, txt_record_len, "%sOS=%s.", txt_record, val );
    free( val );

    val = __strdup_trans_dot( MODEL );
    snprintf( txt_record, txt_record_len, "%sMD=%s.", txt_record, val );
    free( val );

    val = __strdup_trans_dot( MANUFACTURER );
    snprintf( txt_record, txt_record_len, "%sMF=%s.", txt_record, val );
    free( val );

    if ( interface == Soft_AP )
    {
        snprintf( txt_record, txt_record_len, "%swlan unconfigured=T.", txt_record );
        snprintf( txt_record, txt_record_len, "%sFTC=T.", txt_record );
    }
    else
    {
        snprintf( txt_record, txt_record_len, "%swlan unconfigured=F.", txt_record );
#if MICO_CONFIG_SERVER_ENABLE
        snprintf(txt_record, txt_record_len, "%sFTC=T.", txt_record);
#else
        snprintf( txt_record, txt_record_len, "%sFTC=F.", txt_record );
#endif
    }

    snprintf( txt_record, txt_record_len, "%sID=%lx.", txt_record, easyLink_id );
}



OSStatus easylink_bonjour_start( WiFi_Interface interface, uint32_t easyLink_id, system_context_t * const inContext )
{
    uint8_t mac[6];

    mico_wlan_get_mac_address( mac );
    snprintf( srv_name, 32+5, "%s(%02X%02X%02X)", MODEL, mac[3],  mac[4], mac[5] );

    memset(&easylink_service, 0x0, sizeof(struct mdns_service));
    easylink_service.servname = srv_name;
    easylink_service.servtype = "easylink_config";
    easylink_service.port = MICO_CONFIG_SERVER_PORT;
    easylink_service.proto = MDNS_PROTO_TCP;

    _bonjour_generate_txt_record( keyvals, EASYLINK_BONJOUR_TXT_LEN, easyLink_id, interface, inContext );
    mdns_set_txt_rec(&easylink_service, keyvals, '.');

    mdns_start(NULL, DEFAULT_NAME);
    mdns_announce_service(&easylink_service, interface);

    return kNoErr;
}

OSStatus easylink_bonjour_update( WiFi_Interface interface, uint32_t easyLink_id, system_context_t * const inContext )
{
    OSStatus err = kNoErr;

    _bonjour_generate_txt_record( keyvals, EASYLINK_BONJOUR_TXT_LEN, easyLink_id, interface, inContext );
    err = mdns_set_txt_rec(&easylink_service, keyvals, '.');
    require_noerr(err, exit);
    err = mdns_iface_state_change(interface, REANNOUNCE);
    require_noerr(err, exit);

    exit:
    return err;
}

void easylink_remove_bonjour( WiFi_Interface interface )
{
    mdns_deannounce_service(&easylink_service, interface);
}

