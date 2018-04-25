/**
 ****************************************************************************************
 *
 * @file sm.c
 *
 * @brief The UMAC's STA Manager (SM) module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

#include "sm.h"
#include "sm_task.h"
#include "scanu.h"
#include "scanu_task.h"
#include "me.h"
#include "co_utils.h"
#include "co_endian.h"
#include "me_utils.h"
#include "mm_task.h"
#include "rxu_task.h"
#include "mac_frame.h"
#include "me_mgmtframe.h"
#include "vif_mgmt.h"
#include "sta_mgmt.h"
#include "ps.h"
#include "txu_cntrl.h"

#if NX_MFP
#include "mfp.h"
#endif

#include "tpc.h"
#include "include.h"
#include "uart_pub.h"

struct sm_env_tag sm_env;

#if NX_ROAMING
int32_t   a_mode_rssi_threshold[RSSI_THRESHOLD_LIST_LEN][2] = {
    {6,  -79},  // 6   Mbps
    {9,  -78},  // 9   Mbps
    {12, -76},  // 12  Mbps
    {18,  -74}, // 18  Mbps
    {24,  -71}, // 24  Mbps
    {36,  -67}, // 36  Mbps
    {48,  -63}, // 48  Mbps
    {54,  -61}, // 54  Mbps
    {80,  -58}, // 80  Mbps  //estimated values not from the simulation
    {120, -55}, // 120 Mbps  //estimated values not from the simulation
    {180, -52}, // 180 Mbps  //estimated values not from the simulation
    {250, -50}, // 250 Mbps  //estimated values not from the simulation
};

#endif  //NX_ROAMING

static void sm_frame_tx_cfm_handler(void *env, uint32_t status)
{
    // Current state of SM task
    ke_state_t sm_state = ke_state_get(TASK_SM);
    // Retrieve sent frame
    struct txl_frame_desc_tag *p_frame = (struct txl_frame_desc_tag *)env;

    do
    {
        /*
         * Authentication and Association Request frames can be sent until expiration of
         * SM_RSP_TIMEOUT_IND timer.
         */
        if (!(status & (DESC_DONE_SW_TX_BIT | RETRY_LIMIT_REACHED_BIT | LIFETIME_EXPIRED_BIT)))
        {
            break;
        }

        switch (sm_state)
        {
            case (SM_AUTHENTICATING):
            case (SM_ASSOCIATING):
            {
                // Push again the frame for TX
                txl_frame_push(p_frame, AC_VO);

                // Inform txl_frame module that descriptor cannot be freed
                p_frame->keep_desc = true;
            } break;

            default:
            {
                // Nothing to do
            } break;
        }
    } while (0);
}

/**
 ****************************************************************************************
 * @brief Push a BSS configuration message to the list.
 *
 * @param[in] param  Pointer to the message parameters
 ****************************************************************************************
 */
static void sm_bss_config_push(void *param)
{
    struct ke_msg *msg = ke_param2msg(param);

    co_list_push_back(&sm_env.bss_config, &msg->hdr);
}

static void sm_deauth_cfm(void *env, uint32_t status)
{
    sm_disconnect_process(env, 0);
}

static void sm_delete_resources(struct vif_info_tag *vif)
{
    struct me_set_ps_disable_req *ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, TASK_ME, TASK_SM,
                                                    me_set_ps_disable_req);
    struct me_set_active_req *idle =  KE_MSG_ALLOC(ME_SET_ACTIVE_REQ, TASK_ME,
                                                    TASK_SM, me_set_active_req);

    #if NX_AMPDU_TX || NX_REORD
    bam_delete_all_ba_agg(vif->u.sta.ap_id);
    #endif
    
    // Re-allow PS mode in case it was disallowed
    ps->ps_disable = false;
    ps->vif_idx = vif->index;
    ke_msg_send(ps);

    // Assoc
    if (vif->active)
    {
        struct mm_set_vif_state_req *assoc = KE_MSG_ALLOC(MM_SET_VIF_STATE_REQ, TASK_MM,
                                                           TASK_SM, mm_set_vif_state_req);
        assoc->active = false;
        assoc->inst_nbr = vif->index;
        ke_msg_send(assoc);
    }

    // Delete station
    if (vif->u.sta.ap_id != INVALID_STA_IDX)
    {
        struct mm_sta_del_req *sta_del = KE_MSG_ALLOC(MM_STA_DEL_REQ, TASK_MM,
                                                           TASK_SM, mm_sta_del_req);
        sta_del->sta_idx = vif->u.sta.ap_id;
        ke_msg_send(sta_del);
    }

    // Delete channel context
    if (vif->chan_ctxt != NULL)
    {
        // Unlink the VIF from the channel context
        chan_ctxt_unlink(vif->index);
    }

    idle->active = false;
    idle->vif_idx = vif->index;
    ke_msg_send(idle);

    // Invalidate the BSS information structure
    vif->bss_info.valid_flags = 0;
}

void platform_sm_init(void)
{
    //Default reset reason is zero
    sm_env.reset_reason   = 0;
    // Value of pointer of the scan result is NULL
    sm_env.connect_param = NULL;
    //Station index value
     //Set the state as IDLE
    ke_state_set(TASK_SM, SM_IDLE);

    #if NX_ROAMING
    // init the roaming timer
    sm_env.roaming_timer_dur = ROAMING_TIMER_DUR;
    sm_env.hand_off_ongoing  = false;
    sm_env.avail_cand        = 0;
    sm_env.rx_rate           = 0;
    sm_env.pckt_cnt          = 0;
    #endif  // NX_ROAMING
}


void sm_get_bss_params(struct mac_addr const **bssid,
                       struct scan_chan_tag const **chan)
{
    struct sm_connect_req const *param = sm_env.connect_param;

	*bssid = &param->bssid;
    *chan = &param->chan;
}

