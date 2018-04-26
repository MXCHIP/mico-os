/**
 ****************************************************************************************
 *
 * @file me_task.c
 *
 * @brief The UMAC's SCAN module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

#include "me_task.h"
#include "me.h"

#include "me.h"
#include "mm.h"
#include "me_utils.h"
#include "scan_task.h"
#include "rxu_task.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "ps.h"
#include "co_endian.h"

#include "include.h"
#include "mem_pub.h"
#include "uart_pub.h"
#if CFG_MODE_SWITCH
#include "param_config.h"
#endif
/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_config_req_handler(ke_msg_id_t const msgid,
                      struct me_config_req const *param,
                      ke_task_id_t const dest_id,
                      ke_task_id_t const src_id)
{
#if NX_POWERSAVE
    struct mm_set_ps_mode_req *ps;
#endif

    // Copy the HT and VHT capabilities
    me_env.ht_supported = param->ht_supp;
    me_env.ht_cap = param->ht_cap;
#if NX_VHT
    me_env.vht_supported = param->vht_supp;
    me_env.vht_cap = param->vht_cap;
#endif

    ke_msg_send_basic(ME_CONFIG_CFM, src_id, dest_id);

    // Set the maximum number of NSS supported when using STBC for TX
    if (me_env.ht_supported)
        me_env.stbc_nss = (phy_get_nss() + 1) / 2;
    else
        me_env.stbc_nss = 0;

    // Set the lifetime of packets sent under BA agreement
    me_env.tx_lft = param->tx_lft;

#if NX_POWERSAVE
    // Save PS mode flag
    me_env.ps_on = param->ps_on;

    // Enable PS mode if required
    if (me_env.ps_on)
    {
        ps = KE_MSG_ALLOC(MM_SET_PS_MODE_REQ, TASK_MM,
                                    TASK_ME, mm_set_ps_mode_req);
        
        // No requester ID
        me_env.requester_id = TASK_NONE;

        // Send the message to the LMAC
        ps->new_state = PS_MODE_ON_DYN;
        ke_msg_send(ps);

        // Move to BUSY state
        ke_state_set(TASK_ME, ME_BUSY);
    }
#endif

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_chan_config_req_handler(ke_msg_id_t const msgid,
                           struct me_chan_config_req const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)
{
    // Copy the channel list
    me_env.chan = *param;

    ke_msg_send_basic(ME_CHAN_CONFIG_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_set_control_port_req_handler(ke_msg_id_t const msgid,
                                struct me_set_control_port_req const *param,
                                ke_task_id_t const dest_id,
                                ke_task_id_t const src_id)
{
    struct sta_info_tag *sta = &sta_info_tab[param->sta_idx];
    struct vif_info_tag *p_vif = &vif_info_tab[sta->inst_nbr];

    // Copy the HT and VHT capabilities
    sta->ctrl_port_state = param->control_port_open ? PORT_OPEN : PORT_CONTROLED;

	os_printf("ctrl_port_hdl:%d\r\n", param->control_port_open);
    // If port is open for a STA, reenable the PS mode now that the association is complete
#if (TDLS_ENABLE)
    // do not reenable PS mode in case of association with the TDLS station
    if ((p_vif->type == VIF_STA) && (sta->ctrl_port_state == PORT_OPEN) && (!sta->tdls_sta))
#else
    if ((p_vif->type == VIF_STA) && (sta->ctrl_port_state == PORT_OPEN))
#endif
    {
        struct me_set_ps_disable_req *ps;
		
		ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, 
								TASK_ME, 
								TASK_SM,
								me_set_ps_disable_req);
        ps->ps_disable = false;
        ps->vif_idx = sta->inst_nbr;

        ke_msg_send(ps);
    }

    ke_msg_send_basic(ME_SET_CONTROL_PORT_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}

/**

*******************************************************************************
 * @brief Handle reception of the ME_MGMT_TX_REQ message from the host.
 *        If the ME task state is busy, the message is stored and is handled
once state
 *        goes back to idle.
 *        ME_MGMT_TX_CFM message is sent once request message is handled
during an idle
 *        period.
 *
 *        If request can be handled, procedure will be divided into several
steps:
 *          Step 1 - DMA Download, once over me_dma_cb function is called.
 *          Step 2 - Schedule the channel on which management frame has to be
sent.
 *                   Once we are on the channel, me_chan_ctxt_sched_cfm
function is called.
 *          Step 3 - Push the frame for transmission. As soon as the frame is
confirmed by
 *                   the HW, the me_mgmt_tx_cb function will be called.
 *                   ME_MGMT_TX_DONE_IND message is sent to the host once the
packet has been
 *                   confirmed.
 *
 * @param[in] msgid         Id of the message received
 * @param[in] param         Pointer to the parameters of the message.
 * @param[in] dest_id       TaskId of the receiving task.
 * @param[in] src_id        TaskId of the sending task.
 *
 * @return Whether the message was consumed or not.
*******************************************************************************
 */
