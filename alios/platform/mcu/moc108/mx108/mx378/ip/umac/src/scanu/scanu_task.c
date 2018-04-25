/**
 ****************************************************************************************
 *
 * @file scanu_task.c
 *
 * @brief The UMAC's SCAN module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

#include "scanu_task.h"
#include "scanu.h"

#include "me.h"
#include "scan_task.h"
#include "vif_mgmt.h"
#include "rxu_task.h"

/** @addtogroup TASK_SCANU
 * @{
 */


#if (NX_ROAMING)
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
scanu_fast_req_handler(ke_msg_id_t const msgid,
                       struct scanu_fast_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // build a channel list with only one channel
    // record the SCAN information in the environment
    //   - save if it is an fast or slow scan
    scanu_env.fast = true;
    //   - the type of scan (active or passive)
    scanu_env.activescan = true;
    //   - store the source task ID to use when confirming the SCAN
    scanu_env.src_id = src_id;
    //   - the BSSID and SSID to scan for
    scanu_env.bssid = param->bssid;
    scanu_env.ssid = param->ssid;
    //   - the scan times
    scanu_env.minch_time = param->minch_time;
    scanu_env.maxch_time = param->maxch_time;
    //   - the list of channels
    // check if there is a channel list specified
    scanu_env.chlist.nbr = 1;
    scanu_env.chlist.list[0] = param->ch_nbr;

    // start scanning
    scanu_start();
    return (KE_MSG_CONSUMED);
}
#endif


/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the scan request message from any module.
 * This message Requests the STA to perform active or passive scan.
 * If a scan is requested within two seconds of the last scan, no scan is performed
 * and the last scan results are sent to the requester. The time at which the last
 * scan was performed is stored in the time_stamp element in me_scan_context.
 * If a scan should be performed, the following is done:
 *   - Store the important parameters in the SCAN context.
 *   - Decrease the number of channels to be scanned.
 *   - Send a scan request to the LMAC.
 *   - Move to the state SCANNING to wait the CFM and send the next request.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
scanu_start_req_handler(ke_msg_id_t const msgid,
                        struct scanu_start_req const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    // record the SCAN information in the environment
    //   - save if it is a fast or slow scan
    scanu_env.fast = false;
    scanu_env.joining = false;
    scanu_env.param = param;
    scanu_env.src_id = src_id;
    scanu_env.band = PHY_BAND_2G4;
    scanu_env.bssid = param->bssid;

    // start scanning
    scanu_start();

    return (KE_MSG_NO_FREE);
}

/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the scan request message from any module.
 * This message Requests the STA to perform active or passive scan.
 * If a scan is requested within two seconds of the last scan, no scan is performed
 * and the last scan results are sent to the requester. The time at which the last
 * scan was performed is stored in the time_stamp element in me_scan_context.
 * If a scan should be performed, the following is done:
 *   - Store the important parameters in the SCAN context.
 *   - Decrease the number of channels to be scanned.
 *   - Send a scan request to the LMAC.
 *   - Move to the state SCANNING to wait the CFM and send the next request.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
scanu_join_req_handler(ke_msg_id_t const msgid,
                        struct scanu_start_req const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];
    struct mac_bss_info *bss = &vif->bss_info;

    // Reset the valid flag of the BSS information
    bss->valid_flags = 0;

    // record the SCAN information in the environment
    //   - save if it is a fast or slow scan
    scanu_env.fast = false;
    scanu_env.joining = true;
    scanu_env.param = param;
    scanu_env.src_id = src_id;
    scanu_env.band = PHY_BAND_2G4;
    scanu_env.bssid = param->bssid;

    // Sanity checks
    ASSERT_ERR(!MAC_ADDR_GROUP(&param->bssid));

    // start scanning
    scanu_start();

    return (KE_MSG_NO_FREE);
}

/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the scan confirmation message from LMAC.
 * This message confirms a scan request command after LMAC is finished with scan procedure
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
scan_start_cfm_handler(ke_msg_id_t const msgid,
                       struct scan_start_cfm const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    // Check if the scanning procedure was correctly started by LMAC
    if (param->status != CO_OK)
    {
        scanu_confirm(param->status);
    }

    return (KE_MSG_CONSUMED);
}


/**
 ****************************************************************************************
 * @brief SCAN module message handler.
 * This function handles the scan confirmation message from LMAC.
 * This message confirms a scan request command after LMAC is finished with scan procedure
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
scan_done_ind_handler(ke_msg_id_t const msgid,
                      void const *param,
                      ke_task_id_t const dest_id,
                      ke_task_id_t const src_id)
{
    // Update the band
    scanu_env.band++;

    // Scan the next band
    scanu_scan_next();

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
    int msg_status;

    // Call frame handler
    msg_status = scanu_frame_handler(param);

    return (msg_status);
}

/// Specifies the messages handled in idle state.
const struct ke_msg_handler scanu_idle[] =
{
    {SCANU_START_REQ, (ke_msg_func_t)scanu_start_req_handler},
    {SCANU_JOIN_REQ, (ke_msg_func_t)scanu_join_req_handler},
    #if (NX_ROAMING)
    {SCANU_FAST_REQ, (ke_msg_func_t)scanu_fast_req_handler},
    #endif

};

/// Specifies the messages handled in scanning state.
const struct ke_msg_handler scanu_scanning[] =
{
    {SCAN_START_CFM, (ke_msg_func_t)scan_start_cfm_handler},
    {SCAN_DONE_IND, (ke_msg_func_t)scan_done_ind_handler},
    {RXU_MGT_IND, (ke_msg_func_t)rxu_mgt_ind_handler},
};

/// DEFAULT handler definition.
const struct ke_msg_handler scanu_default_state[] =
{
    // if receiving scan requests while not in IDLE, save message
    {SCANU_START_REQ, ke_msg_save},
    {SCANU_JOIN_REQ, ke_msg_save},
    {SCANU_FAST_REQ, ke_msg_save},
    // if receiving probe responses or beacons out of scan, discard them
    {RXU_MGT_IND, ke_msg_discard},
};

/// Specifies the message handler structure for every input state.
const struct ke_state_handler scanu_state_handler[SCANU_STATE_MAX] =
{
    /// IDLE State message handlers.
    KE_STATE_HANDLER(scanu_idle), // [SCANU_IDLE] = 
    /// SCANNING State message handlers
    KE_STATE_HANDLER(scanu_scanning), // [SCANU_SCANNING] = 
};


/// Specifies the message handlers that are common to all states.
const struct ke_state_handler scanu_default_handler =
    KE_STATE_HANDLER(scanu_default_state);


/// Defines the place holder for the states of all the task instances.
ke_state_t scanu_state[SCANU_IDX_MAX];


/// @} end of group