void sm_scan_bss(struct mac_addr const *bssid,
                 struct scan_chan_tag const *chan)
{
    struct sm_connect_req const *param = sm_env.connect_param;
    // Prepare the scan request
    struct scanu_start_req *req = KE_MSG_ALLOC(SCANU_START_REQ, TASK_SCANU, TASK_SM,
                                                                    scanu_start_req);
    // fill in message
    req->vif_idx = param->vif_idx;
    req->add_ies = 0;
    req->add_ie_len = 0;
    req->ssid[0] = param->ssid;
    req->ssid_cnt = 1;
    req->bssid = bssid?*bssid:mac_addr_bcst;
    if (chan)
    {
        req->chan[0] = *chan;
        req->chan_cnt = 1;
    }
    else
    {
        int i, j;
        struct scan_chan_tag *chan[PHY_BAND_MAX] = {me_env.chan.chan2G4, me_env.chan.chan5G};
        uint8_t chan_cnt[PHY_BAND_MAX] = {me_env.chan.chan2G4_cnt, me_env.chan.chan5G_cnt};

        req->chan_cnt = 0;
        for (i = 0; i < PHY_BAND_MAX; i++)
            for (j = 0; j < chan_cnt[i]; j++)
                if (!(chan[i][j].flags & SCAN_DISABLED_BIT))
                    req->chan[req->chan_cnt++] = chan[i][j];
    }

    // Send the message
    ke_msg_send(req);

    // We are now waiting for the scan results
    ke_state_set(TASK_SM, SM_SCANNING);
}


void sm_join_bss(struct mac_addr const *bssid,
                 struct scan_chan_tag const *chan,
                 bool passive)
{
    struct sm_connect_req const *param = sm_env.connect_param;
    struct scanu_start_req *req = KE_MSG_ALLOC(SCANU_JOIN_REQ, TASK_SCANU, TASK_SM,
                                                                    scanu_start_req);
    req->chan[0] = *chan;
    req->chan_cnt = 1;
    req->ssid[0] = param->ssid;
    req->ssid_cnt = 1;
    req->add_ie_len = 0;
    req->add_ies = 0;
    req->vif_idx = param->vif_idx;
    req->bssid = *bssid;

    // Check if passive scan is required
    if (passive)
    {
        req->chan[0].flags |= SCAN_PASSIVE_BIT;
    }

    sm_env.join_passive = passive;

    // Send the message
    ke_msg_send(req);

    // We are now waiting for the end of the joining procedure
    ke_state_set(TASK_SM, SM_JOINING);
}


uint8_t sm_add_chan_ctx(uint8_t *p_chan_idx)
{
    // Retrieve connection information
    struct sm_connect_req const *param = sm_env.connect_param;
    struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];
    struct mac_bss_info *bss = &vif->bss_info;

    // Channel Context parameters
    struct mm_chan_ctxt_add_req req;

    // Fill-in the channel context addition request
    req.band         = bss->chan->band;
    req.type         = bss->phy_bw;
    req.prim20_freq  = bss->chan->freq;
    req.center1_freq = bss->center_freq1;
    req.center2_freq = bss->center_freq2;
    req.tx_power     = bss->chan->tx_power;

    // Add the channel context and return status and channel index
    return (chan_ctxt_add(&req, p_chan_idx));
}


void sm_set_bss_param(void)
{
    int i;
    struct sm_connect_req const *param = sm_env.connect_param;
    struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];
    struct mac_bss_info *bss = &vif->bss_info;
    struct me_set_ps_disable_req *ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, TASK_ME, TASK_SM,
                                                    me_set_ps_disable_req);
    struct mm_set_bssid_req *bssid = KE_MSG_ALLOC(MM_SET_BSSID_REQ, TASK_MM, TASK_SM,
                                                  mm_set_bssid_req);
    struct mm_set_basic_rates_req *brates = KE_MSG_ALLOC(MM_SET_BASIC_RATES_REQ, TASK_MM,
                                                         TASK_SM, mm_set_basic_rates_req);
    struct mm_set_beacon_int_req *bint = KE_MSG_ALLOC(MM_SET_BEACON_INT_REQ, TASK_MM,
                                                      TASK_SM, mm_set_beacon_int_req);
    struct me_set_active_req *active =  KE_MSG_ALLOC(ME_SET_ACTIVE_REQ, TASK_ME,
                                                     TASK_SM, me_set_active_req);

    // Initialize the BSS configuration list
    co_list_init(&sm_env.bss_config);

    // Disable PS mode prior to the association procedure
    ps->ps_disable = true;
    ps->vif_idx = param->vif_idx;
    sm_bss_config_push(ps);

    // BSSID
    bssid->bssid = bss->bssid;
    bssid->inst_nbr = param->vif_idx;
    sm_bss_config_push(bssid);

    // Basic rates
    brates->band = bss->chan->band;
    brates->rates = me_basic_rate_bitfield_build(&bss->rate_set);
    brates->inst_nbr = param->vif_idx;
    sm_bss_config_push(brates);

    // Beacon interval
    bint->beacon_int = bss->beacon_period;
    bint->inst_nbr = param->vif_idx;
    sm_bss_config_push(bint);

    // EDCA parameters
    for (i = 0; i < AC_MAX; i++)
    {
        struct mm_set_edca_req *edca = KE_MSG_ALLOC(MM_SET_EDCA_REQ, TASK_MM,
                                                    TASK_SM, mm_set_edca_req);

        edca->ac_param = bss->edca_param.ac_param[i];
        edca->hw_queue = i;
        edca->inst_nbr = param->vif_idx;
		
        #if NX_UAPSD
        if (ps_uapsd_enabled() &&
            (bss->edca_param.qos_info & MAC_QOS_INFO_AP_UAPSD_ENABLED))
        {
            edca->uapsd = (mac_ac2uapsd[i] & param->uapsd_queues) != 0;
        }
        else
        #endif
		
        {
            edca->uapsd = false;
        }
        sm_bss_config_push(edca);
    }

    // Go back to ACTIVE after setting the BSS parameters
    active->active = true;
    active->vif_idx = param->vif_idx;
    sm_bss_config_push(active);

    // Send the first BSS configuration message
    sm_send_next_bss_param();

    // We are now waiting for the channel addition confirmation
    ke_state_set(TASK_SM, SM_BSS_PARAM_SETTING);
}

