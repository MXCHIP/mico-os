/**
 ****************************************************************************************
 *
 * @file mm_task.c
 *
 * @brief MAC Management task.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 *****************************************************************************************
 * @addtogroup MM_TASK
 * @{
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_endian.h"

// for mode
#include "mac_defs.h"
#include "mac_frame.h"
#include "mm.h"
#include "ps.h"
#include "me.h"
#include "rxu_cntrl.h"
#include "mm_task.h"
#include "mm_bcn.h"
#include "phy.h"

#include "reg_mac_pl.h"
#include "reg_mac_core.h"
#include "hal_machw.h"
#include "rxl_cntrl.h"
#include "txl_cntrl.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "version.h"
#include "ke_timer.h"
#include "chan.h"
#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#include "tpc.h"

/*
 * MESSAGE HANDLERS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief MM module read version request message handler.
 * This function handles the MM_VERSION_REQ message, which reads the FW and HW versions
 * and reports them to upper MAC
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_version_req_handler(ke_msg_id_t const msgid,
                       void const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // Allocate the response message
    struct mm_version_cfm *msg = KE_MSG_ALLOC(MM_VERSION_CFM, src_id, dest_id,
                                              mm_version_cfm);

    // Fill in the parameters
    msg->version_lmac = NX_VERSION;
    msg->version_machw_1 = nxmac_version_1_get();
    msg->version_machw_2 = nxmac_version_2_get();
    phy_get_version(&msg->version_phy_1, &msg->version_phy_2);

    msg->features = (0
    #if (NX_BEACONING)
            | CO_BIT(MM_FEAT_BCN_BIT)
    #endif //(NX_BEACONING)
    #if (NX_BCN_AUTONOMOUS_TX)
            | CO_BIT(MM_FEAT_AUTOBCN_BIT)
    #endif //(NX_BCN_AUTONOMOUS_TX)
    #if (NX_HW_SCAN)
            | CO_BIT(MM_FEAT_HWSCAN_BIT)
    #endif //(NX_HW_SCAN)
    #if (NX_CONNECTION_MONITOR)
            | CO_BIT(MM_FEAT_CMON_BIT)
    #endif //(NX_CONNECTION_MONITOR)
    #if (NX_MULTI_ROLE)
            | CO_BIT(MM_FEAT_MROLE_BIT)
    #endif //(NX_MULTI_ROLE)
    #if (NX_RADAR_DETECT)
            | CO_BIT(MM_FEAT_RADAR_BIT)
    #endif //(NX_RADAR_DETECT)
    #if (NX_POWERSAVE)
            | CO_BIT(MM_FEAT_PS_BIT)
    #endif //(NX_POWERSAVE)
    #if (NX_UAPSD)
            | CO_BIT(MM_FEAT_UAPSD_BIT)
    #endif //(NX_UAPSD)
    #if (NX_DPSM)
            | CO_BIT(MM_FEAT_DPSM_BIT)
    #endif //(NX_DPSM)
    #if (NX_AMPDU_TX)
            | CO_BIT(MM_FEAT_AMPDU_BIT)
    #endif //(NX_AMPDU_TX)
    #if (NX_AMSDU_TX)
            | CO_BIT(MM_FEAT_AMSDU_BIT)
    #endif //(NX_AMSDU_TX)
    #if (NX_CHNL_CTXT)
            | CO_BIT(MM_FEAT_CHNL_CTXT_BIT)
    #endif //(NX_CHNL_CTXT)
    #if (NX_P2P)
            | CO_BIT(MM_FEAT_P2P_BIT)
    #endif //(NX_P2P)
    #if (NX_P2P_GO)
            | CO_BIT(MM_FEAT_P2P_GO_BIT)
    #endif //(NX_P2P_GO)
            | CO_BIT(MM_FEAT_UMAC_BIT)
    #if (NX_REORD)
            | CO_BIT(MM_FEAT_REORD_BIT)
    #endif //(NX_REORD)
    #if (NX_MFP)
            | CO_BIT(MM_FEAT_MFP_BIT)
    #endif //(NX_MFP)
    #if (RW_MESH_EN)
            | CO_BIT(MM_FEAT_MESH_BIT)
    #endif //(RW_MESH_EN)
    #if (TDLS_ENABLE)
            | CO_BIT(MM_FEAT_TDLS_BIT)
    #endif //(TDLS_ENABLE)
            );

    #if (RW_BFMEE_EN)
    if (hal_machw_bfmee_support())
        msg->features |= CO_BIT(MM_FEAT_BFMEE_BIT);
    #endif //(RW_BFMEE_EN)

    #if (RW_BFMER_EN)
    msg->features |= CO_BIT(MM_FEAT_BFMER_BIT);
    #endif //(RW_BFMER_EN)

    #if (RW_MUMIMO_RX_EN)
    if (hal_machw_mu_mimo_rx_support())
        msg->features |= CO_BIT(MM_FEAT_MU_MIMO_RX_BIT);
    #endif //(RW_MUMIMO_RX_EN)

    #if (RW_MUMIMO_TX_EN)
    msg->features |= CO_BIT(MM_FEAT_MU_MIMO_TX_BIT);
    #endif //(RW_MUMIMO_TX_EN)

    #if (RW_WAPI_EN)
    if (nxmac_wapi_getf())
        msg->features |= CO_BIT(MM_FEAT_WAPI_BIT);
    #endif //(RW_WAPI_EN)

    #if (NX_VHT)
    msg->features |= CO_BIT(MM_FEAT_UMAC_VHT_BIT);
    #endif

    // Send the response
    ke_msg_send(msg);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_reset_req_handler(ke_msg_id_t const msgid,
                     void const *param,
                     ke_task_id_t const dest_id,
                     ke_task_id_t const src_id)
{    
    GLOBAL_INT_DECLARATION();
    
    // For safety do all the reset procedure with interrupts disabled
    GLOBAL_INT_DISABLE();

    // Stop the MAC HW operation
    hal_machw_stop();

    // Stop the PHY operation
    phy_stop();

    // Re-initialize the UMAC
    me_init();

    // Re-Initialize the SW
    mm_init();

    // Reenable interrupts
    GLOBAL_INT_RESTORE();

    // Send the confirmation
    ke_msg_send_basic(MM_RESET_CFM, src_id, dest_id);

    // Go to the IDLE state
    ke_state_set(TASK_MM, MM_IDLE);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_start_req_handler(ke_msg_id_t const msgid,
                     struct mm_start_req const *param,
                     ke_task_id_t const dest_id,
                     ke_task_id_t const src_id)
{
    // Sanity check: This message can be handled in IDLE state only
    ASSERT_ERR(ke_state_get(dest_id) == MM_IDLE);

    // Initialize the PHY
    phy_init(&param->phy_cfg);

    // Start the PHY on the default channel
    phy_set_channel(PHY_BAND_2G4, PHY_CHNL_BW_20, 2412, 2412, 0, PHY_PRIM);
    tpc_update_tx_power(15);

    #if NX_UAPSD
    // Set the U-APSD timeout
    ps_env.uapsd_timeout = param->uapsd_timeout * MILLI2MICRO;
    #endif

    #if NX_POWERSAVE
    /// Save local LP clock accuracy (in ppm)
    mm_env.lp_clk_accuracy = param->lp_clk_accuracy;
    #endif

    // Send the confirmation
    ke_msg_send_basic(MM_START_CFM, src_id, dest_id);

    mm_active();
    // Request to MAC HW to switch to IDLE state
    hal_machw_idle_req();

    // Adjust the MM state accordingly
    ke_state_set(dest_id, MM_GOING_TO_IDLE);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

#if (!NX_CHNL_CTXT)
/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the MM_SET_CHANNEL_REQ message, which sets the channel.
 * If channel context is enabled, this function can only set channel of secondary chain.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_channel_req_handler(ke_msg_id_t const msgid,
                           struct mm_set_channel_req const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)
{    
    GLOBAL_INT_DECLARATION();
	struct mm_set_channel_cfm *cfm;
    
    // Once in IDLE, handle the packets already in the RX queue to ensure that the
    // channel information indicated to the upper MAC is correct. This has to be done with
    // interrupts disabled, as the normal handling of the packets is done under interrupt
    GLOBAL_INT_DISABLE();
    rxl_timer_int_handler();
    rxl_cntrl_evt(0);
    GLOBAL_INT_RESTORE();

    // Allocate confirmation message
    cfm = KE_MSG_ALLOC(MM_SET_CHANNEL_CFM, src_id, dest_id, mm_set_channel_cfm);
    // Now we can move the PHY to the requested channel
    phy_set_channel(param->band, param->type, param->prim20_freq, param->center1_freq,
                    param->center2_freq, param->index);

    if (param->index == PHY_PRIM)
    {
        tpc_update_tx_power(param->tx_power);
        cfm->power = param->tx_power;
        phy_get_rf_gain_idx(&cfm->power, &cfm->radio_idx);
    }

    // Send the confirmation
    ke_msg_send(cfm);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

#else
static int
mm_set_channel_req_handler(ke_msg_id_t const msgid,
                           struct mm_set_channel_req const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)
{
    struct mm_set_channel_cfm *cfm;

    // Allocate confirmation message
    cfm = KE_MSG_ALLOC(MM_SET_CHANNEL_CFM, src_id, dest_id, mm_set_channel_cfm);
    if (param->index != PHY_PRIM) 
	{
        // Now we can move the PHY to the requested channel
        phy_set_channel(param->band, param->type, param->prim20_freq, param->center1_freq,
                        param->center2_freq, param->index);
    }

    
     // Send the confirmation
    ke_msg_send(cfm);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}
#endif //(!NX_CHNL_CTXT)

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_dtim_req_handler(ke_msg_id_t const msgid,
                        struct mm_set_dtim_req const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    // Set the DTIM period in the HW
    nxmac_dtim_period_setf(param->dtim_period);
    nxmac_dtim_updated_by_sw_setf(1);

    // Send the confirmation
    ke_msg_send_basic(MM_SET_DTIM_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the SET_BEACON_INT_REQ message, which programs the dedicated
 * HW register.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_beacon_int_req_handler(ke_msg_id_t const msgid,
                              struct mm_set_beacon_int_req const *param,
                              ke_task_id_t const dest_id,
                              ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->inst_nbr];

    // Check if VIF is a AP or STA one
    if (vif_entry->type == VIF_STA)
    {
        #if (NX_MULTI_ROLE)
        {
            // Get peer AP information
            struct sta_info_tag *p_sta_entry = &sta_info_tab[vif_entry->u.sta.ap_id];

            // For STA, the beacon interval is only used by the FW
            p_sta_entry->bcn_int = param->beacon_int * TU_DURATION;
        }
        #else //(NX_MULTI_ROLE)
        nxmac_beacon_int_setf(param->beacon_int * TU_DURATION);
        #endif //(NX_MULTI_ROLE)
    }
    else
    {
        // For AP or MP, set the beacon interval in the HW
        vif_mgmt_set_ap_bcn_int(vif_entry, param->beacon_int);
    }

    // Send the confirmation
    ke_msg_send_basic(MM_SET_BEACON_INT_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the @ref MM_SET_BASIC_RATES_REQ message, which programs the
 * dedicated HW register.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_basic_rates_req_handler(ke_msg_id_t const msgid,
                               struct mm_set_basic_rates_req const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
    #if NX_CHNL_CTXT
    struct chan_ctxt_tag *chan = chan_env.current_channel;

    // Get the new ones
    mm_env.basic_rates[param->band] = param->rates;

    // Check if we are currently on the band
    if (chan && (chan->channel.band == param->band))
        // Set the basic rates in HW
        nxmac_rates_set(mm_env.basic_rates[param->band]);

    #else
    // Set the basic rates in the HW
    nxmac_rates_set(param->rates);
    #endif

    // Send the confirmation
    ke_msg_send_basic(MM_SET_BASIC_RATES_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the SET_FILTER_REQ message, which sets the RX filter in the HW.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_filter_req_handler(ke_msg_id_t const msgid,
                          struct mm_set_filter_req const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Set the UMAC RX filter
    mm_rx_filter_umac_set(param->filter);

    // Send the confirmation
    ke_msg_send_basic(MM_SET_FILTER_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the SET_BSSID_REQ message, which sets the BSSID in the HW.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_bssid_req_handler(ke_msg_id_t const msgid,
                         struct mm_set_bssid_req const *param,
                         ke_task_id_t const dest_id,
                         ke_task_id_t const src_id)
{
    #if NX_MULTI_ROLE
    struct vif_info_tag *vif_entry = &vif_info_tab[param->inst_nbr];

    // Copy the BSSID in the VIF structure
    memcpy(&vif_entry->bssid, &param->bssid, sizeof(param->bssid));

    // Check if there is currently only one VIF created
    if (vif_mgmt_used_cnt() == 1)
    {
        // Only one VIF created, so put the BSSID in the HW
        // write lower 4 bytes of BSSID
        nxmac_bss_id_low_setf(param->bssid.array[0] | (((uint32_t)param->bssid.array[1]) << 16));

        // write higher 2 bytes of BSSID
        nxmac_bss_id_high_setf(param->bssid.array[2]);
    }
    #else
    // write lower 4 bytes of BSSID
    nxmac_bss_id_low_setf(param->bssid.array[0] | (((uint32_t)param->bssid.array[1]) << 16));

    // write higher 2 bytes of BSSID
    nxmac_bss_id_high_setf(param->bssid.array[2]);
    #endif

    // Send the confirmation
    ke_msg_send_basic(MM_SET_BSSID_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the SET_BSSID_REQ message, which sets the BSSID in the HW.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_edca_req_handler(ke_msg_id_t const msgid,
                         struct mm_set_edca_req const *param,
                         ke_task_id_t const dest_id,
                         ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->inst_nbr];

    // Store the parameters in the VIF
    vif_entry->txq_params[param->hw_queue] = param->ac_param;

    // If VIF is active, change the parameters immediately
    if (vif_entry->active)
    {
        // Put the EDCA parameters in the correct register according to the queue index
        switch (param->hw_queue)
        {
            case AC_BK:
                nxmac_edca_ac_0_set(param->ac_param);
                break;
            case AC_BE:
                nxmac_edca_ac_1_set(param->ac_param);
                break;
            case AC_VI:
                nxmac_edca_ac_2_set(param->ac_param);
                break;
            default:
                nxmac_edca_ac_3_set(param->ac_param);
                break;
        }

        // Store A-MPDU maximum duration parameters in environment for faster access by TX path
        mm_env_max_ampdu_duration_set();
    }

    #if NX_UAPSD
    // Store the information on UAPSD for the queue and interface index
    if (vif_entry->type == VIF_STA)
    {
        ps_uapsd_set(vif_entry, param->hw_queue, param->uapsd);
    }
    #endif

    // Send the confirmation
    ke_msg_send_basic(MM_SET_EDCA_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the SET_BSSID_REQ message, which sets the BSSID in the HW.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_slottime_req_handler(ke_msg_id_t const msgid,
                            struct mm_set_slottime_req const *param,
                            ke_task_id_t const dest_id,
                            ke_task_id_t const src_id)
{
    uint16_t mac_core_clk;

    // Get MAC core clock value
    mac_core_clk = nxmac_mac_core_clk_freq_getf();

    // Set slot time
    nxmac_timings_2_pack(mac_core_clk * param->slottime, param->slottime);

    // Send the confirmation
    ke_msg_send_basic(MM_SET_SLOTTIME_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the SET_BSSID_REQ message, which sets the BSSID in the HW.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_mode_req_handler(ke_msg_id_t const msgid,
                        struct mm_set_mode_req const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    // Set slot time
    nxmac_abgn_mode_setf(param->abgnmode);

    // Send the confirmation
    ke_msg_send_basic(MM_SET_MODE_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the SET_BSSID_REQ message, which sets the BSSID in the HW.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_vif_state_req_handler(ke_msg_id_t const msgid,
                             struct mm_set_vif_state_req const *param,
                             ke_task_id_t const dest_id,
                             ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->inst_nbr];

    // Check if the VIF is of STA type
    if (vif_entry->type == VIF_STA)
    {
        if (param->active)
        {
            // Get peer AP information
            struct sta_info_tag *p_sta_entry = &sta_info_tab[vif_entry->u.sta.ap_id];
            #if NX_POWERSAVE
            uint32_t drift;
            #endif
            #if NX_MULTI_ROLE
            // Compute default value of next TBTT based on current local time
            uint32_t next_tbtt = ke_time() + p_sta_entry->bcn_int;

            // Program the next TBTT
            mm_timer_set(&vif_entry->tbtt_timer, next_tbtt);
            #endif

            // We save the AP index for later use
            p_sta_entry->aid = param->aid;

            #if (RW_BFMEE_EN)
            /*
             * If we are STA and MU Beamformee Capable we have to keep the AID in a register so that when we receive
             * a NDPA with several STA Info HW can detect the STA Info that is dedicated to us
             * Note: We currently support to be only one time MU Beamformee as STA.
             */
            nxmac_aid_setf(param->aid);
            #endif //(RW_BFMEE_EN)

            #if NX_POWERSAVE
            vif_entry->u.sta.listen_interval = 0;
            vif_entry->u.sta.dont_wait_bcmc = false;
            // Compute the maximum drift we could suffer due to the LP clock inaccuracy
            drift = ((uint32_t)(mm_env.lp_clk_accuracy + MM_AP_CLK_ACCURACY) *
                     (uint32_t)p_sta_entry->bcn_int) / 1000000;
            p_sta_entry->drift = (uint16_t)drift;
            #if NX_UAPSD
            vif_entry->u.sta.uapsd_last_rxtx = ke_time();
            #endif
            vif_entry->prevent_sleep |= PS_VIF_WAITING_BCN;
            #endif

            #if NX_CONNECTION_MONITOR
            // Reset the beacon loss count and keep-alive frame time
            vif_entry->u.sta.beacon_loss_cnt = 0;
            vif_entry->u.sta.mon_last_crc = 0;
            vif_entry->u.sta.mon_last_tx = ke_time();
            #endif

            #if (NX_CHNL_CTXT)
            // Try to spend more time on channel in order to catch a beacon
            chan_bcn_detect_start(vif_entry);
            #endif //(NX_CHNL_CTXT)
        }
        else
        {
            #if NX_MULTI_ROLE
            // Clear the TBTT timer
            mm_timer_clear(&vif_entry->tbtt_timer);
            #endif
        }
    }

    #if (NX_P2P)
    p2p_set_vif_state(vif_entry, param->active);
    #endif //(NX_P2P)

    // Save the state of the VIF
    vif_entry->active = param->active;

    // Put the EDCA parameters in the registers
    if (param->active)
    {
        nxmac_edca_ac_0_set(vif_entry->txq_params[AC_BK]);
        nxmac_edca_ac_1_set(vif_entry->txq_params[AC_BE]);
        nxmac_edca_ac_2_set(vif_entry->txq_params[AC_VI]);
        nxmac_edca_ac_3_set(vif_entry->txq_params[AC_VO]);

        // Store A-MPDU maximum duration parameters in environment for faster access by TX path
        mm_env_max_ampdu_duration_set();
    }

    // Send the confirmation
    ke_msg_send_basic(MM_SET_VIF_STATE_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the MM_SET_POWER_REQ message, which sets the tx power on a vif.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_power_req_handler(ke_msg_id_t const msgid,
                         struct mm_set_power_req const *param,
                         ke_task_id_t const dest_id,
                         ke_task_id_t const src_id)
{
    struct mm_set_power_cfm *cfm;
    struct vif_info_tag *vif_entry = &vif_info_tab[param->inst_nbr];

    // Allocate confirmation message
    cfm = KE_MSG_ALLOC(MM_SET_POWER_CFM, src_id, dest_id, mm_set_power_cfm);

    vif_entry->user_tx_power = param->power;
    cfm->power = vif_entry->tx_power;
    vif_entry->tx_power = VIF_UNDEF_POWER; // To force power update
    tpc_update_vif_tx_power(vif_entry, &cfm->power, &cfm->radio_idx);

    // Send the confirmation
    ke_msg_send(cfm);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the DBG_TRIGGER_REQ message.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_dbg_trigger_req_handler(ke_msg_id_t const msgid,
                           struct mm_dbg_trigger_req const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)
{
    // Call the platform function to force the trigger
    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_add_if_req_handler(ke_msg_id_t const msgid,
                      struct mm_add_if_req const *param,
                      ke_task_id_t const dest_id,
                      ke_task_id_t const src_id)
{
    struct mm_add_if_cfm *cfm;

    // Allocate confirmation message
    cfm = KE_MSG_ALLOC(MM_ADD_IF_CFM, src_id, dest_id, mm_add_if_cfm);

    // Register the VIF
    cfm->status = vif_mgmt_register(&param->addr, param->type, param->p2p, &cfm->inst_nbr);

    // Send the confirmation
    ke_msg_send(cfm);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_remove_if_req_handler(ke_msg_id_t const msgid,
                         struct mm_remove_if_req const *param,
                         ke_task_id_t const dest_id,
                         ke_task_id_t const src_id)
{
    // Check if VIF index is valid
    if (param->inst_nbr < NX_VIRT_DEV_MAX)
    {
        vif_mgmt_unregister(param->inst_nbr);
    }

    // Check if we have removed the last VIF
    if (co_list_is_empty(&vif_mgmt_env.used_list))
    {
        // No more active VIFs, go back to monitor mode
        hal_machw_enter_monitor_mode();
    }

    // Send the confirmation
    ke_msg_send_basic(MM_REMOVE_IF_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the SET_POWERMODE_REQ message sent by UMAC, which tell the POWER
 * Save mode to operate in.This information is updated in lmac_sta_info_tag and STA State is
 * changed to DOZE/ACTIVE.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_idle_req_handler(ke_msg_id_t const msgid,
                        struct mm_set_idle_req const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    if (ke_state_get(dest_id) == MM_HOST_BYPASSED)
    {
        // UMAC requests for IDLE or active are currently bypassed.
        return (KE_MSG_SAVED);
    }

    // Save the state that is requested by the host
    mm_env.host_idle = param->hw_idle;

    // Check if HW has to be put in IDLE or ACTIVE state
    if (param->hw_idle)
    {
        // Check if we are in IDLE state
        switch (ke_state_get(dest_id))
        {
            case MM_IDLE:
                // Sanity check: As MM state is IDLE, HW state is also supposed to be IDLE
                ASSERT_ERR(nxmac_current_state_getf() == HW_IDLE);

                mm_env.prev_mm_state = MM_IDLE;
                mm_env.prev_hw_state = HW_IDLE;
                break;

            case MM_GOING_TO_IDLE:
                // MAC is currently going to IDLE, so simply save the message. It will be
                // rescheduled once the MAC has switched to IDLE.
                return (KE_MSG_SAVED);

            default:
                // Request to MAC HW to switch to IDLE state
                hal_machw_idle_req();

                // Adjust the MM state accordingly
                ke_state_set(dest_id, MM_GOING_TO_IDLE);

                // Save the message
                return (KE_MSG_SAVED);
        }
    }
    else
    {
        if (ke_state_get(dest_id) == MM_GOING_TO_IDLE)
        {
            // We are currently going to IDLE, wait for this to be complete before
            // accepting a new request from host
            return (KE_MSG_SAVED);
        }

        mm_active();
    }

    // Send the confirmation
    ke_msg_send_basic(MM_SET_IDLE_CFM, src_id, dest_id);

    // Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the SET_POWERMODE_REQ message sent by UMAC, which tell the POWER
 * Save mode to operate in.This information is updated in lmac_sta_info_tag and STA State is
 * changed to DOZE/ACTIVE.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_force_idle_req_handler(ke_msg_id_t const msgid,
                          struct mm_force_idle_req const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Check if we are in IDLE state
    switch (ke_state_get(dest_id))
    {
        case MM_IDLE:
            // Sanity check: As MM state is IDLE, HW state is also supposed to be IDLE
            ASSERT_ERR(nxmac_current_state_getf() == HW_IDLE);
            break;

        case MM_GOING_TO_IDLE:
            // MAC is currently going to IDLE, so simply save the message. It will be
            // rescheduled once the MAC has switched to IDLE.
            return (KE_MSG_SAVED);

        default:
            // Request to MAC HW to switch to IDLE state
            hal_machw_idle_req();

            // Adjust the MM state accordingly
            ke_state_set(dest_id, MM_GOING_TO_IDLE);

            // Save the message
            return (KE_MSG_SAVED);
    }

    // IDLE requests from host are now bypassed
    ke_state_set(dest_id, MM_HOST_BYPASSED);

    // Call the callback function
    param->cb();

    // Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module AP INFORMATION MANAGEMENT message handler.
 * This function handles STA_ADD_REQ message received from UMAC.Received parameter
 * staindex is used to allocated lmac_sta_info_tag from global lmac_sta_info_tag pool. The
 * allocated lmac_sta_info_tag is updated with other received parameters.
 * Finally a STA_ADD_CFM message is sent to the UMAC with result of operation.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_sta_add_req_handler(ke_msg_id_t const msgid,
                       struct mm_sta_add_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // Allocate the response structure
    struct mm_sta_add_cfm *rsp = KE_MSG_ALLOC(MM_STA_ADD_CFM, src_id, dest_id, mm_sta_add_cfm);

    // Register the new station
    rsp->status = mm_sta_add(param, &rsp->sta_idx, &rsp->hw_sta_idx);

    // Send the confirmation
    ke_msg_send(rsp);

    return (KE_MSG_CONSUMED);
}
/**
 ****************************************************************************************
 * @brief MM module AP INFORMATION MANAGEMENT message handler.
 * This function handles STA_DEL_REQ message received from UMAC.Received parameter
 * staindex is used to de-allocat lmac_sta_info_tag from global lmac_sta_info_tag pool. The
 * de-allocated lmac_sta_info_tag is updated.Finally a STA_DEL_CFM message is sent to the
 * UMAC with result of operation.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_sta_del_req_handler(ke_msg_id_t const msgid,
                       struct mm_sta_del_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // Delete the station
    mm_sta_del(param->sta_idx);

    // Send the confirmation to the upper MAC
    ke_msg_send_basic(MM_STA_DEL_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT SECURITY Module message handler.
 * This function handles the KEY_ADD_REQ message, sent by UMAC and then calls the actual
 * Key handling function mm_sec_keyaddreq_handler() to perform the Key Add operation.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_key_add_req_handler(ke_msg_id_t const msgid,
                       struct mm_key_add_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    uint8_t hw_key_idx;
    struct mm_key_add_cfm *keyadd_cfm_param =
        KE_MSG_ALLOC(MM_KEY_ADD_CFM, src_id, dest_id, mm_key_add_cfm);

    // Assert on invalid Key Index.
    #if NX_MFP
    ASSERT_ERR(param->key_idx < MAC_DEFAULT_MFP_KEY_COUNT);
    #else
    ASSERT_ERR(param->key_idx < MAC_DEFAULT_KEY_COUNT);
    #endif

    // Assert on invalid Key Length.
    ASSERT_ERR(param->key.length <= MAC_SEC_KEY_LEN);

    // Assert on invalid Key Length.
    ASSERT_ERR(param->cipher_suite <= MAC_RSNIE_CIPHER_AES_CMAC);

    // Copy the key in the HW
    hw_key_idx = mm_sec_machwkey_wr(param);

    // KEY_ADD_CFM return Parameter.
    keyadd_cfm_param->hw_key_idx = hw_key_idx;
    keyadd_cfm_param->status = CO_OK;

    // Post the MM_KEY_ADD_CFM message in QUEUE
    ke_msg_send(keyadd_cfm_param);

    // Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT SECURITY Module message handler.
 * This function handles the KEY_DEL_REQ message, sent by UMAC and then calls the actual
 * Key handling function mm_sec_keydelreq_handler() to perform the Key Delete operation.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_key_del_req_handler(ke_msg_id_t const msgid,
                       struct mm_key_del_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // Assert on invalid Key Index.
    #if NX_MFP
    ASSERT_ERR(param->hw_key_idx <= MM_SEC_MAX_MFP_KEY_NBR);
    #else
    ASSERT_ERR(param->hw_key_idx <= MM_SEC_MAX_KEY_NBR);
    #endif

    // Disable the key in the HW
    mm_sec_machwkey_del(param->hw_key_idx);

    // Post the MM_KEY_DEL_CFM message in QUEUE
    ke_msg_send_basic(MM_KEY_DEL_CFM, src_id, dest_id);

    // Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT QoS Module message handler.
 * This function handles the @ref MM_BA_ADD_REQ message, sent by UMAC, and registers
 * the received BA agreement details (tid, ssn, buffer size) for the appropriate STA.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_ba_add_req_handler(ke_msg_id_t const msgid,
                      struct mm_ba_add_req const *param,
                      ke_task_id_t const dest_id,
                      ke_task_id_t const src_id)
{
    struct mm_ba_add_cfm *cfm;
    uint8_t status = BA_AGMT_ESTABLISHED;

    do
    {
        // Check BA type
        if (param->type == BA_AGMT_TX)
        {
            #if (NX_AMPDU_TX)
            // Buffer Size
            uint8_t bufsz;

            // Limit the maximum number of MPDUs in the A-MPDU to the minimum of
            // the half of the BA window and the half of the number of TX descriptors
            // per queue
            if (param->bufsz < nx_txdesc_cnt[mac_tid2ac[param->tid]])
            {
                bufsz = param->bufsz / 2;
            }
            else
            {
                bufsz = nx_txdesc_cnt[mac_tid2ac[param->tid]] / 2;
            }

            sta_mgmt_set_tx_buff_size(param->sta_idx, param->tid, bufsz);
            sta_mgmt_set_tx_ssn(param->sta_idx, param->tid, param->ssn);
            #else
            status = BA_AGMT_NOT_SUPPORTED;
            #endif //(NX_AMPDU_TX)
        }
        else
        {
            #if (NX_REORD)
            // Create the reordering instance
            if (!rxu_cntrl_reord_create(&sta_info_tab[param->sta_idx], param->tid,
                                        param->ssn))
            {
                status = BA_AGMT_NO_MORE_BA_AGMT;
                break;
            }
            #endif //(NX_REORD)

            // Reset the MAC HW BlockAck bitmaps
            // Currently it is not possible to reset per STA/TID, so reset all of them
            nxmac_ba_ps_bitmap_reset_setf(1);
        }
    } while (0);

    // Allocate kernel message space for the confirmation to send
    cfm = KE_MSG_ALLOC(MM_BA_ADD_CFM, src_id, dest_id, mm_ba_add_cfm);

    cfm->sta_idx = param->sta_idx;
    cfm->tid     = param->tid;
    cfm->status  = status;

    // Post the MM_BA_ADD_CFM message in QUEUE
    ke_msg_send(cfm);

    // Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}


#if (NX_AMPDU_TX || NX_REORD)
/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT QoS Module message handler.
 * This function handles the @ref MM_BA_DEL_REQ message, sent by UMAC and then unregisters
 * the BA agreement information from the STA info table.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_ba_del_req_handler(ke_msg_id_t const msgid,
                     struct mm_ba_del_req const *param,
                     ke_task_id_t const dest_id,
                     ke_task_id_t const src_id)
{
    struct mm_ba_del_cfm *cfm;
    uint8_t status = BA_AGMT_DELETED;

    do
    {
        // Check BA type
        if (param->type == BA_AGMT_TX)
        {
            #if (NX_AMPDU_TX)
            // Check if the specified BA agreement already exists
            if (!mm_ba_agmt_tx_exists(param->sta_idx, param->tid))
            {
                status = BA_AGMT_DOESNT_EXIST;
                break;
            }

            // Unregister BA agreement information from (STA, TID) -> invalidate bufsz
            sta_mgmt_set_tx_buff_size(param->sta_idx, param->tid, 0);
            sta_mgmt_set_tx_ssn(param->sta_idx, param->tid, 0);
            #else
            status = BA_AGMT_NOT_SUPPORTED;
            #endif //(NX_AMPDU_TX)
        }
        else
        {
            #if (NX_REORD)
            // Check if the specified BA agreement already exists
            if (!mm_ba_agmt_rx_exists(param->sta_idx, param->tid))
            {
                status = BA_AGMT_DOESNT_EXIST;
                break;
            }

            // Delete the reordering instance
            rxu_cntrl_reord_delete(&sta_info_tab[param->sta_idx], param->tid);

            #else
            status = BA_AGMT_NOT_SUPPORTED;
            #endif //(NX_REORD)
        }
    } while (0);

    // Allocate kernel message space for the confirmation to send
    cfm = KE_MSG_ALLOC(MM_BA_DEL_CFM, src_id, dest_id, mm_ba_del_cfm);

    cfm->sta_idx = param->sta_idx;
    cfm->tid     = param->tid;
    cfm->status  = status;

    // Post the MM_BA_DEL_CFM message in QUEUE
    ke_msg_send(cfm);

    // Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}
#endif //(NX_AMPDU_TX || NX_REORD)

#if (NX_CHNL_CTXT)
/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_chan_ctxt_update_req_handler(ke_msg_id_t const msgid,
                                struct mm_chan_ctxt_update_req const *param,
                                ke_task_id_t const dest_id,
                                ke_task_id_t const src_id)
{
    // Link the channel context to the VIF
    chan_ctxt_update(param);

    // Send the confirmation
    ke_msg_send_basic(MM_CHAN_CTXT_UPDATE_CFM, src_id, dest_id);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_remain_on_channel_req_handler(ke_msg_id_t const msgid,
                                 struct mm_remain_on_channel_req const *param,
                                 ke_task_id_t const dest_id,
                                 ke_task_id_t const src_id)
{
    // Start RoC
    uint8_t status = chan_roc_req(param, src_id);

    #if (TDLS_ENABLE)
    if ((src_id != TASK_MM) && (src_id != TASK_TDLS))
    #else
    if (src_id != TASK_MM)
    #endif
    {
        // Send back the confirmation
        struct mm_remain_on_channel_cfm *cfm = KE_MSG_ALLOC(MM_REMAIN_ON_CHANNEL_CFM,
                                                            src_id, dest_id,
                                                            mm_remain_on_channel_cfm);

        cfm->op_code         = param->op_code;
        cfm->status          = status;
        cfm->chan_ctxt_index = CHAN_ROC_CTXT_IDX;

        ke_msg_send(cfm);
    }

    return (KE_MSG_CONSUMED);
}
#endif //(NX_CHNL_CTXT)

#if NX_BCN_AUTONOMOUS_TX
/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_bcn_change_req_handler(ke_msg_id_t const msgid,
                          struct mm_bcn_change_req const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Schedule the channel context
    mm_bcn_change(param);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_NO_FREE);
}

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_tim_update_req_handler(ke_msg_id_t const msgid,
                          struct mm_tim_update_req const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Schedule the channel context
    mm_tim_update(param);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_NO_FREE);
}
#endif

/**
 ****************************************************************************************
 * @brief MM module STA INFORMATION MANAGEMENT message handler.
 * This function handles the RESET_REQ message, which resets all data structure and
 * variables. Programs MAC HW register for Reset.Initializes all other contexts and
 * modules. After RESET procedure is complete it sends the MM_RESET_CFM message to UMAC.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_hw_config_handler(ke_msg_id_t const msgid,
                     void const *param,
                     ke_task_id_t const dest_id,
                     ke_task_id_t const src_id)
{
    int status = KE_MSG_SAVED;

    // Check if we are in IDLE state
    switch (ke_state_get(dest_id))
    {
        case MM_IDLE:
            // Sanity check: As MM state is IDLE, HW state is also supposed to be IDLE
            ASSERT_ERR(nxmac_current_state_getf() == HW_IDLE);

            // Now that we are in IDLE state, handle the configuration request
            switch (msgid)
            {
                case MM_SET_CHANNEL_REQ:
                    status = mm_set_channel_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_FILTER_REQ:
                    status = mm_set_filter_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_ADD_IF_REQ:
                    status = mm_add_if_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_REMOVE_IF_REQ:
                    status = mm_remove_if_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_BASIC_RATES_REQ:
                    status = mm_set_basic_rates_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_BEACON_INT_REQ:
                    status = mm_set_beacon_int_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_DTIM_REQ:
                    status = mm_set_dtim_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_BSSID_REQ:
                    status = mm_set_bssid_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_EDCA_REQ:
                    status = mm_set_edca_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_SLOTTIME_REQ:
                    status = mm_set_slottime_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_MODE_REQ:
                    status = mm_set_mode_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_SET_VIF_STATE_REQ:
                    status = mm_set_vif_state_req_handler(msgid, param, dest_id, src_id);
                    break;
                case MM_BA_ADD_REQ:
                    status = mm_ba_add_req_handler(msgid, param, dest_id, src_id);
                    break;
                #if (NX_CHNL_CTXT)
                case MM_CHAN_CTXT_UPDATE_REQ:
                    status = mm_chan_ctxt_update_req_handler(msgid, param, dest_id, src_id);
                    break;
                #endif //(NX_CHNL_CTXT)
                case MM_DBG_TRIGGER_REQ:
                    status = mm_dbg_trigger_req_handler(msgid, param, dest_id, src_id);
                    break;

                default:
                    break;
            }

            // Restore the HW state
            nxmac_next_state_setf(mm_env.prev_hw_state);

            // As well as the MM state
            ke_state_set(dest_id, mm_env.prev_mm_state);
            break;

        case MM_GOING_TO_IDLE:
        case MM_HOST_BYPASSED:
            // MAC is currently going to IDLE, so simply save the message. It will be
            // rescheduled once the MAC has switched to IDLE.
            break;

        default:
            // Store the current HW and MM states for later restoring
            mm_env.prev_hw_state = nxmac_current_state_getf();
            mm_env.prev_mm_state = ke_state_get(dest_id);

            // Request to MAC HW to switch to IDLE state
            hal_machw_idle_req();

            // Adjust the MM state accordingly
            ke_state_set(dest_id, MM_GOING_TO_IDLE);
            break;
    }

    return (status);
}

#if NX_POWERSAVE
/**
 ****************************************************************************************
 * @brief MM module message handler.
 * This function handles the @ref MM_SET_PS_MODE_REQ message, sent by UMAC, which tells
 * LMAC whether PowerSave should be enabled or disabled.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_set_ps_mode_req_handler(ke_msg_id_t const msgid,
                           struct mm_set_ps_mode_req const *param,
                           ke_task_id_t const dest_id,
                           ke_task_id_t const src_id)
{
    // Ask the PS module to change the Power-save mode
    ps_set_mode(param->new_state, src_id);

    return (KE_MSG_CONSUMED);
}

static int
mm_set_ps_options_req_handler(ke_msg_id_t const msgid,
                              struct mm_set_ps_options_req const *param,
                              ke_task_id_t const dest_id,
                              ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_index];

    // Sanity check - These parameters apply only for a STA interface
    ASSERT_ERR(vif_entry->type == VIF_STA);

    // Set the parameters
    vif_entry->u.sta.listen_interval = param->listen_interval;
    vif_entry->u.sta.dont_wait_bcmc = param->dont_listen_bc_mc;

    // Send the confirmation
    ke_msg_send_basic(MM_SET_PS_OPTIONS_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}
#endif

#if (NX_P2P_GO)
/**
 ****************************************************************************************
 * @brief
 ****************************************************************************************
 */
static int mm_set_p2p_noa_req_handler(ke_msg_id_t const msgid,
                                      struct mm_set_p2p_noa_req const *param,
                                      ke_task_id_t const dest_id,
                                      ke_task_id_t const src_id)
{
    // Allocate confirmation message
    struct mm_set_p2p_noa_cfm *cfm = KE_MSG_ALLOC(MM_SET_P2P_NOA_CFM, TASK_API,
                                                  TASK_MM, mm_set_p2p_noa_cfm);

    cfm->status    = CO_FAIL;

    do
    {
        struct vif_info_tag *p_vif_entry;

        if (param->vif_index >= NX_VIRT_DEV_MAX)
        {
            break;
        }

        p_vif_entry = &vif_info_tab[param->vif_index];

        if (!p_vif_entry || !p_vif_entry->p2p)
        {
            break;
        }

        if (param->count)
        {
            // Compute next TBTT instant in order to deduce the NOA start time
            uint32_t beacon_int = (uint32_t)p_vif_entry->u.ap.bcn_int << 10;
            uint32_t next_tbtt  = ((nxmac_tsf_lo_get() / beacon_int) + 1) * beacon_int;

            /*
             * Next TBTT time is currently based on TSF value, as TSF counter and Monoatomic counter might have
             * different values, we have to convert this value in local time in order to use internal timers.
             * The NOA start time inserted in the beacon will be a TSF value.
             */
            next_tbtt += (ke_time() - nxmac_tsf_lo_get());

            // Start NOA procedure and check if NOA has been successfully added
            if (p2p_go_noa_start(p_vif_entry, false, param->dyn_noa,
                                 param->count, param->interval_us, param->duration_us, next_tbtt + param->start_offset))
            {
                cfm->status = CO_OK;
            }
        }
        else
        {
            cfm->status = p2p_go_noa_stop(p_vif_entry, param->noa_inst_nb, true);
        }
    } while (0);

    // Send the message
    ke_msg_send(cfm);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief
 ****************************************************************************************
 */
static int mm_set_p2p_oppps_req_handler(ke_msg_id_t const msgid,
                                        struct mm_set_p2p_oppps_req const *param,
                                        ke_task_id_t const dest_id,
                                        ke_task_id_t const src_id)
{
    // Allocate confirmation message
    struct mm_set_p2p_oppps_cfm *cfm = KE_MSG_ALLOC(MM_SET_P2P_OPPPS_CFM, TASK_API,
                                                    TASK_MM, mm_set_p2p_oppps_cfm);

    cfm->status    = CO_FAIL;

    do
    {
        struct vif_info_tag *p_vif_entry;

        if (param->vif_index >= NX_VIRT_DEV_MAX)
        {
            break;
        }

        p_vif_entry = &vif_info_tab[param->vif_index];

        if (!p_vif_entry || !p_vif_entry->p2p)
        {
            break;
        }

        cfm->status = CO_OK;

        if (param->ctwindow)
        {
            // Start OPPPS procedure
            p2p_go_oppps_start(p_vif_entry, param->ctwindow);
        }
        else
        {
            p2p_go_oppps_stop(p_vif_entry);
        }
    } while (0);

    // Send the message
    ke_msg_send(cfm);

    return (KE_MSG_CONSUMED);
}
#endif //(NX_P2P_GO)

#if (RW_BFMER_EN)
/**
 ****************************************************************************************
 * @brief
 ****************************************************************************************
 */
static int mm_bfmer_enable_req_handler(ke_msg_id_t const msgid,
                                       struct mm_bfmer_enable_req const *param,
                                       ke_task_id_t const dest_id,
                                       ke_task_id_t const src_id)
{
    // Check that connection exists
    if (bfr_is_enabled() && (sta_info_tab[param->sta_idx].staid != INVALID_STA_IDX))
    {
        bfr_add_sta_ind(param->sta_idx, param->vht_mu_bfmee, param->aid, param->host_bfr_addr,
                                 param->rx_nss);
    }

    return (KE_MSG_CONSUMED);
}
#endif //(RW_BFMER_EN)

#if (RW_MUMIMO_TX_EN)
/**
 ****************************************************************************************
 * @brief
 ****************************************************************************************
 */
static int mm_mu_group_update_req_handler(ke_msg_id_t const msgid,
                                          struct mm_mu_group_update_req const *param,
                                          ke_task_id_t const dest_id,
                                          ke_task_id_t const src_id)
{
    // Update the peer
    bfr_group_update_req(param);

    // Freeing done when the request is confirmed to the host
    return (KE_MSG_NO_FREE);
}
#endif //(RW_MUMIMO_TX_EN)

static
int mm_cfg_rssi_req_handler(ke_msg_id_t const msgid,
                            struct mm_cfg_rssi_req const *param,
                            ke_task_id_t const dest_id,
                            ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_index];

    // Sanity check - These parameters apply only for a STA interface
    ASSERT_ERR(vif_entry->type == VIF_STA);

    // Set the parameters
    vif_entry->u.sta.rssi_thold = param->rssi_thold;
    vif_entry->u.sta.rssi_hyst = param->rssi_hyst;
    vif_entry->u.sta.rssi_status = 0;

    return (KE_MSG_CONSUMED);
}

/*
 * TASK DESCRIPTOR DEFINITIONS
 ****************************************************************************************
 */
/// Message handlers in state DEFAULT.
const struct ke_msg_handler mm_default_state[] =
{
    // From UMAC
    {MM_START_REQ, (ke_msg_func_t)mm_start_req_handler},
    // From UMAC
    {MM_VERSION_REQ, (ke_msg_func_t)mm_version_req_handler},
    // From UMAC
    {MM_ADD_IF_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_REMOVE_IF_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_RESET_REQ, (ke_msg_func_t)mm_reset_req_handler},
    // From UMAC
    {MM_SET_CHANNEL_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_BASIC_RATES_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_BEACON_INT_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_DTIM_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_FILTER_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_BSSID_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_EDCA_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_SLOTTIME_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_MODE_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_VIF_STATE_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_DBG_TRIGGER_REQ, (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_SET_IDLE_REQ, (ke_msg_func_t)mm_set_idle_req_handler},
    // From internal
    {MM_FORCE_IDLE_REQ, (ke_msg_func_t)mm_force_idle_req_handler},
    // From UMAC
    {MM_SET_POWER_REQ, (ke_msg_func_t)mm_set_power_req_handler},
    // From UMAC
    {MM_KEY_ADD_REQ, (ke_msg_func_t)mm_key_add_req_handler},
    // From UMAC
    {MM_KEY_DEL_REQ, (ke_msg_func_t)mm_key_del_req_handler},
    // From UMAC
    {MM_STA_ADD_REQ, (ke_msg_func_t)mm_sta_add_req_handler},
    // From UMAC
    {MM_STA_DEL_REQ, (ke_msg_func_t)mm_sta_del_req_handler},

    // From UMAC
    {MM_BA_ADD_REQ,  (ke_msg_func_t)mm_hw_config_handler},
    #if (NX_AMPDU_TX || NX_REORD)
    // From UMAC
    {MM_BA_DEL_REQ,  (ke_msg_func_t)mm_ba_del_req_handler},
    #endif

    #if (NX_CHNL_CTXT)
    // From UMAC
    {MM_CHAN_CTXT_UPDATE_REQ,  (ke_msg_func_t)mm_hw_config_handler},
    // From UMAC
    {MM_REMAIN_ON_CHANNEL_REQ,  (ke_msg_func_t)mm_remain_on_channel_req_handler},
    #endif //(NX_CHNL_CTXT)

    #if NX_BCN_AUTONOMOUS_TX
    // From UMAC
    {MM_BCN_CHANGE_REQ,  (ke_msg_func_t)mm_bcn_change_req_handler},
    // From UMAC
    {MM_TIM_UPDATE_REQ,  (ke_msg_func_t)mm_tim_update_req_handler},
    #endif

    #if NX_POWERSAVE
    // From UMAC
    {MM_SET_PS_MODE_REQ, (ke_msg_func_t)mm_set_ps_mode_req_handler},
    // From UMAC
    {MM_SET_PS_OPTIONS_REQ, (ke_msg_func_t)mm_set_ps_options_req_handler},
    #endif

    #if (NX_P2P_GO)
    // From UMAC
    {MM_SET_P2P_NOA_REQ,   (ke_msg_func_t)mm_set_p2p_noa_req_handler},
    // From UMAC
    {MM_SET_P2P_OPPPS_REQ, (ke_msg_func_t)mm_set_p2p_oppps_req_handler},
    #endif //(NX_P2P_GO)

    #if (RW_BFMER_EN)
    // From UMAC
    {MM_BFMER_ENABLE_REQ, (ke_msg_func_t)mm_bfmer_enable_req_handler},
    #endif //(RW_BFMER_EN)
    #if (RW_MUMIMO_TX_EN)
    // From UMAC
    {MM_MU_GROUP_UPDATE_REQ, (ke_msg_func_t)mm_mu_group_update_req_handler},
    #endif //(RW_MUMIMO_TX_EN)

    {MM_CFG_RSSI_REQ, (ke_msg_func_t)mm_cfg_rssi_req_handler},
};

/// Specifies the message handler structure for every input state of STA State Machine
const struct ke_state_handler mm_state_handler[MM_STATE_MAX] =
{
    /// IDLE State message handlers.
    KE_STATE_HANDLER_NONE,   // [MM_IDLE] = 
    /// JOIN State message handlers.
    KE_STATE_HANDLER_NONE    // [MM_ACTIVE] = 
};

/// Specifies the message handlers that are common to all states.
const struct ke_state_handler mm_default_handler =
    KE_STATE_HANDLER(mm_default_state);

/// Defines the placeholder for the states of all the task instances.
ke_state_t mm_state[MM_IDX_MAX];

/// @} end of group