static int me_mgmt_tx_req_handler(ke_msg_id_t const msgid,
                       struct me_mgmt_tx_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // Returned message status
    int msg_status;

    // Check current task state
    if (ke_state_get(TASK_ME) == ME_BUSY)
    {
        // Message will be handled once task state will be updated
        msg_status = KE_MSG_SAVED;
    }
    else
    {
        // Request can be handled, allocate the confirmation message
        struct me_mgmt_tx_cfm *cfm = KE_MSG_ALLOC(ME_MGMT_TX_CFM,
                                     src_id,
                                     dest_id,
                                     me_mgmt_tx_cfm);

        // Fill the parameters
#if CFG_WIFI_AP_MODE
#if CFG_MODE_SWITCH
		if(g_wlan_general_param->role == CONFIG_ROLE_AP)
#endif
        {
            uint16_t *framectrl;

            cfm->hostid = (uint32_t)os_malloc(param->len);
            cfm->length = param->len;
            ASSERT(cfm->hostid);

            os_memcpy((void *)cfm->hostid, (void *)param->hostid, param->len);
            framectrl = (uint16_t *)param->hostid;
            *framectrl &= ~(BIT(1));// wangzhilei callback
        }
#else
#if CFG_MODE_SWITCH
		if(g_wlan_general_param->role == CONFIG_ROLE_STA)
#endif
        cfm->hostid = param->hostid;
#endif

        cfm->status = me_mgmt_tx(param);

        // If the TX status is OK, keep the request message during the procedure, else consume it
        msg_status = (cfm->status == CO_OK) ? KE_MSG_NO_FREE : KE_MSG_CONSUMED;

#if CFG_WIFI_AP_MODE
#if CFG_MODE_SWITCH
		if(g_wlan_general_param->role == CONFIG_ROLE_AP)
#endif
        if(CO_OK == cfm->status)
        {
            extern int ke_mgmt_packet_tx(unsigned char *, int, int);
            
            ke_mgmt_packet_tx((unsigned char *)cfm->hostid, cfm->length, 0);
            os_free((void *)cfm->hostid);
        }
        else
        {
            os_free((void *)cfm->hostid);
            cfm->hostid = 0;
        }
#endif

        // Send the confirmation message
        ke_msg_send(cfm);
    }

    return (msg_status);
}

/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_sta_add_req_handler(ke_msg_id_t const msgid,
                       struct me_sta_add_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    struct mm_sta_add_req sta_add_req;
    struct me_sta_add_cfm *rsp = KE_MSG_ALLOC(ME_STA_ADD_CFM, src_id, dest_id, me_sta_add_cfm);
    uint16_t ampdu_size_max_ht = 0;
    uint32_t ampdu_size_max_vht = 0;
    uint8_t ampdu_spacing_min = 0;
    uint8_t hw_sta_idx;
    uint8_t pm_state = rxu_cntrl_get_pm();

#if (TDLS_ENABLE)
    // In case of TDLS station disable PS
    if (param->tdls_sta)
    {
        struct me_set_ps_disable_req *ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, TASK_ME, TASK_SM,
                                           me_set_ps_disable_req);
        ps->ps_disable = true;
        ps->vif_idx = param->vif_idx;

        ke_msg_send(ps);
    }