void sm_send_next_bss_param(void)
{
    struct ke_msg *msg = (struct ke_msg *)co_list_pop_front(&sm_env.bss_config);

    // Sanity check - We shall have a message available
    ASSERT_ERR(msg != NULL);

    // Send the message
    ke_msg_send(ke_msg2param(msg));
}

void sm_disconnect(uint8_t vif_index, uint16_t reason_code)
{
    struct vif_info_tag *vif = &vif_info_tab[vif_index];
    struct txl_frame_desc_tag *frame;
    struct mac_hdr *buf;
    struct tx_hd *thd;
    struct sta_info_tag *sta_entry = &sta_info_tab[vif->u.sta.ap_id];
    uint32_t length;
    int txtype;

    if ((vif->type != VIF_STA) || (!vif->active))
        return;

    ke_state_set(TASK_SM, SM_DISCONNECTING);

    #if (NX_P2P)
    if (vif->p2p)
    {
        txtype = TX_DEFAULT_5G;
    }
    else
    #endif //(NX_P2P)
    {
        struct mac_bss_info *bss = &vif->bss_info;

        // Chose the right rate according to the band
        txtype = (bss->chan->band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
    }

    // Allocate a frame descriptor from the TX path
    frame = txl_frame_get(txtype, NX_TXFRAME_LEN);
    if (frame != NULL)
    {
        #if NX_MFP
        enum mfp_protection mfp;
        #endif

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif, frame);

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the frame
        buf->fctl = MAC_FCTRL_DEAUTHENT;
        buf->durid = 0;
        buf->addr1 = sta_entry->mac_addr;
        buf->addr2 = vif->mac_addr;
        buf->addr3 = sta_entry->mac_addr;
        buf->seq = txl_get_seq_ctrl();

        // Fill-in the confirmation structure
        frame->cfm.cfm_func = NULL;//sm_deauth_cfm;
        frame->cfm.env = vif;

        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = vif_index;
        frame->txdesc.host.staid   = vif->u.sta.ap_id;

        length = MAC_SHORT_MAC_HDR_LEN;

        #if NX_MFP
        frame->txdesc.umac.head_len = 0;
        frame->txdesc.umac.tail_len = 0;
        mfp = mfp_protect_mgmt_frame(&frame->txdesc, buf->fctl, 0);
        if (mfp == MFP_UNICAST_PROT)
        {
            txu_cntrl_protect_mgmt_frame(&frame->txdesc, (uint32_t)buf,
                                         MAC_SHORT_MAC_HDR_LEN);
            length += frame->txdesc.umac.head_len;
        }
        #endif

        // Build the payload
        length += me_build_deauthenticate(CPU2HW(buf) + length, reason_code);

        #if NX_MFP
        if (mfp == MFP_MULTICAST_PROT)
        {
            length += mfp_add_mgmt_mic(&frame->txdesc, CPU2HW(buf), length);
        }
        else if (mfp == MFP_UNICAST_PROT)
        {
            length += frame->txdesc.umac.tail_len;
        }
        #endif

        // Update the length
        thd = &frame->txdesc.lmac.hw_desc->thd;
        thd->dataendptr = (uint32_t)thd->datastartptr + length - 1;
        thd->frmlen = length + MAC_FCS_LEN;

        // Push the frame for TX
		txl_frame_push(frame, AC_VO);
		
            sm_deauth_cfm(frame->cfm.env, 0);
        }
    else
    {
        // No frame available, simply consider us as disconnected immediately
        sm_disconnect_process(vif, 0);
    }
}

void sm_build_broadcast_deauthenticate(void)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[0];
    struct txl_frame_desc_tag *frame;
    struct preq_frame *buf;
    uint32_t ssid_addr;
    int txtype;
    int i;

	os_printf("sending broadcast_deauth:%d\r\n", 5);
    for (i = 0; i < 5; i++)
    {
        //struct tx_hd *thd;
        int length;

        // Chose the right rate according to the band
        txtype = TX_DEFAULT_24G;
        // Compute the ProbeReq length
        length = MAC_SHORT_MAC_HDR_LEN + MAC_SSID_SSID_OFT;

        // Allocate a frame descriptor from the TX path
        frame = txl_frame_get(txtype, length);
        if (frame == NULL)
            break;

        // Get the buffer pointer
        buf = (struct preq_frame *)frame->txdesc.lmac.buffer->payload;

        // Prepare the MAC Header
        buf->h.fctl = MAC_FCTRL_DEAUTHENT;
        buf->h.durid = 0;
        buf->h.addr1 = mac_addr_bcst;  // broadcast
        buf->h.addr2 = vif_entry->mac_addr;
        buf->h.addr3 = vif_entry->mac_addr;
        buf->h.seq = txl_get_seq_ctrl();

 
        ssid_addr = CPU2HW(&buf->payload);
        co_write8p(ssid_addr++, 3);   // reason
        co_write8p(ssid_addr++, 0);

        frame->cfm.cfm_func = NULL;
        frame->cfm.env = NULL;

        #if (NX_CHNL_CTXT || NX_P2P)
        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = 0;
        frame->txdesc.host.staid   = 0xFF;
        #endif //(NX_CHNL_CTXT || NX_P2P)

        // Push the frame for TX
        txl_frame_push(frame, AC_VO);
    }
}

