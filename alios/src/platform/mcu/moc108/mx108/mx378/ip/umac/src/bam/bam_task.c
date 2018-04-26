/**
 ****************************************************************************************
 *
 * @file bam_task.c
 *
 * @brief UMAC ME BAM (Block ACK Manager) module state machine.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

// for endian functions
#include "co_endian.h"
// for co read/write
#include "co_utils.h"
// for BA type
#include "mac_defs.h"
#include "bam.h"
#include "bam_task.h"
#include "mac_frame.h"
#include "me_mgmtframe.h"
#include "ke_timer.h"
#include "mm.h"
#include "mm_task.h"
#include "rxu_task.h"
#include "sta_mgmt.h"
#include "me.h"
#include "ps.h"

/**
 * @addtogroup TASK_BAM
 * @{
 */

/*
 * API MESSAGES HANDLERS
 ****************************************************************************************
 */

#if NX_AMPDU_TX
/**
 ****************************************************************************************
 * @brief BAM module message handler.
 * This function handles the  BAM_ADD_BA_RSP_TIMEOUT_IND message.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int bam_add_ba_rsp_timeout_ind_handler(ke_msg_id_t const msgid,
                                              void const *param,
                                              ke_task_id_t const dest_id,
                                              ke_task_id_t const src_id)
{
    if (ke_state_get(dest_id) == BAM_WAIT_RSP)
    {
        uint16_t bam_idx = KE_IDX_GET(dest_id);

        // Reset information store in the STA_MGMT module
        bam_param_sta_info_tab_reset(bam_idx);

        // Go back to IDLE state
        ke_state_set(dest_id, BAM_IDLE);
    }
    // else drop the message

    return (KE_MSG_CONSUMED);
}
#endif

#if NX_AMPDU_TX || NX_REORD
/**
 ****************************************************************************************
 * @brief BAM module message handler.
 * This function handles the  BAM_INACTIVITY_TIMEOUT_IND message.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int bam_inactivity_timeout_ind_handler(ke_msg_id_t const msgid,
                                              void const *param,
                                              ke_task_id_t const dest_id,
                                              ke_task_id_t const src_id)
{

    if (ke_state_get(dest_id) == BAM_ACTIVE)
    {
        uint16_t bam_idx = KE_IDX_GET(dest_id);
        struct bam_env_tag *bam = &bam_env[bam_idx];

        // Check if the BA agreement has been inactive for the timeout duration
        if (!bam_env[bam_idx].pkt_cnt &&
             hal_machw_time_past(bam->last_activity_time + bam->ba_timeout * TU_DURATION))
        {
            uint16_t sta_idx = bam_env[bam_idx].sta_idx;


            // Send DELBA packet
            bam_send_air_action_frame(sta_idx, &bam_env[bam_idx], MAC_BA_ACTION_DELBA, 0, 0, MAC_RS_TIMEOUT);

            // Send MM_BA_DEL_REQ to LMAC
            bam_send_mm_ba_del_req(sta_idx, bam_idx);
        }
        else
        {
            // Re-start the timer
            bam_start_inactivity_timer(bam_idx);
        }
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief BAM module message handler.
 * This function handles the MM_BAADD_CFM message from LMAC.
 *
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int mm_ba_add_cfm_handler(ke_msg_id_t const msgid,
                                 struct mm_ba_add_cfm const *param,
                                 ke_task_id_t const dest_id,
                                 ke_task_id_t const src_id)
{
    uint16_t bam_idx = KE_IDX_GET(dest_id);

    do
    {
        if (ke_state_get(dest_id) != BAM_CHECK_ADMISSION)
        {
            ASSERT_WARN(0);
            break;
        }

        // Check the device type
        switch (bam_env[bam_idx].dev_type)
        {
            #if NX_AMPDU_TX
            case BA_ORIGINATOR:
            {
                if (param->status == BA_AGMT_ESTABLISHED)
                {
                    int8_t credit_oft = (int8_t)bam_env[bam_idx].buffer_size -
                                                        NX_DEFAULT_TX_CREDIT_CNT;

                    // Initialize the BAW
                    bam_baw_init(&bam_env[bam_idx]);

                    // Initialize the activity timer
                    bam_env[bam_idx].last_activity_time = hal_machw_time();

                    // Update the credit offset for this STA/TID pair
                    //sta_mgmt_set_credit_oft(param->sta_idx, param->tid, credit_oft);
                    me_credits_update_ind(param->sta_idx, param->tid, credit_oft);

                    // Start the activity timeout timer
                    bam_start_inactivity_timer(bam_idx);

                    // Go back to ACTIVE state
                    ke_state_set(dest_id, BAM_ACTIVE);
                }
                else
                {
                    // Clear the info of the BA in the sta_info_tab and mac_ba_param
                    bam_param_sta_info_tab_reset(bam_idx);

                    // Go back to IDLE state
                    ke_state_set(dest_id, BAM_IDLE);
                }
            } break;
            #endif

            #if NX_REORD
            case BA_RESPONDER:
            {
                if (param->status == BA_AGMT_ESTABLISHED)
                {
                    // Send AIR_ADDBA RSP frame to the originator.
                    bam_send_air_action_frame(bam_env[bam_idx].sta_idx, &bam_env[bam_idx],
                                              MAC_BA_ACTION_ADDBA_RSP,
                                              bam_env[bam_idx].dialog_token,
                                              bam_build_baparamset(&bam_env[bam_idx]),
                                              MAC_BA_ST_SUCCESS);

                    // Save the BAM Task index
                    sta_info_tab[param->sta_idx].ba_info[param->tid].bam_idx_rx = bam_idx;

                    // Initialize the activity timer
                    bam_env[bam_idx].last_activity_time = hal_machw_time();

                    // Start the activity timeout timer
                    bam_start_inactivity_timer(bam_idx);

                    // Go back to ACTIVE state
                    ke_state_set(dest_id, BAM_ACTIVE);
                }
                else
                {
                    // Send AIR_ADDBA RSP frame to the originator.
                    bam_send_air_action_frame(bam_env[bam_idx].sta_idx, &bam_env[bam_idx],
                                              MAC_BA_ACTION_ADDBA_RSP,
                                              bam_env[bam_idx].dialog_token,
                                              bam_build_baparamset(&bam_env[bam_idx]),
                                              MAC_BA_ST_REQUEST_REFUSED);

                    // Go back to IDLE state
                    ke_state_set(dest_id, BAM_IDLE);
                }
            } break;
            #endif

            default:
            {
                // Should not happen
                ASSERT_WARN(0);
            } break;
        }
    } while (0);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief BAM module message handler.
 * This function handles the MM_BADEL_CFM message from LMAC.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int mm_ba_del_cfm_handler(ke_msg_id_t const msgid,
                                 struct mm_ba_del_cfm const *param,
                                 ke_task_id_t const dest_id,
                                 ke_task_id_t const src_id)
{
    uint16_t bam_idx = KE_IDX_GET(dest_id);

    if (param->status == BA_AGMT_DELETED)
    {
        #if NX_AMPDU_TX
        if (bam_env[bam_idx].dev_type == BA_ORIGINATOR)
        {
            int8_t credit_oft = NX_DEFAULT_TX_CREDIT_CNT -
                                  (int8_t)bam_env[bam_idx].buffer_size +
                                  bam_flush_baw(&bam_env[bam_idx].baw);

            // Update the credit offset for this STA/TID pair
            //sta_mgmt_set_credit_oft(param->sta_idx, param->tid, credit_oft);
            me_credits_update_ind(param->sta_idx, param->tid, credit_oft);
        }
        #endif

        bam_delete_ba_agg(bam_idx);
    }

    return (KE_MSG_CONSUMED);
}
#endif

static int rxu_mgt_ind_handler(ke_msg_id_t const msgid,
                               struct rxu_mgt_ind const *param,
                               ke_task_id_t const dest_id,
                               ke_task_id_t const src_id)
{
    // Pointer used to read the RX frame
    uint32_t frame = CPU2HW(param->payload);
    uint8_t action = co_read8p(frame + MAC_ACTION_ACTION_OFT);

    // Call correct frame handler
    switch (action)
    {
        case MAC_BA_ACTION_ADDBA_REQ:
        {
			#if NX_REORD
            uint16_t bam_idx = 0;
            struct sta_info_tag *sta;
			#endif
			
            uint16_t sta_idx  = param->sta_idx;
            uint8_t dialog_token = co_read8p(frame + MAC_ACTION_TOKEN_OFT);
            uint16_t ba_param = co_read16p(frame + BAM_ADDBAREQ_BA_PARAM_OFFSET);

            #if NX_REORD
            sta = &sta_info_tab[sta_idx];
            do
            {
                uint8_t tid       = BAM_BA_PARAM_GET(ba_param, TID);

                // Check received TID
                if (tid >= TID_MAX)
                {
                    break;
                }

                // Check if a BA agreement already exists for this STA/TID pair
                if (mm_ba_agmt_rx_exists(sta_idx, tid))
                {
                    break;
                }

                // Don't accept BA agreement from stations in PS mode
                if (sta->ps_state == PS_MODE_ON)
                    break;

                // Try to alloc a new BAM environment
                bam_idx = bam_alloc_new_task();

                if (bam_idx == BAM_INVALID_TASK_IDX)
                {
                    // No more Block Ack agreement can be handled
                    break;
                }

                bam_env[bam_idx].tid              = tid;
                bam_env[bam_idx].sta_idx          = sta_idx;
                bam_env[bam_idx].dialog_token     = dialog_token;
                bam_env[bam_idx].ba_policy        = BAM_BA_PARAM_GET(ba_param, BA_POL);
                bam_env[bam_idx].dev_type         = BA_RESPONDER;
                bam_env[bam_idx].ba_timeout       = co_read16p(frame + BAM_ADDBAREQ_BA_TIMEOUT_OFFSET);
                bam_env[bam_idx].buffer_size      = co_min(BAM_BA_PARAM_GET(ba_param, BUFF_SIZE), NX_REORD_BUF_SIZE);
                bam_env[bam_idx].ssn              = co_read16p(frame + BAM_ADDBAREQ_BA_SSC_OFFSET)>>MAC_SEQCTRL_NUM_OFT;
                bam_env[bam_idx].pkt_cnt          = 0;

                if (!bam_env[bam_idx].ba_timeout)
                {
                    // Force inactivity timeout value a non null value
                    bam_env[bam_idx].ba_timeout = BAM_INACTIVITY_TO_DURATION;
                }

                // Send MM_BA_ADD_REQ message to LMAC
                bam_send_mm_ba_add_req(sta_idx, bam_idx);

                // Go to BAM_CHECK_ADMISSION state
                ke_state_set(KE_BUILD_ID(TASK_BAM, bam_idx), BAM_CHECK_ADMISSION);

                return (KE_MSG_CONSUMED);
            } while (0);
            #endif

            // If we reach this point, an error has occurred, reply with an error status
            bam_send_air_action_frame(sta_idx, NULL, MAC_BA_ACTION_ADDBA_RSP, dialog_token,
                                      ba_param, MAC_BA_ST_REQUEST_REFUSED);
            break;
        }

        #if NX_AMPDU_TX
        case MAC_BA_ACTION_ADDBA_RSP:
        {
            uint16_t status;
            uint16_t ba_param;
            uint8_t bam_idx;
            uint8_t tid;

            do
            {
                ba_param = co_read16p(frame + BAM_ADDBARSP_BA_PARAM_OFFSET);
                tid      = BAM_BA_PARAM_GET(ba_param, TID);

                // Check TID
                if (tid >= TID_MAX)
                {
                    // Drop the packet
                    status = MAC_BA_ST_SUCCESS;
                    break;
                }

                // Look for the BAM instance
                bam_idx = sta_mgmt_get_tx_bam_idx(param->sta_idx, tid);

                if (bam_idx == BAM_INVALID_TASK_IDX)
                {
                    // Drop the packet
                    status = MAC_BA_ST_SUCCESS;
                    break;
                }

                status = MAC_BA_ST_INVALID_PARAMETERS;

                // Stop ADDBA response timeout timer
                ke_timer_clear(BAM_ADD_BA_RSP_TIMEOUT_IND, KE_BUILD_ID(TASK_BAM, bam_idx));

                // Check Dialog Token
                if (co_read8p(frame + MAC_ACTION_TOKEN_OFT) != bam_env[bam_idx].dialog_token)
                {
                    status = MAC_BA_ST_INVALID_PARAMETERS;
                    break;
                }

                // Check if received TID matches with the sent one
                if (tid != bam_env[bam_idx].tid)
                {
                    status = MAC_BA_ST_INVALID_PARAMETERS;
                    break;
                }

                status = co_read16p(frame + BAM_ADDBARSP_STATUS_OFFSET);

                // Check received status
                if (status != MAC_BA_ST_SUCCESS)
                {
                    break;
                }

                /*
                 * 9.21.2 - If the value in the Buffer Size field of the ADDBA Response is smaller than
                 * the value in the ADDBA Request frame, the originator shall change the size of its
                 * transmission window so that it is not greater than the value in the Buffer Size field
                 * of the ADDBA Response frame and is not greater than the value 64.
                 */
                if (BAM_BA_PARAM_GET(ba_param, BUFF_SIZE) < bam_env[bam_idx].buffer_size)
                {
                    bam_env[bam_idx].buffer_size = BAM_BA_PARAM_GET(ba_param, BUFF_SIZE);
                }

                bam_env[bam_idx].amsdu=BAM_BA_PARAM_GET(ba_param, AMSDU_SUP);

                // Send MM_BA_ADD_REQ message to LMAC
                bam_send_mm_ba_add_req(param->sta_idx, bam_idx);

                // Go to BAM_CHECK_ADMISSION state
                ke_state_set(KE_BUILD_ID(TASK_BAM, bam_idx), BAM_CHECK_ADMISSION);
            } while (0);

            if (status != MAC_BA_ST_SUCCESS)
            {
                /*
                 * Request has been rejected
                 *      - Clean STA Mgmt
                 *      - Go back to IDLE state
                 */
                bam_param_sta_info_tab_reset(bam_idx);

                ke_state_set(KE_BUILD_ID(TASK_BAM, bam_idx), BAM_IDLE);
            }
            break;
        }
        #endif

        #if NX_AMPDU_TX || NX_REORD
        case MAC_BA_ACTION_DELBA:
        {
            uint16_t delba_param = co_read16p(frame + BAM_DELBA_PARAM_OFFSET);
            uint8_t tid          = BAM_DELBA_PARAM_GET(delba_param, TID);
            uint16_t bam_idx;

            do
            {
                // Check received TID
                if ((tid >= TID_MAX) || (param->sta_idx == INVALID_STA_IDX))
                {
                    break;
                }

                // Check if the indicated BA exists
                if (BAM_DELBA_PARAM_GET(delba_param, INITIATOR))
                {
                    bam_idx = sta_mgmt_get_tx_bam_idx(param->sta_idx, tid);
                }
                else
                {
                    bam_idx = sta_mgmt_get_rx_bam_idx(param->sta_idx, tid);
                }

                if (bam_idx == BAM_INVALID_TASK_IDX)
                {
                    break;
                }

                // Stop the inactivity timeout timer
                ke_timer_clear(BAM_INACTIVITY_TIMEOUT_IND, KE_BUILD_ID(TASK_BAM, bam_idx));

                // Send MM_BA_DEL_REQ to LMAC
                bam_send_mm_ba_del_req(param->sta_idx, bam_idx);
            } while (0);
            break;
        }
        #endif

        default:
            break;
    }

    return (KE_MSG_CONSUMED);
}

/*
 * PUBLIC FUNCTIONS
 ****************************************************************************************
 */

/// DEFAULT handler definition.
const struct ke_msg_handler bam_default_state[] =
{
    // From SME
    #if NX_AMPDU_TX
    {BAM_ADD_BA_RSP_TIMEOUT_IND,    (ke_msg_func_t)bam_add_ba_rsp_timeout_ind_handler},
    #endif
    #if NX_AMPDU_TX || NX_REORD
    {BAM_INACTIVITY_TIMEOUT_IND,    (ke_msg_func_t)bam_inactivity_timeout_ind_handler},
    {MM_BA_DEL_CFM,                 (ke_msg_func_t)mm_ba_del_cfm_handler},
    {MM_BA_ADD_CFM,                 (ke_msg_func_t)mm_ba_add_cfm_handler},
    #endif
    {RXU_MGT_IND,                   (ke_msg_func_t)rxu_mgt_ind_handler},
};

/// Message handlers that are common to all states.
const struct ke_state_handler bam_default_handler = KE_STATE_HANDLER(bam_default_state);

/// place holder for the states of the BAM task.
ke_state_t bam_state[BAM_IDX_MAX];

/// @} end of addtogroup
