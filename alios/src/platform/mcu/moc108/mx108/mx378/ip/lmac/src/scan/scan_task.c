/**
 ****************************************************************************************
 *
 * @file scan_task.c
 *
 * @brief Task responsible for scanning process.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 *****************************************************************************************
 * @addtogroup SCAN_TASK
 * @{
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_endian.h"

#include "scan.h"
#include "scan_task.h"
#include "phy.h"
#include "mm.h"

#include "include.h"
#include "uart_pub.h"

#if NX_HW_SCAN
/*
 * MESSAGE HANDLERS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief SCAN start request message handler.
 * This function handles the @SCAN_START_REQ message, which starts the scanning procedure.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
scan_start_req_handler(ke_msg_id_t const msgid,
                       struct scan_start_req const *param,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    int msg_status = KE_MSG_CONSUMED;

    // Allocate the response message
    struct scan_start_cfm *msg = KE_MSG_ALLOC(SCAN_START_CFM, src_id, dest_id,
                                              scan_start_cfm);
	os_printf("scan_start_req_handler\r\n");
    // Check the current state of the scanning procedure
    if (ke_state_get(TASK_SCAN) == SCAN_IDLE)
    {
        // Sanity check - at least one channel should be requested
        ASSERT_ERR(param->chan_cnt > 0);

        // No scan ongoing, we will proceed to the request
        msg->status = CO_OK;

        // Initialize the scan parameters
        scan_env.chan_idx = 0;
        scan_env.req_id = src_id;
        scan_env.param = param;

        // Prepare the IE buffer of the Probe Request
        scan_ie_download(param);

        // Message will be freed at the end of the scanning procedure
        msg_status = KE_MSG_NO_FREE;
    }
    else
    {
        // We are currently in a scan process, so reject the current request
        msg->status = CO_BUSY;
    }

    // Send the response
    ke_msg_send(msg);

    //  Return the message status.
    return (msg_status);
}

/**
 ****************************************************************************************
 * @brief SCAN cancel request message handler.
 * This function handles the @SCAN_CANCEL_REQ message, which starts the scanning procedure.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
scan_cancel_req_handler(ke_msg_id_t const msgid,
                        void const *param,
                        ke_task_id_t const dest_id,
                        ke_task_id_t const src_id)
{
    ke_state_t scan_state = ke_state_get(TASK_SCAN);

    // Check if a scan process is in progress
    if (scan_state != SCAN_IDLE)
    {
    	os_printf("scan_cancel_req_handler\r\n");
        scan_env.abort = true;
    }
    else
    {
        // Directly send the confirmation with an error status
    	os_printf("scan_send_cancel_cfm_fail\r\n");
        scan_send_cancel_cfm(CO_FAIL, src_id);
    }

    //  Return the message status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM set scan channel confirmation handler.
 * This function handles the @MM_SCAN_CHANNEL_START_IND message, which confirms the switch
 * to the scanned channel.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_scan_channel_start_ind_handler(ke_msg_id_t const msgid,
                                  void const *param,
                                  ke_task_id_t const dest_id,
                                  ke_task_id_t const src_id)
{
    // Sanity check
    ASSERT_ERR(ke_state_get(TASK_SCAN) == SCAN_WAIT_CHANNEL);

    // Configure the RX filter to receive beacons and probe responses
    mm_rx_filter_lmac_enable_set(NXMAC_ACCEPT_ALL_BEACON_BIT | NXMAC_ACCEPT_PROBE_RESP_BIT);

    // Prepare and transmit the Probe Request(s)
    scan_probe_req_tx();
	scan_probe_req_tx();
	scan_probe_req_tx();

    // Change the state of the task
    ke_state_set(TASK_SCAN, SCAN_WAIT_BEACON_PROBE_RSP);

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief MM set scan channel confirmation handler.
 * This function handles the @MM_SCAN_CHANNEL_END_IND message, which confirms the switch
 * to the scanned channel.
 *
 * @param[in] msgid Id of the message received.
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_scan_channel_end_ind_handler(ke_msg_id_t const msgid,
                                void const *param,
                                ke_task_id_t const dest_id,
                                ke_task_id_t const src_id)
{
    // Sanity check
    ASSERT_ERR(ke_state_get(TASK_SCAN) == SCAN_WAIT_BEACON_PROBE_RSP);

    // Configure the RX filter to disable beacons and probe responses reception
    mm_rx_filter_lmac_enable_clear(NXMAC_ACCEPT_ALL_BEACON_BIT | NXMAC_ACCEPT_PROBE_RESP_BIT);

    // Increase the channel index
    scan_env.chan_idx++;

    // Check if we still have channels to scan, or if abort has been requested
    if ((scan_env.chan_idx < scan_env.param->chan_cnt) && !scan_env.abort)
    {
        // Send the new set channel request
        scan_set_channel_request();
    }
    else
    {
        // Free the scan parameter buffer
        ke_msg_free(ke_param2msg(scan_env.param));

        if (scan_env.abort)
        {
        	os_printf("scan_send_cancel_cfm\r\n");
            // Send the scan end indication to the host
            scan_send_cancel_cfm(CO_OK, scan_env.req_id);

            scan_env.abort = false;
        }
        else
        {
            // Send the scan end indication to the host
            //os_printf("SCAN_DONE_IND\r\n");
            ke_msg_send_basic(SCAN_DONE_IND, scan_env.req_id, TASK_SCAN);
        }

        // Put back the state machine to IDLE
        ke_state_set(TASK_SCAN, SCAN_IDLE);
    }

    //  Return the KE_MSG_CONSUMED status.
    return (KE_MSG_CONSUMED);
}

/*
 * TASK DESCRIPTOR DEFINITIONS
 ****************************************************************************************
 */
/// Message handlers in state DEFAULT.
const struct ke_msg_handler scan_default_state[] =
{
    // From UMAC
    {SCAN_START_REQ,            (ke_msg_func_t)scan_start_req_handler},
    // From MM
    {MM_SCAN_CHANNEL_START_IND, (ke_msg_func_t)mm_scan_channel_start_ind_handler},
    // From MM
    {MM_SCAN_CHANNEL_END_IND,   (ke_msg_func_t)mm_scan_channel_end_ind_handler},
    // From UMAC
    {SCAN_CANCEL_REQ,           (ke_msg_func_t)scan_cancel_req_handler},
};

/// Specifies the message handlers that are common to all states.
const struct ke_state_handler scan_default_handler =
    KE_STATE_HANDLER(scan_default_state);

/// Defines the placeholder for the states of all the task instances.
ke_state_t scan_state[SCAN_IDX_MAX];
#endif

/// @} end of group