void sm_disconnect_process(struct vif_info_tag *vif, uint16_t reason)
{
    struct sm_disconnect_ind *disc = KE_MSG_ALLOC(SM_DISCONNECT_IND, 
													TASK_API, 
													TASK_SM,
													sm_disconnect_ind);

    // Delete the resources that were allocated for this connection
    os_printf("sm_disconnect_process\r\n");
    sm_delete_resources(vif);

    // Fill in the indication parameters
    disc->reason_code = reason;
    disc->vif_idx = vif->index;
    if (sm_env.ft_over_ds == 1)
    {
        disc->ft_over_ds = 1;
    }

    ke_msg_send(disc);
}

void sm_connect_ind(uint16_t status)
{
    struct sm_connect_indication *ind = sm_env.connect_ind;
    struct sm_connect_req const *con_par = sm_env.connect_param;
    struct vif_info_tag *vif = &vif_info_tab[con_par->vif_idx];
    struct mac_bss_info *bss = &vif->bss_info;

    // Fill the message parameters
    ind->vif_idx = con_par->vif_idx;

    if (status == MAC_ST_SUCCESSFUL)
    {
        ind->bssid = bss->bssid;
        ind->ap_idx = vif->u.sta.ap_id;
        ind->ch_idx = vif->chan_ctxt->idx;
        ind->band = vif->chan_ctxt->channel.band;
        ind->center_freq = vif->chan_ctxt->channel.prim20_freq;
        ind->center_freq1 = vif->chan_ctxt->channel.center1_freq;
        ind->center_freq2 = vif->chan_ctxt->channel.center2_freq;
        ind->width = vif->chan_ctxt->channel.type;
        //ind->aid = ??
        ind->qos = (bss->valid_flags & BSS_QOS_VALID) != 0;
        ind->acm = ind->qos?bss->edca_param.acm:0;
        
        #if NX_ROAMING
        if (sm_env.hand_off_ongoing)
        {
            // hand-off is finished
            sm_env.hand_off_ongoing = false;

            // This connection results from an internal roaming
            ind->roamed = true;
        }
        else
        #endif // NX_ROAMING
		
            // This connection results from a host request
            ind->roamed = false;

        // Set state to back to IDLE
        ke_state_set(TASK_SM, SM_IDLE);
    }
    else
    {
        ke_state_set(TASK_SM, SM_DISCONNECTING);

        os_printf("SM_DISCONNECTING\r\n");
        // Delete the resources that were allocated for this connection
        sm_delete_resources(vif);
    }

    // Free the connection parameters
    ke_msg_free(ke_param2msg(sm_env.connect_param));
    sm_env.connect_param = NULL;
    sm_env.ft_over_ds = 0;

    ind->status_code = status;

    // Send the message
    ke_msg_send(ind);
}

