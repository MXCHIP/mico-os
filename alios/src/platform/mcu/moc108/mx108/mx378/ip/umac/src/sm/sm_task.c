/**
 ****************************************************************************************
 *
 * @file sm_task.c
 *
 * @brief The UMAC's STA Manager (SM) module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

#include "sm_task.h"
#include "co_endian.h"
#include "scanu_task.h"
#include "scanu.h"
#include "sm.h"
#include "me_utils.h"
#include "mac_frame.h"

#include "ke_timer.h"
#include "mac.h"
#include "mm_task.h"
#include "me.h"
#include "me_mgmtframe.h"
#include "mm.h"
#include "vif_mgmt.h"
#include "sta_mgmt.h"
#include "rxu_task.h"
#include "ps.h"

/** @addtogroup TASK_SM
* @{
*/


/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles the message from SME.
 * This message Requests the UMAC to join and associate to a specific AP or IBSS STA.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */

#include "include.h"
#include "arm_arch.h"
#include "uart_pub.h"

static int
sm_connect_req_handler(ke_msg_id_t const msgid,
                       struct sm_connect_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    struct mac_addr const *bssid = NULL;
    struct scan_chan_tag const *chan = NULL;
    uint8_t status = CO_BUSY;
    int msg_status = KE_MSG_CONSUMED;
    struct sm_connect_cfm *cfm;

    do
    {
        struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];

        if (ke_state_get(TASK_SM) == SM_DISCONNECTING)
            return (KE_MSG_SAVED);

        // Allocate the confirmation message
        cfm = KE_MSG_ALLOC(SM_CONNECT_CFM, src_id, dest_id, sm_connect_cfm);

        // Check if we are not busy
        if (ke_state_get(TASK_SM) != SM_IDLE)
        {
            status = CO_BUSY;
            break;
        }

        // Check if we are not already connected
        if (((vif->type != VIF_STA) || (vif->active)) && (param->auth_type != MAC_AUTH_ALGO_FT))
        {
            status = CO_OP_IN_PROGRESS;
            break;
        }

        if (param->auth_type != MAC_AUTH_ALGO_FT)
        {
            // Sanity check - If we are not associated, we should not have a AP STA index
            // registered neither a channel context
            ASSERT_ERR(vif->u.sta.ap_id == INVALID_STA_IDX);
            ASSERT_ERR(vif->chan_ctxt == NULL);
        }

        // Save the parameters
        sm_env.connect_param = param;

        // Allocate the kernel message for the connection status forwarding
        sm_env.connect_ind = KE_MSG_ALLOC(SM_CONNECT_IND, 
									        src_id, 
									        dest_id, 
									        sm_connect_indication);

        // Reset ft_over_ds flag
        sm_env.ft_over_ds = 0;

        // Check if it's a FT over DS request
        if (param->auth_type == MAC_AUTH_ALGO_FT)
        {
            sm_env.ft_over_ds = 1;
            memcpy(&sm_env.ft_old_bssid, &vif_info_tab[param->vif_idx].bssid, sizeof(sm_env.ft_old_bssid));
            ke_state_set(TASK_SM, SM_DISCONNECTING);
            sm_disconnect_process(&vif_info_tab[param->vif_idx], 0);
            status = CO_OK;
            msg_status = KE_MSG_NO_FREE;
            break;
        }

        // Get the BSSID and Channel from the parameter and/or the scan results
        sm_get_bss_params(&bssid, &chan);

        // Check if we have enough information to launch the join or if we have to scan
        if (bssid && chan)
        {
            sm_join_bss(bssid, chan, false);
        }
        else
        {
            sm_scan_bss(bssid, chan);
        }

        // We will now proceed to the connection, so the status is OK
        status = CO_OK;
        msg_status = KE_MSG_NO_FREE;
    } while(0);

    // Send the confirmation
    cfm->status = status;
    ke_msg_send(cfm);

    return (msg_status);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles the SM_DEAUTHENTICATION_REQ message from SME.
 * This message Requests the UMAC to disassociate from a specific AP
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
sm_disconnect_req_handler(ke_msg_id_t const msgid,
                          struct sm_disconnect_req const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Check if we are in an association process
    if (ke_state_get(TASK_SM) != SM_IDLE)
        return (KE_MSG_SAVED);

    // Check on the value of the BSS type
    sm_disconnect(param->vif_idx, param->reason_code);

    ke_msg_send_basic(SM_DISCONNECT_CFM, TASK_API, TASK_SM);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles the SM_SYNCLOST_IND_TMR message from MM.
 * This message indicates that network connection with the associated AP has been lost.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_connection_loss_ind_handler(ke_msg_id_t const msgid,
                               struct mm_connection_loss_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
    struct vif_info_tag *vif = &vif_info_tab[param->inst_nbr];

    // Check if we are in a disconnection procedure
    if (ke_state_get(TASK_SM) != SM_IDLE)
        // We are already in a procedure, so save the message
        return KE_MSG_SAVED;

    if ((vif->type != VIF_STA) || (!vif->active))
        return KE_MSG_CONSUMED;

    ke_state_set(TASK_SM, SM_DISCONNECTING);

    // Proceed to the disconnection
    sm_disconnect_process(vif, MAC_RS_UNSPECIFIED);
	os_printf("mm_connection_loss_ind_handler\r\n");

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles the SM_AUTHENTICATION_RSP_TIMER_IND.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
sm_rsp_timeout_ind_handler(ke_msg_id_t const msgid,
                           void const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)

{
    // Check if we are in a state where such timeout could occur
    if ((ke_state_get(TASK_SM) != SM_AUTHENTICATING) && (ke_state_get(TASK_SM) != SM_ASSOCIATING))
        return (KE_MSG_CONSUMED);

    // Status is not successful
    #if NX_ROAMING
    if(sm_env.hand_off_ongoing)
    {
        // Authentication failed, try next candidate
        update_best_cand();

        // Set the reset reason
        reason = ROAMING_RESET;
    }
    else
    #endif // NX_ROAMING
    {
        // Send confirmation to SME
        sm_connect_ind(MAC_ST_FAILURE);
    }

    return (KE_MSG_CONSUMED);
}

#if NX_ROAMING
/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles roaming timer.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
sm_roaming_timer_ind_handler (ke_msg_id_t const msgid,
                              void const *param,
                              ke_task_id_t const dest_id,
                              ke_task_id_t const src_id)
{
    struct best_cand_info  *best_cand_info_ptr;
    int32_t thrsh, gap, hyst;

    if(ke_state_get(TASK_SM) != SM_ACTIVE)
    {
        // sm is not active, no need for the results
        return (KE_MSG_CONSUMED);
    }

    gap  = sm_env.cur_rssi_gap;
    hyst = sm_env.cur_rssi_hyst;

    thrsh = get_roaming_thrsh();

    if(sm_env.cur_rssi < thrsh)
    {
        // check if we have a best candidate
        if(sm_env.avail_cand == 0)
        {
            // no best candidate available, perform full scan to search for possible candidates
            sm_full_scan();
        }
        else if(sm_env.cur_rssi > sm_env.cur_mrssi - gap)
        {
            /* we have a best candidate, but the change in the RSSI
             * is less than the gap, decrease the gap, the minimum gap = 0  */
            gap = (gap > 3) ? (gap-3) : 0;

            sm_env.cur_rssi_gap = gap;

            // restart the timer with the default value
            sm_env.roaming_timer_dur = ROAMING_TIMER_DUR;

            if(gap != 0)
            {
                // the RSSI is decreasing, monitor the changes closely
                ke_timer_set(SM_ROAMING_TIMER_IND,
                             TASK_SM,
                             sm_env.roaming_timer_dur ,
                             false);
            }
            else
            {
                // we are going down, lets check our best candidate
                sm_fast_scan(sm_env.best_cand_info_list);
            }
        }
        else
        {
            //get pointer to the best candidate AP
            best_cand_info_ptr = &sm_env.best_cand_info_list[0];


            if(sm_env.cur_rssi > best_cand_info_ptr->rssi - hyst)
            {
                /* the best candidate is not a lot better than the current,
                   update the best candidate RSSI */
                sm_fast_scan(best_cand_info_ptr);
            }
            else
            {
                /* the best candidate is a lot better than the current,
                   hand-over to the new AP */
                sm_lmac_reset(false, ROAMING_RESET);

                //send SM_DEAUTHENTICATION_IND message to SME
                sm_deauth_ind_send(MAC_RS_UNSPECIFIED);
                // set the hand-off flag
                sm_env.hand_off_ongoing = true;
                // Set the state as reset
                ke_state_set(TASK_SM, SM_RESETTING);
            }
        }

        // update the memorize RSSI of the current AP
        sm_env.cur_mrssi = sm_env.cur_rssi;
    }
    else
    {
        // we are above the threshold remove the best candidate.
        sm_env.avail_cand = 0;
        sm_env.cur_rssi_gap = sm_env.rssi_gap;

        // reset memorize RSSI of the current AP
        sm_env.cur_mrssi = thrsh;

    }
    return (KE_MSG_CONSUMED);
}

#endif // NX_ROAMING

#if NX_ROAMING
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
nxapi_scan_cfm_handler(ke_msg_id_t const msgid,
                       struct nxapi_scan_cfm const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    if(ke_state_get(TASK_SM) != SM_ACTIVE)
    {
        // sm is not active, no need for the results
        return (KE_MSG_CONSUMED);
    }
    // reset all the candidates
    sm_env.avail_cand = 0;

    // if we have results, update the candidate list
    if (scanu_env.result_cnt > 0)
    {
        sm_update_cand_list();
    }
    // check if we have candidates
    if(sm_env.avail_cand == 0)
    {
        // no candidates detected, increase the roaming timer
        sm_env.roaming_timer_dur += ROAMING_TIMER_DUR;

        if(sm_env.roaming_timer_dur > MAX_ROAMING_TIMER_DUR)
        {
            // roaming timer should not exceed MAX_ROAMING_TIMER_DUR
            sm_env.roaming_timer_dur = MAX_ROAMING_TIMER_DUR;
        }
    }
    else
    {
        // candidates detected, reset the roaming timer to defult value
        sm_env.roaming_timer_dur = ROAMING_TIMER_DUR;
    }

    // restart the timer with the updated value
    ke_timer_set(SM_ROAMING_TIMER_IND, TASK_SM, sm_env.roaming_timer_dur, false);

    return (KE_MSG_CONSUMED);
}

#endif // NX_ROAMING


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
scanu_start_cfm_handler(ke_msg_id_t const msgid,
                        void const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    struct mac_addr const *bssid = NULL;
    struct scan_chan_tag const *chan = NULL;

    // Sanity check - This message can be received only in the SCANNING or JOINING states
    ASSERT_ERR(ke_state_get(TASK_SM) == SM_SCANNING);

    // Get the BSSID and Channel from the parameter and/or the scan results
    sm_get_bss_params(&bssid, &chan);

    // Check if we now have enough information to launch the join procedure
    if (bssid && chan)
    {
        sm_join_bss(bssid, chan, false);
    }
    else
    {
        // We did not find our BSS, so terminate the connection procedure
        sm_connect_ind(MAC_ST_FAILURE);
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
scanu_join_cfm_handler(ke_msg_id_t const msgid,
                        void const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    struct sm_connect_req const *con_par = sm_env.connect_param;
    struct vif_info_tag *vif;
    struct mac_bss_info *bss;

    // Sanity check - This message can be received only in the SCANNING or JOINING states
    ASSERT_ERR(ke_state_get(TASK_SM) == SM_JOINING);

    // Get parameters
    vif = &vif_info_tab[con_par->vif_idx];
    bss = &vif->bss_info;

    // Check if the joining procedure was successful
    if (bss->valid_flags & BSS_INFO_VALID)
    {
        // Channel Context Index
        uint8_t chan_idx;

        // Create a channel context for the connection and check if the channel context was correctly added
        if (sm_add_chan_ctx(&chan_idx) == CO_OK)
        {
            struct mm_sta_add_req *req;
			req = KE_MSG_ALLOC(MM_STA_ADD_REQ, TASK_MM,
                                  TASK_SM, mm_sta_add_req);

            // Link the VIF to the channel context
            chan_ctxt_link(con_par->vif_idx, chan_idx);

            // Fill in the MM_STA_ADD_REQ parameters
            req->inst_nbr = con_par->vif_idx;
            req->mac_addr = bss->bssid;
            if (bss->valid_flags & BSS_HT_VALID)
            {
                int ampdu_len_exp = (bss->ht_cap.a_mpdu_param & MAC_AMPDU_LEN_EXP_MSK) >>
                                                                       MAC_AMPDU_LEN_EXP_OFT;
                int ampdu_min_spc = (bss->ht_cap.a_mpdu_param & MAC_AMPDU_MIN_SPACING_MSK) >>
                                                                    MAC_AMPDU_MIN_SPACING_OFT;
                req->ampdu_spacing_min = (ampdu_min_spc < 3)?1:(0x01 << (ampdu_min_spc - 3));
                req->ampdu_size_max_ht = (0x01 << (13 + ampdu_len_exp)) - 1;
                #if NX_VHT
                if (bss->valid_flags & BSS_VHT_VALID)
                {
                    int vht_exp = (bss->vht_cap.vht_capa_info &
                                   MAC_VHTCAPA_MAX_A_MPDU_LENGTH_EXP_MSK) >>
                                           MAC_VHTCAPA_MAX_A_MPDU_LENGTH_EXP_OFT;
                    req->ampdu_size_max_vht = (1 << (MAC_HT_MAX_AMPDU_FACTOR + vht_exp)) - 1;
                }
                else
                {
                    req->ampdu_size_max_vht = 0;
                }
                #endif
            }

            // Send the message
            ke_msg_send(req);

            // We are now waiting for the channel addition confirmation
            ke_state_set(TASK_SM, SM_STA_ADDING);
        }
        else
        {
            // No channel context available, terminate the connection procedure
            sm_connect_ind(MAC_ST_FAILURE);
        }

        // Save the VIF flags
        vif->flags = con_par->flags;

        // Check if we need to disable the HT feature
        if (con_par->flags & DISABLE_HT)
            bss->valid_flags &= ~BSS_HT_VALID;
    }
    else
    {
        // If confirmed joined procedure used an active scan, try again with a passive scan
        if (sm_env.join_passive)
        {
            sm_join_bss(&bss->bssid, bss->chan, true);
        }
        else
        {
            // The join was not successful, so terminate the connection procedure
            sm_connect_ind(MAC_ST_FAILURE);
        }
    }

    return (KE_MSG_CONSUMED);
}

#if NX_ROAMING
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
scanu_fast_cfm_handler(ke_msg_id_t const msgid,
                       void const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    struct mac_scan_result *scan_result_ptr;
    if (ke_state_get(TASK_SM) != SM_ACTIVE)
    {
        // sm is not active, no need for the results
        return (KE_MSG_CONSUMED);
    }

    // get pointer to the best candidate result in scan
    scan_result_ptr = scanu_find_result(&sm_env.best_cand_info_list[0].bssid, false);

    if (scan_result_ptr != 0)
    {
        // the best candidate is significantly better than the current AP
        if (sm_env.cur_rssi <= (scan_result_ptr->rssi - sm_env.cur_rssi_hyst))
        {
            // update the RSSI of the best candidate
            sm_env.best_cand_info_list[0].rssi = scan_result_ptr->rssi;

            // restart the timer with the min value
            ke_timer_set(SM_ROAMING_TIMER_IND, TASK_SM, MIN_ROAMING_TIMER_DUR, false);
        }
        else
        {
            sm_full_scan();
        }
    }
    else
    {
        // best candidate is not found, search for a new one.
        sm_full_scan();
    }
    return (KE_MSG_CONSUMED);
}

#endif // NX_ROAMING


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
mm_sta_add_cfm_handler(ke_msg_id_t const msgid,
                       struct mm_sta_add_cfm const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the STA ADDING state
    ASSERT_ERR(ke_state_get(TASK_SM) == SM_STA_ADDING);

    // Check if the STA was correctly added
    if (param->status == CO_OK)
    {
        struct sta_info_tag *sta = &sta_info_tab[param->sta_idx];
        struct vif_info_tag *vif = &vif_info_tab[sta->inst_nbr];
        struct mac_sta_info *info = &sta->info;
        struct mac_bss_info *bss = &vif->bss_info;

        // Initialize the capabilities of the new STA
        info->rate_set = bss->rate_set;
        if (bss->valid_flags & BSS_QOS_VALID)
        {
            info->capa_flags |= STA_QOS_CAPA;
        }
        if (bss->valid_flags & BSS_HT_VALID)
        {
            uint8_t local_supp_bw = BW_20MHZ;
            uint8_t peer_supp_bw = BW_20MHZ;

            info->capa_flags |= STA_HT_CAPA;
            info->ht_cap = bss->ht_cap;
            #if NX_VHT
            if (bss->valid_flags & BSS_VHT_VALID)
            {
                info->capa_flags |= STA_VHT_CAPA;
                info->vht_cap = bss->vht_cap;
                local_supp_bw = BW_80MHZ;
                peer_supp_bw = BW_80MHZ;
            }
            else
            #endif
            {
                // Check our local supported BW
                if (me_env.ht_cap.ht_capa_info & MAC_HTCAPA_40_MHZ)
                    local_supp_bw = BW_40MHZ;

                if (info->ht_cap.ht_capa_info & MAC_HTCAPA_40_MHZ)
                    peer_supp_bw = BW_40MHZ;
            }

            info->bw_max = co_min(local_supp_bw, peer_supp_bw);
            info->bw_cur = co_min(info->bw_max, bss->bw);
        }

        // Set the BSS parameters to LMAC
        sm_set_bss_param();
    }
    else
    {
        // No station available, terminate the connection procedure
        sm_connect_ind(MAC_ST_FAILURE);
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
me_set_ps_disable_cfm_handler(ke_msg_id_t const msgid,
                              void const *param,
                              ke_task_id_t const dest_id,
                              ke_task_id_t const src_id)
{
    /* Sanity check - This message can be received only in the STA ADDING state*/
    ASSERT_ERR((ke_state_get(TASK_SM) == SM_BSS_PARAM_SETTING) ||
               (ke_state_get(TASK_SM) == SM_IDLE) ||
               (ke_state_get(TASK_SM) == SM_DISCONNECTING));
               
    // Check if we were disabling the PS prior to the parameter setting
    if (ke_state_get(TASK_SM) == SM_BSS_PARAM_SETTING)
    {
        // Send the next BSS parameter configuration message
        sm_send_next_bss_param();
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
mm_bss_param_setting_handler(ke_msg_id_t const msgid,
                             void const *param,
                             ke_task_id_t const dest_id,
                             ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the CHAN CTXT ADDING state
    ASSERT_ERR(ke_state_get(TASK_SM) == SM_BSS_PARAM_SETTING);

    // Send the next BSS parameter configuration message
    sm_send_next_bss_param();

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
me_set_active_cfm_handler(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the STA ADDING state
    ASSERT_ERR((ke_state_get(TASK_SM) == SM_BSS_PARAM_SETTING) ||
               (ke_state_get(TASK_SM) == SM_DISCONNECTING));

    // Check if we went to IDLE due to the disconnection procedure
    if (ke_state_get(TASK_SM) == SM_DISCONNECTING)
    {
        if (sm_env.ft_over_ds == 1)
        {
            struct mac_addr const *bssid = NULL;
            struct scan_chan_tag const *chan = NULL;
			
            sm_get_bss_params(&bssid, &chan);
            sm_join_bss(bssid, chan, false);
        }
        else
        {
            ke_state_set(TASK_SM, SM_IDLE);
        }
    }
    else
    {
        if (sm_env.ft_over_ds == 1)
        {
            sm_assoc_req_send();
        }
        else
        {
            sm_auth_send(MAC_AUTH_FIRST_SEQ, NULL);
        }
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
mm_set_vif_state_cfm_handler(ke_msg_id_t const msgid,
                             void const *param,
                             ke_task_id_t const dest_id,
                             ke_task_id_t const src_id)
{
    // Check if we are in the connection process
    if (ke_state_get(TASK_SM) == SM_ACTIVATING)
    {
        struct sm_connect_req const *con_par = sm_env.connect_param;
        struct vif_info_tag *vif = &vif_info_tab[con_par->vif_idx];
        struct sta_info_tag *sta = &sta_info_tab[vif->u.sta.ap_id];
        struct mm_set_ps_options_req *req;

        // Get a pointer to the kernel message
        req = KE_MSG_ALLOC(MM_SET_PS_OPTIONS_REQ, TASK_MM, TASK_SM, mm_set_ps_options_req);

        // Fill the message parameters
        req->dont_listen_bc_mc = con_par->dont_wait_bcmc;
        req->listen_interval = con_par->listen_interval;
        req->vif_index = con_par->vif_idx;

        // Set the PS options for this VIF
        ke_msg_send(req);

        // Open the control port state of the STA
        sta->ctrl_port_state = (vif->flags & CONTROL_PORT_HOST)?PORT_CONTROLED:PORT_OPEN;
        sta->ctrl_port_ethertype = co_ntohs(con_par->ctrl_port_ethertype);

        // If no EAP frame has to be sent, reenable the PS mode now that the association is complete
        if (sta->ctrl_port_state == PORT_OPEN)
        {
            struct me_set_ps_disable_req *ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, TASK_ME, TASK_SM,
                                                            me_set_ps_disable_req);

            ps->ps_disable = false;
            ps->vif_idx = con_par->vif_idx;

            ke_msg_send(ps);
        }

        // Association can now be considered as complete
        sm_connect_ind(MAC_ST_SUCCESSFUL);
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the reception of a BEACON
 * It extracts all required information from them.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
rxu_mgt_ind_handler(ke_msg_id_t const msgid,
                    struct rxu_mgt_ind const *param,
                    ke_task_id_t const dest_id,
                    ke_task_id_t const src_id)
{
    uint16_t fctl = param->framectrl & MAC_FCTRL_TYPESUBTYPE_MASK;
    int msg_status = KE_MSG_CONSUMED;

    if ((fctl == MAC_FCTRL_AUTHENT) 
			&& (ke_state_get(TASK_SM) == SM_AUTHENTICATING))
    {
        os_printf("sm_auth_handler\r\n");
        sm_auth_handler(param);
    }
    else if ((fctl == MAC_FCTRL_ASSOCRSP) 
			&& (ke_state_get(TASK_SM) == SM_ASSOCIATING))
    {
        os_printf("sm_assoc_rsp_handler\r\n");
        sm_assoc_rsp_handler(param);
    }
    else if ((fctl == MAC_FCTRL_REASSOCRSP) 
			&& (ke_state_get(TASK_SM) == SM_ASSOCIATING))
    {
        os_printf("sm_assoc_rsp_handler\r\n");
        sm_assoc_rsp_handler(param);
    }
    else if ((fctl == MAC_FCTRL_DEAUTHENT) || (fctl == MAC_FCTRL_DISASSOC))
    {
        os_printf("sm_deauth_handler\r\n");
        msg_status = sm_deauth_handler(param);
    }
    else
    {
        os_printf("rxu_mgt_ind:%x\r\n", fctl);
    }

    return (msg_status);
}

/// DEFAULT handler definition.
const struct ke_msg_handler sm_default_state[] =
{
    {SM_CONNECT_REQ, (ke_msg_func_t)sm_connect_req_handler},
    {SM_DISCONNECT_REQ, (ke_msg_func_t)sm_disconnect_req_handler},
    {SCANU_START_CFM, (ke_msg_func_t)scanu_start_cfm_handler},
    {SCANU_JOIN_CFM, (ke_msg_func_t)scanu_join_cfm_handler},
    {SM_RSP_TIMEOUT_IND, (ke_msg_func_t)sm_rsp_timeout_ind_handler},
    {MM_CONNECTION_LOSS_IND, (ke_msg_func_t)mm_connection_loss_ind_handler},
    {MM_STA_ADD_CFM, (ke_msg_func_t)mm_sta_add_cfm_handler},
    {ME_SET_ACTIVE_CFM, (ke_msg_func_t)me_set_active_cfm_handler},
    {MM_SET_BSSID_CFM, mm_bss_param_setting_handler},
    {MM_SET_BASIC_RATES_CFM, mm_bss_param_setting_handler},
    {MM_SET_BEACON_INT_CFM, mm_bss_param_setting_handler},
    {MM_SET_EDCA_CFM, mm_bss_param_setting_handler},
    {MM_SET_VIF_STATE_CFM, mm_set_vif_state_cfm_handler},
    {ME_SET_PS_DISABLE_CFM, (ke_msg_func_t)me_set_ps_disable_cfm_handler},
    {MM_SET_PS_OPTIONS_CFM, ke_msg_discard},
    {MM_STA_DEL_CFM, ke_msg_discard},
    {RXU_MGT_IND, (ke_msg_func_t)rxu_mgt_ind_handler},

    #if NX_ROAMING
    // roaming timer
    {SM_ROAMING_TIMER_IND, (ke_msg_func_t)sm_roaming_timer_ind_handler},
    // from scan
    {NXAPI_SCAN_CFM, (ke_msg_func_t)nxapi_scan_cfm_handler},
    // from scan
    {SCANU_FAST_CFM, scanu_fast_cfm_handler},
    #endif

    // From AP
    //{SM_DEAUTHENTICATION_REQ, (ke_msg_func_t)sm_deauthentication_req_handler},
};

/// Specifies the message handlers that are common to all states.
const struct ke_state_handler sm_default_handler =
KE_STATE_HANDLER(sm_default_state);

/// Defines the placeholder for the states of all the task instances.
ke_state_t sm_state[SM_IDX_MAX];

/// @} //end of group