#endif

    // Compute HT and VHT A-MPDU parameters
    if (param->flags & STA_HT_CAPA)
    {
        int ampdu_len_exp = (param->ht_cap.a_mpdu_param & MAC_AMPDU_LEN_EXP_MSK) >>
                            MAC_AMPDU_LEN_EXP_OFT;
        int ampdu_min_spc = (param->ht_cap.a_mpdu_param & MAC_AMPDU_MIN_SPACING_MSK) >>
                            MAC_AMPDU_MIN_SPACING_OFT;

#if NX_VHT
        if (param->flags & STA_VHT_CAPA)
        {
            int vht_exp = (param->vht_cap.vht_capa_info &
                           MAC_VHTCAPA_MAX_A_MPDU_LENGTH_EXP_MSK) >>
                          MAC_VHTCAPA_MAX_A_MPDU_LENGTH_EXP_OFT;
            ampdu_size_max_vht = (1 << (MAC_HT_MAX_AMPDU_FACTOR + vht_exp)) - 1;
        }
#endif
        ampdu_spacing_min = (ampdu_min_spc < 3) ? 1 : (0x01 << (ampdu_min_spc - 3));
        ampdu_size_max_ht = (0x01 << (13 + ampdu_len_exp)) - 1;
    }

    sta_add_req.mac_addr = param->mac_addr;
    sta_add_req.ampdu_size_max_ht = ampdu_size_max_ht;
    sta_add_req.ampdu_size_max_vht = ampdu_size_max_vht;
    sta_add_req.ampdu_spacing_min = ampdu_spacing_min;
    sta_add_req.inst_nbr = param->vif_idx;
#if (TDLS_ENABLE)
    sta_add_req.tdls_sta = param->tdls_sta;