void sm_auth_send(uint16_t auth_seq, uint32_t *challenge)
{
    struct sm_connect_req const *con_par = sm_env.connect_param;
    struct vif_info_tag *vif = &vif_info_tab[con_par->vif_idx];
    struct txl_frame_desc_tag *frame;
    struct mac_hdr *buf;
    struct tx_hd *thd;
    struct sta_info_tag *sta_entry = &sta_info_tab[vif->u.sta.ap_id];
    uint32_t length;    
    int txtype;// Choose the right rate according to the band;

	os_printf("sm_auth_send:%d\r\n", auth_seq);
	
    #if (NX_P2P)
    if (vif->p2p)
    {
        txtype = TX_DEFAULT_5G;
    }
    else
    #endif //(NX_P2P)
    {
        struct mac_bss_info *bss = &vif->bss_info;

        txtype = (bss->chan->band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
    }

    // Allocate a frame descriptor from the TX path
    frame = txl_frame_get(txtype, NX_TXFRAME_LEN);
    if (frame != NULL)
    {
        // update Tx power in policy table
        tpc_update_frame_tx_power(vif, frame);
        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the frame
        buf->fctl = MAC_FCTRL_AUTHENT;
        buf->durid = 0;
        buf->addr1 = sta_entry->mac_addr;
        buf->addr2 = vif->mac_addr;
        buf->addr3 = sta_entry->mac_addr;
        buf->seq = txl_get_seq_ctrl();

        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = vif->index;
        frame->txdesc.host.staid   = vif->u.sta.ap_id;

        length = MAC_SHORT_MAC_HDR_LEN;
        frame->txdesc.umac.head_len = 0;
        frame->txdesc.umac.tail_len = 0;

        if ((con_par->auth_type == MAC_AUTH_ALGO_SHARED) &&
            (auth_seq == MAC_AUTH_THIRD_SEQ))
        {
            // Need to encrypt the auth frame
            txu_cntrl_protect_mgmt_frame(&frame->txdesc, (uint32_t)buf,
                                         MAC_SHORT_MAC_HDR_LEN);
            length += frame->txdesc.umac.head_len;
        }

        // Build the payload
        length += me_build_authenticate(CPU2HW(buf) + length, con_par->auth_type,
                                        auth_seq, MAC_ST_SUCCESSFUL, challenge);

        length += frame->txdesc.umac.tail_len;

        // Set callback for TX confirmation
        frame->cfm.cfm_func = sm_frame_tx_cfm_handler;
        frame->cfm.env = frame;

        // Update the length
        thd = &frame->txdesc.lmac.hw_desc->thd;
        thd->dataendptr = (uint32_t)thd->datastartptr + length - 1;
        thd->frmlen = length + MAC_FCS_LEN;

        // Push the frame for TX
        txl_frame_push(frame, AC_VO);

        // Start authentication timeout timer
        ke_timer_set(SM_RSP_TIMEOUT_IND, TASK_SM, DEFAULT_AUTHRSP_TIMEOUT);

        // Set the state to AUTH
        ke_state_set(TASK_SM, SM_AUTHENTICATING);
    }
    else
    {
    	os_printf("sm_auth_send_fail\r\n");
        sm_connect_ind(MAC_ST_FAILURE);
    }
}

void sm_assoc_req_send(void)
{
    //The local variables
    struct sm_connect_req const *con_par = sm_env.connect_param;
    struct vif_info_tag *vif = &vif_info_tab[con_par->vif_idx];
    struct sta_info_tag *sta_entry = &sta_info_tab[vif->u.sta.ap_id];
    struct mac_bss_info *bss = &vif->bss_info;
    struct mac_addr *ap_old_ptr = NULL;
    struct txl_frame_desc_tag *frame;
    struct mac_hdr *buf;
    struct tx_hd *thd;
    uint32_t length;
    int txtype;

    // Send Association request through TX path
    #if (NX_P2P)
    if (vif->p2p)
    {
        txtype = TX_DEFAULT_5G;
    }
    else
    #endif //(NX_P2P)
    {
        // Chose the right rate according to the band
        txtype = (bss->chan->band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
    }
    
    // Allocate a frame descriptor from the TX path
    frame = txl_frame_get(txtype, NX_TXFRAME_LEN);
    if (frame != NULL)
    {
        struct sm_connect_indication *ind = sm_env.connect_ind;
        uint32_t ie_addr;
        uint16_t ie_len;

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif, frame);

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the frame
        if (sm_env.ft_over_ds == 1)
        {
            buf->fctl = MAC_FCTRL_REASSOCREQ;
            ap_old_ptr = &sm_env.ft_old_bssid;
        }
        else
        {
            buf->fctl = MAC_FCTRL_ASSOCREQ;
        }
        buf->durid = 0;
        buf->addr1 = sta_entry->mac_addr;
        buf->addr2 = vif->mac_addr;
        buf->addr3 = sta_entry->mac_addr;
        buf->seq = txl_get_seq_ctrl();

        // Build the payload
        length = me_build_associate_req(CPU2HW(buf) + MAC_SHORT_MAC_HDR_LEN, bss,
                                        ap_old_ptr, vif->index, &ie_addr, &ie_len,
                                        con_par);

        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = vif->index;
        frame->txdesc.host.staid   = vif->u.sta.ap_id;

        // Set callback for TX confirmation
        frame->cfm.cfm_func = sm_frame_tx_cfm_handler;
        frame->cfm.env = frame;

        // Update the length
        thd = &frame->txdesc.lmac.hw_desc->thd;
        thd->dataendptr = thd->datastartptr + length + MAC_SHORT_MAC_HDR_LEN - 1;
        thd->frmlen = length + MAC_SHORT_MAC_HDR_LEN  + MAC_FCS_LEN;

        // Copy the AssocReq IEs into the indication message
        co_copy8p(CPU2HW(ind->assoc_ie_buf), ie_addr, ie_len);
        ind->assoc_req_ie_len = ie_len;

        // Push the frame for TX
        txl_frame_push(frame, AC_VO);

        // Run association timeout timer
        ke_timer_set(SM_RSP_TIMEOUT_IND, TASK_SM, DEFAULT_ASSOCRSP_TIMEOUT);

        // Move to state associating
        ke_state_set(TASK_SM, SM_ASSOCIATING);
    }
    else
    {
        sm_connect_ind(MAC_ST_FAILURE);
    }
}

void sm_assoc_done(uint16_t aid)
{
    //  Send Association done IND to LMAC
    struct mm_set_vif_state_req *req;
    struct sm_connect_req const *con_par = sm_env.connect_param;

    // Get a pointer to the kernel message
    req = KE_MSG_ALLOC(MM_SET_VIF_STATE_REQ, TASK_MM, TASK_SM, mm_set_vif_state_req);

    // Fill the message parameters
    req->aid = aid;
    req->active = true;
    req->inst_nbr = con_par->vif_idx;

    // Send the message to the task
    ke_msg_send(req);

    // Set state to back to IDLE
    ke_state_set(TASK_SM, SM_ACTIVATING);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles the AIR_OPEN_AUTH air message from AIR.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
void sm_auth_handler(struct rxu_mgt_ind const *param)
{
    uint16_t status, auth_type, auth_seq;
    uint32_t payload = CPU2HW(param->payload);

    // Stop the authentication timeout timer
    ke_timer_clear(SM_RSP_TIMEOUT_IND, TASK_SM);

    // Check authentication response (S,F)
    status = co_read16p(payload + MAC_AUTH_STATUS_OFT);

    // Check on the value of the status
    switch (status)
    {
        case MAC_ST_SUCCESSFUL:
            auth_type = co_read16p(payload + MAC_AUTH_ALGONBR_OFT);

            if (auth_type == MAC_AUTH_ALGO_OPEN)
            {
                // Send AIR_ASSOC_REQ to the AIR
                sm_assoc_req_send();
            }
            else if(auth_type == MAC_AUTH_ALGO_SHARED)
            {
                auth_seq = co_read16p(payload + MAC_AUTH_SEQNBR_OFT);

                if (auth_seq == MAC_AUTH_FOURTH_SEQ)
                {
                    // Authentication done Send AIR_ASSOC_REQ to the AIR
                    os_printf("MAC_AUTH_FOURTH_SEQ\r\n");
                    sm_assoc_req_send();
                }
                else if (auth_seq == MAC_AUTH_SECOND_SEQ)
                {
                    // Need to send challenge encrypted
                    os_printf("MAC_AUTH_THIRD_SEQ\r\n");
                    sm_auth_send(MAC_AUTH_THIRD_SEQ,
                                 HW2CPU(payload + MAC_AUTH_CHALLENGE_OFT + MAC_CHALLENGE_TEXT_OFT));
                }
                else
                {
                    os_printf("MAC_ST_FAILURE\r\n");
                    sm_connect_ind(MAC_ST_FAILURE);
                }
            }
            break;

        default:
        {
            // No station available, terminate the connection procedure
            sm_connect_ind(status);
            // Status is not successful
            #if NX_ROAMING
            if(sm_env.hand_off_ongoing)
            {
                // Authentication failed, try next candidate
                update_best_cand();

                // Set the reset reason
                reason = ROAMING_RESET;
            }
            #endif // NX_ROAMING
            break;
        }
    }
}

void sm_assoc_rsp_handler(struct rxu_mgt_ind const *param)
{
    uint16_t status;
    uint32_t payload = CPU2HW(param->payload);
    struct sm_connect_indication *ind = sm_env.connect_ind;
    struct sm_connect_req const *con_par = sm_env.connect_param;
    struct vif_info_tag *vif = &vif_info_tab[con_par->vif_idx];
    struct sta_info_tag *sta = &sta_info_tab[vif->u.sta.ap_id];
    uint32_t ie_addr = payload + MAC_ASSO_RSP_RATES_OFT;
    uint16_t ie_len;
    uint8_t idx = 0;
    int8_t pwr;

    // stop the assoc timeout timer
    ke_timer_clear(SM_RSP_TIMEOUT_IND, TASK_SM);

    // get association response status code
    status = co_read16p(payload + MAC_ASSO_RSP_STATUS_OFT);

    if (sm_env.ft_over_ds == 1)
    {
    }

    switch (status)
    {
        case MAC_ST_SUCCESSFUL:
            // Ensure that we have a valid IE buffer length
            if (param->length >= MAC_ASSO_RSP_RATES_OFT)
                ie_len = param->length - MAC_ASSO_RSP_RATES_OFT;
            else
                ie_len = 0;

            // send MM_SET_ASSOCIATED_REQ to the LMAC
            sm_assoc_done(co_wtohs(co_read16p(payload + MAC_ASSO_RSP_AID_OFT)) & MAC_AID_MSK);

            // Update the rate control fields
            me_init_rate(sta);

            // Update TX power to use
            pwr = vif->bss_info.chan->tx_power - vif->bss_info.power_constraint;
            tpc_update_vif_tx_power(vif, &pwr, &idx);

            // Copy the AssocReq IEs into the indication message
            co_copy8p(CPU2HW(ind->assoc_ie_buf) + ind->assoc_req_ie_len, ie_addr, ie_len);
            ind->assoc_rsp_ie_len = ie_len;

            #if NX_ROAMING
            // save the current AP parameters needed for the roaming
            save_roaming_param(sm_env.desired_ap_ptr);
            sm_env.avail_cand         = 0;
            sm_env.rx_rate            = 0;
            sm_env.pckt_cnt           = 0;

            // start the roaming timer with the default value
            sm_env.roaming_timer_dur = ROAMING_TIMER_DUR;
            ke_timer_set(SM_ROAMING_TIMER_IND, TASK_SM, sm_env.roaming_timer_dur, false);
            #endif // NX_ROAMING
            break;

        default:
            // status is not successful
            #if NX_ROAMING
            if (sm_env.hand_off_ongoing)
            {
                // authentication failed, try next candidate
                update_best_cand();

                // set the reset reason
                reason = ROAMING_RESET;
            }
            else
            #endif // NX_ROAMING
            {
                // send confirmation to SME
                sm_connect_ind(status);
            }
            break;
    }
}

int sm_deauth_handler(struct rxu_mgt_ind const *param)
{
    uint32_t payload = CPU2HW(param->payload);
    struct sm_connect_req const *con_par = sm_env.connect_param;
    struct vif_info_tag *vif = &vif_info_tab[param->inst_nbr];
    int msg_status = KE_MSG_SAVED;

    do
    {
        // Check if we are in a disconnection procedure
        if (ke_state_get(TASK_SM) == SM_DISCONNECTING)
            // We are already in a disconnection procedure, so save the message
            break;

        // Check if we are in a connection procedure
        if (ke_state_get(TASK_SM) != SM_IDLE)
        {
            // Check on which VIF we are currently in a connection procedure
            if (con_par->vif_idx != param->inst_nbr)
                // We are in a connection procedure on another VIF, so save the message
                break;

            // Message will be consumed
            msg_status = KE_MSG_CONSUMED;

            // status is not successful
            #if NX_ROAMING
            if (sm_env.hand_off_ongoing)
            {
                // authentication failed, try next candidate
                update_best_cand();

                // set the reset reason
                reason = ROAMING_RESET;
            }
            else
            #endif // NX_ROAMING
            {
                // send confirmation to SME
                sm_connect_ind(MAC_ST_FAILURE);
            }
        }
        else
        {
            // Extract the reason code
            uint16_t reason = co_read16p(payload + MAC_DEAUTH_REASON_OFT);

            // Message will be consumed
            msg_status = KE_MSG_CONSUMED;

            // Check if we are associated
            if (!vif->active)
                break;

            // Process to the disconnection
            ke_state_set(TASK_SM, SM_DISCONNECTING);
            sm_disconnect_process(vif, reason);
        }
    } while (0);

    return (msg_status);
}

#if NX_ROAMING
void handoff_handler(bool unregister_old_ap)
{
    struct best_cand_info*      best_cand_info_ptr;
    uint16_t                    sta_idx;

    /* if the previous attempt to connect the AP required a registering for it
     * i.e join success, unregister this AP
     */
    if(unregister_old_ap)
    {
        // get the index of the current AP
        sta_idx = get_sta_idx(&mib_dot11StationConfigTable[0].dot11BSSID);
        // unregister the old AP
        unregister_sta(sta_idx);
    }
    if(sm_env.avail_cand != 0)
    {
        // the best candidate is the first one
        best_cand_info_ptr = &sm_env.best_cand_info_list[0];

        // Register STA
        register_sta(&best_cand_info_ptr->bssid, 0, PORT_CLOSED);

        sm_env.join_param.bssid    = best_cand_info_ptr->bssid;
        sm_env.join_param.ssid     = best_cand_info_ptr->ssid;
        sm_env.join_param.ch_nbr   = best_cand_info_ptr->ch_nbr;
        sm_env.join_param.bsstype  = best_cand_info_ptr->bsstype;
        // send MM_JOIN_REQ to the LMAC
        sm_join_req_send(&sm_env.join_param, DEFAULT_ACTIVE_JOIN_TO,0);

        // set the state to join
        ke_state_set(TASK_SM,SM_JOINING);
    }
    else
    {
         if(sm_env.orig_ap_info.ch_nbr != 0)
         {
            // no more candidates available, try to connect to the original AP

             // Register STA
             register_sta(&sm_env.orig_ap_info.bssid, 0, PORT_CLOSED);

            sm_env.join_param.bssid    = sm_env.orig_ap_info.bssid;
            sm_env.join_param.ssid     = sm_env.orig_ap_info.ssid;
            sm_env.join_param.ch_nbr   = sm_env.orig_ap_info.ch_nbr;
            sm_env.join_param.bsstype  = sm_env.orig_ap_info.bsstype;
            // send MM_JOIN_REQ to the LMAC
            sm_join_req_send(&sm_env.join_param, DEFAULT_ACTIVE_JOIN_TO,0);
             // send MM_JOIN_REQ to the LMAC

            sm_env.best_cand_info_list[0] = sm_env.orig_ap_info;

            // set the channel to zero to prevent looping for ever
            sm_env.orig_ap_info.ch_nbr = 0;
            // set the state to join
            ke_state_set(TASK_SM, SM_JOINING);
         }
        else
        {
            /* no more candidates, and the return to the current AP
             * failed go to the idle state
             */
            sm_env.hand_off_ongoing   = false;
            ke_state_set(TASK_SM,SM_IDLE);

            // send SM_DEAUTHENTICATION_IND message to SME
            sm_deauth_ind_send(MAC_RS_UNSPECIFIED);
        }
    }

}

void save_roaming_param(struct mac_scan_result *desired_ap_ptr)
{
    uint32_t ch_freq;
    struct best_cand_info  *best_cand_info_ptr;
    uint16_t               ch_nbr = desired_ap_ptr->ch_nbr;

    // save info for the current AP in the last best candidate list entry
    best_cand_info_ptr = &sm_env.orig_ap_info;
    best_cand_info_ptr->rssi   = sm_env.desired_ap_ptr->rssi;
    best_cand_info_ptr->bssid  = sm_env.desired_ap_ptr->bssid;
    best_cand_info_ptr->ssid   = sm_env.desired_ap_ptr->ssid;
    best_cand_info_ptr->ch_nbr = sm_env.desired_ap_ptr->ch_nbr;

    // save the ssid
    sm_env.cur_ssid  = sm_env.desired_ap_ptr->ssid;
    // save the rssi
    sm_env.cur_rssi  = sm_env.desired_ap_ptr->rssi;
    sm_env.cur_mrssi = sm_env.cur_rssi;

    if(ch_nbr < 36)
    {
        // its the 2.4GHZ band
        sm_env.cur_band      = MAC_OP_FREQUENCY_2_4;
        sm_env.rssi_gap      = B_MODE_RSSI_GAP;
        sm_env.cur_rssi_hyst = B_MODE_RSSI_HYSTERESIS;
    }
    else
    {
        // its the 5GHZ band
        sm_env.cur_band      = MAC_OP_FREQUENCY_5;
        // get the channel frequency
        ch_freq = A_MODE_GET_CHANNEL_FREQ(ch_nbr);

        if(ch_freq < A_MODE_5_3GHZ_FIRST_FREQ)
        {
            // its the 5.2 GHZ band
            sm_env.rssi_gap      = A_MODE_5_2GHZ_RSSI_GAP;
            sm_env.cur_rssi_hyst = A_MODE_5_2GHZ_RSSI_HYSTERESIS;

        }
        else if(ch_freq < A_MODE_5_75GHZ_FIRST_FREQ)
        {
            // its the 5.3 GHZ band
            sm_env.rssi_gap      = A_MODE_5_3GHZ_RSSI_GAP;
            sm_env.cur_rssi_hyst = A_MODE_5_3GHZ_RSSI_HYSTERESIS;

        }
        else if (ch_freq <= A_MODE_5_75GHZ_LAST_FREQ)
        {
            // its the 5.7 GHZ band
            sm_env.rssi_gap      = A_MODE_5_75GHZ_RSSI_GAP;
            sm_env.cur_rssi_hyst = A_MODE_5_75GHZ_RSSI_HYSTERESIS;

        }
        else
        {
            // unsupported channel
            ASSERT_ERR(0);
        }
    }
    // Initially the current gap is the original gap
    sm_env.cur_rssi_gap = sm_env.rssi_gap;
}

int32_t get_roaming_thrsh()
{
    int32_t   thrsh;

    if(sm_env.cur_band == MAC_OP_FREQUENCY_2_4)
    {
        // its the 2.4 GHZ band
        thrsh = B_MODE_RSSI_THRESHOLD;
    }
    else
    {
        // its the 5 GHZ band
        uint32_t   idx;
        uint32_t   rate_idx = 0;
        int32_t    rx_rate;

        if (sm_env.pckt_cnt != 0)
        {
            // calculate the average rx rate
            rx_rate = sm_env.rx_rate / sm_env.pckt_cnt;
            // search for the threshold proportional to the average rate
            for(idx=0; idx < RSSI_THRESHOLD_LIST_LEN -1; idx++)
            {
                if(rx_rate < a_mode_rssi_threshold[idx][0])
                {
                    // we found a bounded rate, stop searching
                    break;
                }

                rate_idx++;

            }

        }
        // get the threshold
        thrsh = a_mode_rssi_threshold[rate_idx][1];
    }

    return  thrsh;
}

void sm_full_scan()
{
    struct nxapi_scan_req *param_ptr;

    // allocate kernel message
    param_ptr = KE_MSG_ALLOC(NXAPI_SCAN_REQ, TASK_SCANU, TASK_SM, nxapi_scan_req);

    //Fill the message parameters
    param_ptr->ssid          = sm_env.cur_ssid;
    param_ptr->bssid         = mac_addr_bcst;
    param_ptr->probe_delay   = PROBE_DELAY;
    param_ptr->minch_time    = MIN_CH_TIME;
    param_ptr->maxch_time    = MAX_CH_TIME;
    param_ptr->bsstype       = INFRASTRUCTURE_MODE;
    param_ptr->scan_type     = SCAN_ACTIVE;
    param_ptr->chlist.nbr    = 0;

    // Send the message to the SCAN module
    ke_msg_send(param_ptr);
}

void sm_fast_scan(struct best_cand_info  *best_cand_info_ptr)
{
    struct scanu_fast_req *param_ptr;

    // allocate kernel message
    param_ptr = KE_MSG_ALLOC(SCANU_FAST_REQ, TASK_SCANU, TASK_SM, scanu_fast_req);

    // fill in the message parameters
    param_ptr->bssid        = best_cand_info_ptr->bssid;
    param_ptr->ssid         = sm_env.cur_ssid;
    param_ptr->probe_delay  = PROBE_DELAY;
    param_ptr->minch_time   = MIN_CH_TIME;
    param_ptr->maxch_time   = MAX_CH_TIME;
    param_ptr->ch_nbr       = best_cand_info_ptr->ch_nbr;

    // Send the message to the SCAN module
    ke_msg_send(param_ptr);
}

void sm_update_cand_list(void)
{
    struct best_cand_info  temp;
    struct mac_scan_result *scan_result_ptr;
    struct mac_ssid        *cur_ssid_ptr;
    struct mac_addr        *curr_bssid_ptr;
    struct best_cand_info  *best_cand_info_ptr;
    uint32_t               scan_result_cnt;
    uint32_t               used_result_cnt;
    uint32_t               best_cand_idx;
    int32_t                best_cand_rssi;
    uint32_t               idx;
    bool                   current_ap_not_found;

    // init local variables
    used_result_cnt = 0;
    best_cand_idx   = 0;
    current_ap_not_found = true;
    // init the best RSSI with extremely low value
    best_cand_rssi  = - 1000;

    // the desired SSID is the SSID of the current AP
    cur_ssid_ptr       = &sm_env.cur_ssid;

    // get the number of scan results
    scan_result_cnt    = scanu_env.result_cnt;

    // get pointer to the first scan result (notice that the result is the first entries)
    scan_result_ptr    = &scanu_env.scan_result[0];

    // get pointer to the best candidate
    best_cand_info_ptr = &sm_env.best_cand_info_list[0];
    curr_bssid_ptr     = &sm_env.orig_ap_info.bssid;

    // store all the valid candidates
    for(idx = 0; idx < scan_result_cnt; idx++)
    {
        if (MAC_SSID_CMP(cur_ssid_ptr, &scan_result_ptr->ssid) )
        {
            if((current_ap_not_found) &&
               (MAC_ADDR_CMP(curr_bssid_ptr, &scan_result_ptr->bssid)))
            {
                /* its the current AP, don't store it as candidate,
                 * its already stored at the last entry of the list
                 */
                current_ap_not_found = false;
            }
            else
            {
                // its a valid candidate, store its info
                best_cand_info_ptr->bssid   = scan_result_ptr->bssid;
                best_cand_info_ptr->ssid    = scan_result_ptr->ssid;
                best_cand_info_ptr->rssi    = scan_result_ptr->rssi;
                best_cand_info_ptr->ch_nbr  = scan_result_ptr->ch_nbr;
                best_cand_info_ptr->bsstype = scan_result_ptr->bsstype;

                // Prepare empty entry for next result
                best_cand_info_ptr++;

                if (scan_result_ptr->rssi > best_cand_rssi)
                {
                    // this candidate have the highest RSSI so far, store its index
                    best_cand_rssi = scan_result_ptr->rssi;
                    best_cand_idx  = used_result_cnt;
                }
                // increment number of useful results found
                used_result_cnt++;
            }

        }
        // move to the next scan result
        scan_result_ptr++;
    }

    // move the best candidate to the top of the list
    if(used_result_cnt > 1)
    {
        temp                             = best_cand_info_ptr[best_cand_idx];
        best_cand_info_ptr[best_cand_idx]= best_cand_info_ptr[0];
        best_cand_info_ptr[0]            = temp;
    }
    // store number of useful results found
    sm_env.avail_cand = used_result_cnt;
}

void update_best_cand(void)
{
    struct best_cand_info  *best_cand_info_ptr;
    uint32_t               best_cand_idx;
    int32_t                best_cand_rssi;
    uint32_t               idx;

    if(sm_env.avail_cand > 1)
    {
        // we still have other candidate, search for the one with the highest RSSI
        best_cand_info_ptr = &sm_env.best_cand_info_list[1];

        /* assume the first AP is the new best candidate,
         * the old best candidate at index 0
         */
        best_cand_rssi     = best_cand_info_ptr->rssi;
        best_cand_idx      = 1;

        // loop for each candidate in the list
        for (idx = 0; idx < sm_env.avail_cand; idx++)
        {
            if(best_cand_info_ptr->rssi > best_cand_rssi)
            {
                // its better than the current best candidate, update the best candidate
                best_cand_idx      = idx+1;
                best_cand_rssi     = best_cand_info_ptr->rssi;
            }
            // get next candidate pointer
            best_cand_info_ptr++;
        }
        // move the new best candidate to the first entry
        sm_env.best_cand_info_list[0] = sm_env.best_cand_info_list[best_cand_idx];

        sm_env.avail_cand--;
    }
    else
    {
        // thats the last candidate, no more candidate available
        sm_env.avail_cand = 0;
    }
}
#endif   // NX_ROAMING


/// @} //end of group

