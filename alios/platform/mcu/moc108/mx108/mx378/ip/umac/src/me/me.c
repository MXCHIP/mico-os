/**
 ****************************************************************************************
 *
 * @file me.c
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 * @brief Definition of initialization functions for the UMAC ME modules.
 *
 ****************************************************************************************
 */

/** @addtogroup ME_INIT
* @{
*/

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h"
#include "co_int.h"
#include "co_bool.h"

#include "me.h"
#include "sm.h"
#include "scanu.h"
#include "bam.h"
#include "vif_mgmt.h"
#include "ps.h"

#if CFG_WIFI_AP_MODE
#include "apm.h"
#endif

#include "version.h"
#include "reg_mac_core.h"
#include "txl_frame.h"

#include "mem_pub.h"
#if CFG_MODE_SWITCH
#include "param_config.h"
#endif
/**
 * PRIVATE VARIABLES
 ****************************************************************************************
 */

struct me_env_tag me_env;

/**
 * PRIVATE FUNCTIONS DEFINITION
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Initialize content of the ME environment. State of the ME task is set to idle.
 ****************************************************************************************
 */
static void me_env_init(void)
{
    // Reset the ME environment
    memset(&me_env, 0, sizeof(me_env));

    // Set the ME task state
    ke_state_set(TASK_ME, ME_IDLE);
}

/**
 * PUBLIC FUNCTIONS DEFINITION
 ****************************************************************************************
 */

void me_init(void)
{
    // Initialize the environment
    me_env_init();

    // Initialize scan module
    scanu_init();

    // initialize all ME components modules and environment
#if CFG_WIFI_AP_MODE
    // reset AP manager
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP)
#endif
    apm_init();
#endif

#if CFG_WIFI_STATION_MODE
    // reset STA manager
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_STA)
#endif
    platform_sm_init();
#endif

    // Reset the Block ACK manager
    bam_init();

#if (RW_MESH_EN)
    mesh_init();
#endif //(RW_MESH_EN)
}

struct scan_chan_tag *me_freq_to_chan_ptr(uint8_t band, uint16_t freq)
{
    int i, chan_cnt;
    struct scan_chan_tag *chan;

    // Get the correct channel table
    chan = (band == PHY_BAND_2G4) ? me_env.chan.chan2G4 : me_env.chan.chan5G;
    chan_cnt = (band == PHY_BAND_2G4) ? me_env.chan.chan2G4_cnt : me_env.chan.chan5G_cnt;

    for (i = 0; i < chan_cnt; i++)
    {
        if (chan[i].freq == freq)
            return &chan[i];
    }

    return NULL;
}

uint8_t me_mgmt_tx(struct me_mgmt_tx_req const *param)
{
    int txtype;
    uint8_t band;
    struct mac_hdr *buf;
    struct txl_frame_desc_tag *frame;
    uint8_t status = CO_NO_MORE_ELT_AVAILABLE;
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_idx];

    do
    {
        band = vif_entry->chan_ctxt->channel.band;        
        txtype = (param->no_cck || (band == PHY_BAND_5G)) ? TX_DEFAULT_5G : TX_DEFAULT_24G;

        frame = txl_frame_get(txtype, param->len);
        if (frame == NULL)
            break;

        // Get the buffer pointer
#if NX_AMSDU_TX
        buf = (struct mac_hdr *)CPU2HW(frame->txdesc.lmac.buffer[0]->payload);
#else
        buf = (struct mac_hdr *)CPU2HW(frame->txdesc.lmac.buffer->payload);
#endif

        os_memcpy((void *)buf, (void *)param->addr, param->len);
        buf->seq = txl_get_seq_ctrl();

        txl_frame_push(frame, AC_VO);

#if CFG_WIFI_AP_MODE
#if CFG_MODE_SWITCH
		if(g_wlan_general_param->role == CONFIG_ROLE_AP)
#endif
        if(param->req_frm_new)
        {
            os_free((void *)param->addr);
        }
#endif

        ke_state_set(TASK_ME, ME_IDLE);

        status = CO_OK;
    }
    while (0);

    return (status);
}

/// @}