#endif

    // Register the new station
    rsp->status = mm_sta_add(&sta_add_req, &rsp->sta_idx, &hw_sta_idx);

    // If station was successfully allocated
    if (rsp->status == CO_OK)
    {
        struct sta_info_tag *sta = &sta_info_tab[rsp->sta_idx];
        struct vif_info_tag *vif = &vif_info_tab[sta->inst_nbr];
        struct mac_sta_info *info = &sta->info;
        struct mac_bss_info *bss = &vif->bss_info;
        bool smps = false;
        // Initialize the capabilities of the new STA
        info->rate_set = param->rate_set;
        if (param->flags & STA_QOS_CAPA)
        {
            info->capa_flags |= STA_QOS_CAPA;
        }

        if (param->flags & STA_HT_CAPA)
        {
            uint8_t local_supp_bw = BW_20MHZ;
            uint8_t peer_supp_bw = BW_20MHZ;
            uint8_t stbc_nss;

            info->capa_flags |= STA_HT_CAPA;
            info->ht_cap = param->ht_cap;
#if NX_VHT
            if (param->flags & STA_VHT_CAPA)
            {
                info->capa_flags |= STA_VHT_CAPA;
                info->vht_cap = param->vht_cap;
                local_supp_bw = bss->bw;
                peer_supp_bw = BW_80MHZ;
                stbc_nss = (info->vht_cap.vht_capa_info & MAC_VHTCAPA_RXSTBC_MSK) >>
                           MAC_VHTCAPA_RXSTBC_OFT;
                stbc_nss = (stbc_nss <= 4) ? stbc_nss : 0;
            }
            else
#endif
            {
                stbc_nss = (info->ht_cap.ht_capa_info & MAC_HTCAPA_RX_STBC_MSK) >>
                           MAC_HTCAPA_RX_STBC_OFT;

                // Check our local supported BW
                if (me_env.ht_cap.ht_capa_info & MAC_HTCAPA_40_MHZ)
                    local_supp_bw = BW_40MHZ;

                if (info->ht_cap.ht_capa_info & MAC_HTCAPA_40_MHZ)
                    peer_supp_bw = BW_40MHZ;
            }

            info->bw_max = co_min(local_supp_bw, peer_supp_bw);
            info->bw_cur = co_min(info->bw_max, bss->bw);
            info->stbc_nss = co_min(stbc_nss, me_env.stbc_nss);

            if ((info->ht_cap.ht_capa_info & MAC_HTCAPA_SMPS_MSK) !=
                    MAC_HTCAPA_SMPS_DISABLE)
                smps = true;
        }

#if NX_MFP
        if (param->flags & STA_MFP_CAPA)
        {
            info->capa_flags |= STA_MFP_CAPA;
        }
#endif

        // Initialize U-APSD parameters
        info->uapsd_queues = param->uapsd_queues;
        sta->aid = param->aid;

        // Initialize the RC for this STA
        me_init_rate(sta);

        // Check if STA has notified operation mode in its AssocReq
        if ((param->flags & STA_OPMOD_NOTIF) && !(param->opmode & MAC_OPMODE_RXNSS_TYPE_BIT))
        {
            uint8_t bw = (param->opmode & MAC_OPMODE_BW_MSK) >> MAC_OPMODE_BW_OFT;
            uint8_t nss = (param->opmode & MAC_OPMODE_RXNSS_MSK) >> MAC_OPMODE_RXNSS_OFT;

            // Update maximum supported bandwidth
            me_sta_bw_nss_max_upd(sta->staid, bw, nss);
        }

        // Check if STA is using SMPS
        if (smps)
        {
            // Allow only 1 SS to be transmitted
            me_sta_bw_nss_max_upd(sta->staid, 0xFF, 0);
        }

        // Initialize tx power on first transmit
        sta->pol_tbl.upd_field |= CO_BIT(STA_MGMT_POL_UPD_TX_POWER);

        // Init port status
        sta->ctrl_port_state = (vif->flags & CONTROL_PORT_HOST) ? PORT_CONTROLED : PORT_OPEN;
        sta->ctrl_port_ethertype = co_ntohs(vif->u.ap.ctrl_port_ethertype);

        // Get the PM state of the station in case it was catched by the PM monitor
        rsp->pm_state = pm_state;

        if (rsp->pm_state)
        {
            // Set the PM state of the station
            sta->ps_state = pm_state;

            // Update the number of PS stations
            if (!vif->u.ap.ps_sta_cnt)
            {
                mm_ps_change_ind(VIF_TO_BCMC_IDX(vif->index), PS_MODE_ON);
            }

            // Update the number of PS stations
            vif->u.ap.ps_sta_cnt++;

#if (NX_P2P_GO)
            p2p_go_ps_state_update(vif);
#endif //(NX_P2P_GO)
        }
    }

    // Send the confirmation
    ke_msg_send(rsp);

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_sta_del_req_handler(ke_msg_id_t const msgid,
                       struct me_sta_del_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    struct mm_sta_del_req *sta_del = KE_MSG_ALLOC(MM_STA_DEL_REQ, TASK_MM,
                                     TASK_ME, mm_sta_del_req);
#if NX_AMPDU_TX || NX_REORD
    // Release all the Block Ack agreement
    bam_delete_all_ba_agg(param->sta_idx);
#endif

    // Send the message to the LowerMAC
    sta_del->sta_idx = param->sta_idx;
    ke_msg_send(sta_del);

    // In case of TDLS station enable PS
    if (param->tdls_sta)
    {
        struct me_set_ps_disable_req *ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, TASK_ME, TASK_SM,
                                           me_set_ps_disable_req);
        ps->ps_disable = false;
        ps->vif_idx = sta_mgmt_get_vif_idx(param->sta_idx);
        ke_msg_send(ps);
    }

    // Send the confirmation to the upper MAC
    ke_msg_send_basic(ME_STA_DEL_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_uapsd_traffic_ind_req_handler(ke_msg_id_t const msgid,
                                 struct me_uapsd_traffic_ind_req const *param,
                                 ke_task_id_t const dest_id,
                                 ke_task_id_t const src_id)
{
    struct sta_info_tag *sta = &sta_info_tab[param->sta_idx];

    // Update the traffic flag in the STA entry
    sta->uapsd_traffic_avail = param->tx_avail;

    if (sta->info.uapsd_queues == MAC_QOS_INFO_STA_UAPSD_ENABLED_ALL)
    {
        struct mm_tim_update_req *tim = KE_MSG_ALLOC(MM_TIM_UPDATE_REQ, TASK_MM,
                                        TASK_ME, mm_tim_update_req);

        tim->aid = sta->aid;
        tim->inst_nbr = sta->inst_nbr;
        tim->tx_avail = param->tx_avail;

        ke_msg_send(tim);
    }

    // Send the confirmation to the upper MAC
    ke_msg_send_basic(ME_UAPSD_TRAFFIC_IND_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_set_active_req_handler(ke_msg_id_t const msgid,
                          struct me_set_active_req const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Check if the request can be handled immediately
    if (ke_state_get(TASK_ME) == ME_BUSY)
        return (KE_MSG_SAVED);

    // Check if the current state is already OK
    if ((me_env.active_vifs && param->active) ||
            (!me_env.active_vifs && !param->active))
    {
        // Update the bit corresponding to the VIF
        if (param->active)
            me_env.active_vifs |= CO_BIT(param->vif_idx);

        // Send the confirmation immediately
        ke_msg_send_basic(ME_SET_ACTIVE_CFM, src_id, dest_id);
    }
    else
    {
        struct mm_set_idle_req *active =  KE_MSG_ALLOC(MM_SET_IDLE_REQ, TASK_MM,
                                          dest_id, mm_set_idle_req);

        // Update the bit corresponding to the VIF
        if (param->active)
            me_env.active_vifs |= CO_BIT(param->vif_idx);
        else
            me_env.active_vifs &= ~CO_BIT(param->vif_idx);

        // Save the ID of the requester
        me_env.requester_id = src_id;

        // Send the message to the LMAC
        active->hw_idle = (me_env.active_vifs == 0);
        ke_msg_send(active);

        // Move to BUSY state
        ke_state_set(dest_id, ME_BUSY);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles scan confirm from the scan module.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_idle_cfm_handler(ke_msg_id_t const msgid,
                        void const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the BUSY state
    ASSERT_ERR(ke_state_get(dest_id) == ME_BUSY);

    // Send the confirmation to the requester
    if (me_env.requester_id != TASK_NONE)
        ke_msg_send_basic(ME_SET_ACTIVE_CFM, me_env.requester_id, dest_id);

    // Back to IDLE state
    ke_state_set(dest_id, ME_IDLE);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the FAST_SCAN_REQ message from ROAMING (SM), DFS (), 20/40 ().
 * This message requests the UMAC to perform scan on a single channel only.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_set_ps_disable_req_handler(ke_msg_id_t const msgid,
                              struct me_set_ps_disable_req const *param,
                              ke_task_id_t const dest_id,
                              ke_task_id_t const src_id)
{
	
#if NX_POWERSAVE
    // Check if PS is enabled or not
    if (!me_env.ps_on)
    {
        // Send the confirmation immediately
        ke_msg_send_basic(ME_SET_PS_DISABLE_CFM, src_id, dest_id);

        return (KE_MSG_CONSUMED);
    }

    // Check if the request can be handled immediately
    if (ke_state_get(TASK_ME) == ME_BUSY)
        return (KE_MSG_SAVED);

    // Check if the current state is already OK
    if ((me_env.ps_disable_vifs && param->ps_disable) ||
            (!me_env.ps_disable_vifs && !param->ps_disable))
    {
        // Update the bit corresponding to the VIF
        if (param->ps_disable)
            me_env.ps_disable_vifs |= CO_BIT(param->vif_idx);

        // Send the confirmation immediately
        ke_msg_send_basic(ME_SET_PS_DISABLE_CFM, src_id, dest_id);
    }
    else
    {
        struct mm_set_ps_mode_req *ps = KE_MSG_ALLOC(MM_SET_PS_MODE_REQ, TASK_MM,
                                        dest_id, mm_set_ps_mode_req);

        // Update the bit corresponding to the VIF
        if (param->ps_disable)
            me_env.ps_disable_vifs |= CO_BIT(param->vif_idx);
        else
            me_env.ps_disable_vifs &= ~CO_BIT(param->vif_idx);

        // Save the ID of the requester
        me_env.requester_id = src_id;

        // Send the message to the LMAC
        ps->new_state = (me_env.ps_disable_vifs == 0) ? PS_MODE_ON_DYN : PS_MODE_OFF;
        ke_msg_send(ps);

        // Move to BUSY state
        ke_state_set(dest_id, ME_BUSY);
    }
#else
    // Send the confirmation immediately
    ke_msg_send_basic(ME_SET_PS_DISABLE_CFM, src_id, dest_id);
#endif

    return (KE_MSG_CONSUMED);
}

#if NX_POWERSAVE
/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles scan confirm from the scan module.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_ps_mode_cfm_handler(ke_msg_id_t const msgid,
                           void const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the BUSY state
    ASSERT_ERR(ke_state_get(dest_id) == ME_BUSY);

    // Send the confirmation to the requester
    if (me_env.requester_id != TASK_NONE)
        ke_msg_send_basic(ME_SET_PS_DISABLE_CFM, me_env.requester_id, dest_id);

    // Back to IDLE state
    ke_state_set(dest_id, ME_IDLE);

    return (KE_MSG_CONSUMED);
}
#endif

#if RC_ENABLE
/**
 ****************************************************************************************
 * @brief RC module message handler.
 * This function ...
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_rc_stats_req_handler(ke_msg_id_t const msgid,
                        struct me_rc_stats_req const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    // Allocate the response structure
    struct me_rc_stats_cfm *rsp = KE_MSG_ALLOC(ME_RC_STATS_CFM, src_id, dest_id, me_rc_stats_cfm);

    // retrieve statistics for the requested station (number sta_idx)
    struct sta_info_tag *sta = &sta_info_tab[param->sta_idx];
    struct sta_pol_tbl_cntl *rc = &sta->pol_tbl;
    struct rc_sta_stats *rc_ss = rc->sta_stats;
    uint16_t i;

    // set station index
    rsp->sta_idx = param->sta_idx;
    if (rc_ss)
    {
        // set number of samples
        rsp->no_samples = rc_ss->no_samples;
        // set number of MPDUs transmitted (per sampling interval)
        rsp->ampdu_len = rc_ss->ampdu_len;
        // set number of AMPDUs transmitted (per sampling interval)
        rsp->ampdu_packets = rc_ss->ampdu_packets;
        // set average number of MPDUs in each AMPDU frame (EWMA)
        rsp->avg_ampdu_len = rc_ss->avg_ampdu_len;
        // set step 0 of the retry chain
        rsp->sw_retry_step = rc_ss->sw_retry_step;
        // set current value of the trial TX countdown
        rsp->sample_wait = rc_ss->sample_wait;
        // copy the retry chain steps
        memcpy(&rsp->retry[0], &rc_ss->retry[0], sizeof(rsp->retry));
        // copy the statistics
        memcpy(&rsp->rate_stats[0], &rc_ss->rate_stats[0], sizeof(rsp->rate_stats));
        // calculate TP
        for (i = 0; i < rc_ss->no_samples; i++)
        {
            rsp->tp[i] = rc_calc_tp(rc_ss, i);
        }
    }
    else
    {
        rsp->no_samples = 0;
    }

    // Send the confirmation
    ke_msg_send(rsp);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief RC module message handler.
 * This function sets fixed_rate_cfg in the RC structure.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_rc_set_rate_req_handler(ke_msg_id_t const msgid,
                           struct me_rc_set_rate_req const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[param->sta_idx];
    struct sta_pol_tbl_cntl *pt = &sta_entry->pol_tbl;
    struct rc_sta_stats *rc_ss = pt->sta_stats;
    ASSERT_ERR(rc_ss != NULL);
    uint16_t fixed_rate_cfg = param->fixed_rate_cfg;

    if (fixed_rate_cfg != RC_FIXED_RATE_NOT_SET)
    {
        if (rc_check_fixed_rate_config(rc_ss, fixed_rate_cfg))
        {
            // Set the fixed rate configuration in the RC structure
            rc_ss->fixed_rate_cfg = fixed_rate_cfg;
            // Update the fixed rate request flag
            rc_ss->info &= ~RC_FIX_RATE_STATUS_MASK;
            rc_ss->info |= RC_FIX_RATE_REQ_MASK;
        }
    }
    else
    {
        // Set the fixed rate configuration in the RC structure
        rc_ss->fixed_rate_cfg = fixed_rate_cfg;
        rc_ss->info &= ~RC_FIX_RATE_STATUS_MASK;
        // Apply any BW and NSS changes that may have occurred during the fixed rate
        // time
        rc_update_bw_nss_max(sta_entry->staid, rc_ss->bw_max, rc_ss->no_ss);
    }

    return (KE_MSG_CONSUMED);
}
#endif

/// DEFAULT handler definition.
const struct ke_msg_handler me_default_state[] =
{
    {ME_CONFIG_REQ, (ke_msg_func_t)me_config_req_handler},
    {ME_CHAN_CONFIG_REQ, (ke_msg_func_t)me_chan_config_req_handler},
    {ME_SET_CONTROL_PORT_REQ, (ke_msg_func_t)me_set_control_port_req_handler},
    {ME_MGMT_TX_REQ, (ke_msg_func_t)me_mgmt_tx_req_handler},

    {ME_STA_ADD_REQ, (ke_msg_func_t)me_sta_add_req_handler},
    {ME_STA_DEL_REQ, (ke_msg_func_t)me_sta_del_req_handler},

    {ME_SET_ACTIVE_REQ, (ke_msg_func_t)me_set_active_req_handler},
    {ME_SET_PS_DISABLE_REQ, (ke_msg_func_t)me_set_ps_disable_req_handler},
    {ME_UAPSD_TRAFFIC_IND_REQ, (ke_msg_func_t)me_uapsd_traffic_ind_req_handler},
    {MM_SET_IDLE_CFM, (ke_msg_func_t)mm_set_idle_cfm_handler},

#if NX_POWERSAVE
    {MM_SET_PS_MODE_CFM, (ke_msg_func_t)mm_set_ps_mode_cfm_handler},
#endif

    {MM_STA_DEL_CFM, ke_msg_discard},
    {MM_CHAN_CTXT_UPDATE_CFM, ke_msg_discard},
    {MM_TIM_UPDATE_CFM, ke_msg_discard},

#if RC_ENABLE
    {ME_RC_STATS_REQ, (ke_msg_func_t)me_rc_stats_req_handler},
    {ME_RC_SET_RATE_REQ, (ke_msg_func_t)me_rc_set_rate_req_handler},
#endif
};


/// Specifies the message handlers that are common to all states.
const struct ke_state_handler me_default_handler = KE_STATE_HANDLER(me_default_state);


/// Defines the place holder for the states of all the task instances.
ke_state_t me_state[ME_IDX_MAX];


/// @} end of group
