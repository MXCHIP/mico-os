/**
 ****************************************************************************************
 *
 * @file tdls_task.c
 *
 * @brief TDLS task.
 *
 * Copyright (C) RivieraWaves 2016
 *
 ****************************************************************************************
 */

/**
 *****************************************************************************************
 * @addtogroup TDLS_TASK
 * @{
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h" 
#include "co_endian.h"

// for mode
#include "mac_defs.h"
#include "mac_frame.h"
#include "tdls.h"
#include "ps.h"
#include "me.h"
#include "rxu_cntrl.h"
#include "tdls_task.h"
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
#include "mm_timer.h"
#include "chan.h"

#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#include "tpc.h"
#include "mm.h"

#if (TDLS_ENABLE)

/*
 * MESSAGE HANDLERS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief TDLS channel switch request handler.
 * This function handles the @TDLS_CHAN_SWITCH_REQ message, which enables the channel
 * switch to the TDLS target channel.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static
int tdls_chan_switch_req_handler(ke_msg_id_t const msgid,
                                 struct tdls_chan_switch_req const *param,
                                 ke_task_id_t const dest_id,
                                 ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_index];
    uint8_t status = CO_FAIL;

    // Set the parameters
    if (vif_entry->u.sta.sta_tdls && vif_entry->u.sta.sta_tdls->active)
    {
        vif_entry->u.sta.sta_tdls->initiator = param->initiator;
        MAC_ADDR_CPY(&vif_entry->u.sta.sta_tdls->peer_mac_addr, &param->peer_mac_addr);
        vif_entry->u.sta.sta_tdls->chsw_band = param->band;
        vif_entry->u.sta.sta_tdls->chsw_type = param->type;
        vif_entry->u.sta.sta_tdls->chsw_prim20_freq = param->prim20_freq;
        vif_entry->u.sta.sta_tdls->chsw_center1_freq = param->center1_freq;
        vif_entry->u.sta.sta_tdls->chsw_center2_freq = param->center2_freq;
        vif_entry->u.sta.sta_tdls->chsw_tx_power = param->tx_power;
        vif_entry->u.sta.sta_tdls->chsw_req_timer.cb = tdls_chsw_req_evt;
        vif_entry->u.sta.sta_tdls->chsw_req_timer.env = vif_entry;
        vif_entry->u.sta.sta_tdls->chsw_op_class = param->op_class;
        vif_entry->u.sta.sta_tdls->chsw_active = true;

        status = CO_OK;

        // Reset state
        ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);

        // Set next channel switch request
        mm_timer_set(&vif_entry->u.sta.sta_tdls->chsw_req_timer,
                vif_entry->tbtt_timer.time + TDLS_CHSW_REQ_DELAY_US);
    }

    // Send back the confirmation
    struct tdls_chan_switch_cfm *cfm = KE_MSG_ALLOC(TDLS_CHAN_SWITCH_CFM,
            src_id, dest_id,
            tdls_chan_switch_cfm);
    cfm->status = status;

    ke_msg_send(cfm);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief TDLS cancel channel switch request handler.
 * This function handles the @TDLS_CANCEL_CHAN_SWITCH_REQ message, which disables the
 * channel switch to the TDLS target channel.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static
int tdls_cancel_chan_switch_req_handler(ke_msg_id_t const msgid,
                                        struct tdls_chan_switch_req const *param,
                                        ke_task_id_t const dest_id,
                                        ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_index];
    uint8_t status = CO_FAIL;

    if (vif_entry->u.sta.sta_tdls)
    {
        // Clear channel switch request timer
        mm_timer_clear(&vif_entry->u.sta.sta_tdls->chsw_req_timer);
        status = CO_OK;
        vif_entry->u.sta.sta_tdls->chsw_active = false;
    }

    // Send back the confirmation
    struct tdls_chan_switch_cfm *cfm = KE_MSG_ALLOC(TDLS_CANCEL_CHAN_SWITCH_CFM,
            src_id, dest_id,
            tdls_chan_switch_cfm);
    cfm->status = status;

    // Reset state
    ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);

    ke_msg_send(cfm);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief TDLS Peer Traffic Indication request handler.
 * This function handles the @TDLS_PEER_TRAFFIC_IND_REQ message, which sends the TDLS Peer
 * Traffic Indication to the TDLS Peer, through the AP.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static
int tdls_peer_traffic_ind_req_handler(ke_msg_id_t const msgid,
                              struct tdls_peer_traffic_ind_req const *param,
                              ke_task_id_t const dest_id,
                              ke_task_id_t const src_id)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_index];
    uint8_t status = CO_FAIL;

    // Send Peer Traffic Indication frame
    status = txl_frame_send_tdls_peer_traffic_ind_frame(param, 0);

    if (status == CO_OK)
    {
        // set dialog token
        vif_entry->u.sta.sta_tdls->dialog_token = param->dialog_token;
        // set TDLS STA status
        ke_state_set(TASK_TDLS, TDLS_TRAFFIC_IND_TX);
    }

    // Send back the confirmation
    struct tdls_peer_traffic_ind_cfm *cfm = KE_MSG_ALLOC(TDLS_PEER_TRAFFIC_IND_CFM,
                                                         src_id, dest_id,
                                                         tdls_peer_traffic_ind_cfm);
    cfm->status = status;

    ke_msg_send(cfm);

    return (KE_MSG_CONSUMED);

}

/*
 * TASK DESCRIPTOR DEFINITIONS
 ****************************************************************************************
 */
/// Message handlers in state DEFAULT.
const struct ke_msg_handler tdls_default_state[] =
{
    // From UMAC
    {TDLS_CHAN_SWITCH_REQ, (ke_msg_func_t)tdls_chan_switch_req_handler},
    // From UMAC
    {TDLS_CANCEL_CHAN_SWITCH_REQ, (ke_msg_func_t)tdls_cancel_chan_switch_req_handler},
    // From UMAC
    {TDLS_PEER_TRAFFIC_IND_REQ, (ke_msg_func_t)tdls_peer_traffic_ind_req_handler},
};

/// Specifies the message handlers that are common to all states.
const struct ke_state_handler tdls_default_handler =
    KE_STATE_HANDLER(tdls_default_state);

/// Defines the placeholder for the states of all the task instances.
ke_state_t tdls_state[TDLS_IDX_MAX];

#endif // TDLS_ENABLE

/// @} end of group
