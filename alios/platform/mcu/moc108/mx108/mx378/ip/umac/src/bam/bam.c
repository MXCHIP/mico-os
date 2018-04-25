/**
 ****************************************************************************************
 *
 * @file bam.c
 *
 * @brief Functions and structures used by the UMAC ME BAM (Block ACK Manager) module.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h"
#include "mac_defs.h"
#include "bam.h"
#include "bam_task.h"
#include "mac_frame.h"
#include "me_mgmtframe.h"
#include "mm_task.h"
#include "ke_timer.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "txu_cntrl.h"
#include "ps.h"

#if NX_MFP
#include "mfp.h"
#endif

#include "tpc.h"
/**
 * @addtogroup BAM
 * @{
 */

/**
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// BAM module environment definition.
struct bam_env_tag bam_env[BAM_IDX_MAX];

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
#if NX_AMPDU_TX
#ifdef CFG_RWTL
/// Variable used for a temporary workaround for the TL4 compiler
uint16_t tl_tudiff;
#endif

/**
 *
 */
static uint16_t bam_time_get(void)
{
    // Get the current system time and convert it in TUs
    return((hal_machw_time() >> 10) & 0xFFFF);
}

/**
 ****************************************************************************************
 * @brief Compare TU timer absolute expiration time.
 *
 * @param[in] time1 First time to compare.
 * @param[in] time2 Second time to compare.
 *
 * @return true if time1 is earlier than time2.
 ****************************************************************************************
 */
static bool bam_time_cmp(uint16_t time1, uint16_t time2)
{
    uint16_t diff = time1 - time2;

    #ifdef CFG_RWTL
    /// Temporary workaround for the TL4 compiler
    tl_tudiff = diff;
    #endif

    return (((int16_t)diff) < 0);
}

/**
 ****************************************************************************************
 * @brief Compare TU timer absolute expiration time.
 *
 * @param[in] time Time to compare.
 *
 * @return true if time is in the past, false otherwise
 ****************************************************************************************
 */
static bool bam_time_past(uint32_t time)
{
    return (bam_time_cmp(time, bam_time_get()));
}


/**
 *
 */
static int8_t bam_move_baw(struct bam_baw *baw)
{
    int8_t credits = 0;

    while (baw->states[baw->fsn_idx] == BAW_CONFIRMED) {
        baw->states[baw->fsn_idx] = BAW_FREE;
        baw->fsn = (baw->fsn + 1) & ((1 << 12) - 1);
        baw->fsn_idx = (baw->fsn_idx + 1) % baw->buf_size;
        credits++;
    }

    return(credits);
}

#ifdef BAM_CHECK_ROUTINE
static bool bam_is_first_in_baw(struct bam_baw *baw, uint16_t sn)
{
    return sn == baw->fsn;
}

static inline bool bam_is_last_in_baw(struct bam_baw *baw, uint16_t sn)
{
    uint16_t last_sn = (baw->fsn + baw->buf_size - 1) & ((1 << 12) - 1);

    return (sn == last_sn);
}
#endif

/**
 *
 */
static void bam_set_baw_state(struct bam_baw *baw, uint16_t sn, uint8_t state)
{
    uint16_t baw_offset;
    int index;

    baw_offset = ((uint16_t)(sn - baw->fsn)) & ((1 << 12) - 1);

    ASSERT_ERR(baw_offset < baw->buf_size);

    index = (baw_offset + baw->fsn_idx) % baw->buf_size;

    baw->states[index] = state;
}

/**
 *
 */
static int8_t bam_check_tx_baw(struct txdesc *txdesc, uint8_t bam_idx,
                               bool success)
{
    struct bam_baw *baw = &bam_env[bam_idx].baw;
    struct tx_cfm_tag *cfm = &txdesc->lmac.hw_desc->cfm;
    struct hostdesc *host = &txdesc->host;

    do
    {
        // First check if the MPDU was part of an A-MPDU
        if (!(txdesc->umac.flags & AMPDU_BIT))
            break;

        // Then check if the MPDU was successful or not
        if (!success && !bam_time_past(host->timestamp))
        {
            // Update the confirmation descriptor
            cfm->status |= TX_STATUS_RETRY_REQUIRED;
            cfm->pn[0] = host->pn[0];
            cfm->pn[1] = host->pn[1];
            cfm->pn[2] = host->pn[2];
            cfm->pn[3] = host->pn[3];
            cfm->sn = host->sn;
            cfm->timestamp = host->timestamp;
            return 0;
        }
    } while (0);

    // Update the state in the BAW
    bam_set_baw_state(baw, host->sn, BAW_CONFIRMED);

    // Move the transmission window
    return (bam_move_baw(baw));
}

static uint8_t bam_create_ba_agg(uint8_t sta_idx, uint8_t tid, uint16_t ssn)
{
    // Try to allocate a new BAM environment
    uint8_t bam_idx = bam_alloc_new_task();
    // Returned error code
    uint8_t error_code;

    do
    {
        if (bam_idx == BAM_INVALID_TASK_IDX)
        {
            error_code = CO_FULL;
            break;
        }

        error_code = CO_OK;

        // Store the useful information in the environment
        bam_env[bam_idx].sta_idx          = sta_idx;
        bam_env[bam_idx].tid              = tid;
        bam_env[bam_idx].dialog_token     = co_rand_byte();
        bam_env[bam_idx].ba_timeout       = BAM_INACTIVITY_TO_DURATION;
        bam_env[bam_idx].ba_policy        = 1;
        bam_env[bam_idx].dev_type         = BA_ORIGINATOR;
        bam_env[bam_idx].buffer_size      = 64;
        bam_env[bam_idx].ssn              = ssn;
        bam_env[bam_idx].pkt_cnt          = 0;

        // Set the BAM Task Index and device type in the lmac_sta_info_tag
        sta_info_tab[sta_idx].ba_info[tid].bam_idx_tx = bam_idx;

        // Send ADDBA Request packet
        bam_send_air_action_frame(sta_idx, &bam_env[bam_idx], MAC_BA_ACTION_ADDBA_REQ, 0, 0, 0);

        // Store current time
        sta_mgmt_set_add_ba_time(sta_idx, tid, ke_time());

        // Start the ADDBA RSP Timeout timer
        ke_timer_set(BAM_ADD_BA_RSP_TIMEOUT_IND, KE_BUILD_ID(TASK_BAM, bam_idx),
                     (BAM_RESPONSE_TO_DURATION * TU_DURATION));

        // Set state as wait_rsp
        ke_state_set(KE_BUILD_ID(TASK_BAM, bam_idx), BAM_WAIT_RSP);
    } while (0);

    return (error_code);
}
#endif

void bam_init(void)
{
    uint8_t bam_idx;
    
    for (bam_idx = 0; bam_idx < BAM_IDX_MAX; bam_idx++)
    {
        bam_env[bam_idx].sta_idx     = INVALID_STA_IDX;

        ke_state_set(KE_BUILD_ID(TASK_BAM, bam_idx), BAM_IDLE);
    }
}

#if NX_AMPDU_TX || NX_REORD
void bam_param_sta_info_tab_reset(uint16_t bam_idx)
{
    uint16_t sta_idx = bam_env[bam_idx].sta_idx;
    uint8_t tid      = bam_env[bam_idx].tid;

    // Initialize the parameters of the BA in the station info table
    switch (bam_env[bam_idx].dev_type)
    {
        case BA_RESPONDER:
            sta_info_tab[sta_idx].ba_info[tid].bam_idx_rx = BAM_INVALID_TASK_IDX;
            break;

        case BA_ORIGINATOR:
            sta_info_tab[sta_idx].ba_info[tid].bam_idx_tx = BAM_INVALID_TASK_IDX;
            break;

        default:
            break;
    }
}

uint16_t bam_alloc_new_task(void)
{
    // Loop on the BAM IDX (Instance 0 is BAM Manager)
    uint8_t i;
    
    for (i = 0; i < BAM_IDX_MAX; i++)
    {
        // Check if the BAM task is Idle or not
        if (bam_state[i] == BAM_IDLE)
        {
            // Return bam_task_idx
            return i;
        }
    }

    return BAM_INVALID_TASK_IDX;
}

void bam_delete_ba_agg(uint8_t bam_idx)
{
    // BAM Task id
    ke_task_id_t task_id = KE_BUILD_ID(TASK_BAM, bam_idx);

    // Stop Inactivity Timeout timer
    ke_timer_clear(BAM_ADD_BA_RSP_TIMEOUT_IND, task_id);
    // Stop Inactivity Timeout timer
    ke_timer_clear(BAM_INACTIVITY_TIMEOUT_IND, task_id);

    // Clear information stored in the STA_MGMT module
    bam_param_sta_info_tab_reset(bam_idx);

    // Go back to idle state
    ke_state_set(task_id, BAM_IDLE);
}

void bam_delete_all_ba_agg(uint8_t sta_idx)
{
    uint8_t i;
    
    // Loop on the BAM IDX (Instance 0 is BAM Manager)
    for (i = 0; i < BAM_IDX_MAX; i++)
    {
        // Check if the BAM task is Idle or not
        if (bam_state[i] == BAM_IDLE)
        {
            continue;
        }

        // Check STAID
        if (bam_env[i].sta_idx != sta_idx)
        {
            continue;
        }

        // Delete
        bam_send_mm_ba_del_req(sta_idx, i);
    }
}

void bam_start_inactivity_timer(uint16_t bam_idx)
{
    ke_timer_set(BAM_INACTIVITY_TIMEOUT_IND, KE_BUILD_ID(TASK_BAM, bam_idx),
                 bam_env[bam_idx].ba_timeout * TU_DURATION);
}

void bam_send_mm_ba_add_req(uint16_t sta_idx, uint16_t bam_idx)
{
    // Allocate the message
    struct mm_ba_add_req *req = KE_MSG_ALLOC(MM_BA_ADD_REQ,
                                             TASK_MM, KE_BUILD_ID(TASK_BAM, bam_idx),
                                             mm_ba_add_req);

    // Fill the message parameters
    req->type      = (bam_env[bam_idx].dev_type == BA_ORIGINATOR) ? 0 : 1;
    req->sta_idx   = sta_idx;
    req->tid       = bam_env[bam_idx].tid;
    req->bufsz     = bam_env[bam_idx].buffer_size;
    req->ssn       = bam_env[bam_idx].ssn;

    // Send the message to the task
    ke_msg_send(req);
}

void bam_send_mm_ba_del_req(uint16_t sta_idx, uint16_t bam_idx)
{
    // Allocate the message
    struct mm_ba_del_req *req = KE_MSG_ALLOC(MM_BA_DEL_REQ,
                                             TASK_MM, KE_BUILD_ID(TASK_BAM, bam_idx),
                                             mm_ba_del_req);

    // Fill the message parameters
    req->type      = (bam_env[bam_idx].dev_type == BA_ORIGINATOR) ? 0 : 1;
    req->sta_idx   = sta_idx;
    req->tid       = bam_env[bam_idx].tid;

    // Send the message
    ke_msg_send(req);
}
#endif

void bam_send_air_action_frame(uint8_t sta_idx,
                               struct bam_env_tag *bam_env,
                               uint8_t action,
                               uint8_t dialog_token,
                               uint16_t param,
                               uint16_t status_code)
{
    struct txl_frame_desc_tag *frame;
    struct mac_hdr *buf;
    struct tx_hd *thd;
    uint8_t ac = AC_VO;
    int txtype;
    // Get the VIF index
    uint8_t vif_idx = sta_mgmt_get_vif_idx(sta_idx);

    #if (NX_P2P)
    if (vif_mgmt_is_p2p(vif_idx))
    {
        txtype = TX_DEFAULT_5G;
    }
    else
    #endif //(NX_P2P)
    {
        struct phy_channel_info phy_info;
        uint8_t band;

        // Get the RF band on which we are to know which rate to use
        phy_get_channel(&phy_info, PHY_PRIM);
        band = phy_info.info1 & 0xFF;

        // Chose the right rate according to the band
        txtype = (band == PHY_BAND_2G4) ? TX_DEFAULT_24G : TX_DEFAULT_5G;
    }

    // Allocate a frame descriptor from the TX path
    frame = txl_frame_get(txtype, NX_TXFRAME_LEN);

    if (frame)
    {
        // Payload length
        uint32_t length = 0;
        struct vif_info_tag *vif = &vif_info_tab[vif_idx];

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif, frame);

        // Get the buffer pointer
        #if (NX_AMSDU_TX)
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer->payload;
        #endif //(NX_AMSDU_TX)

        // Fill-in the frame
        buf->fctl  = MAC_FCTRL_ACTION;
        // TODO[LT] - Set duration
        buf->durid = 0;
        // Set RA
        MAC_ADDR_CPY(&buf->addr1, sta_mgmt_get_peer_addr(sta_idx));
        // Set TA
        MAC_ADDR_CPY(&buf->addr2, vif_mgmt_get_addr(vif_idx));
        // Set ADDR3
        if (vif_mgmt_get_type(vif_idx) == VIF_AP)
        {
            MAC_ADDR_CPY(&buf->addr3, vif_mgmt_get_addr(vif_idx));
        }
        else
        {
            // STA or IBSS
            MAC_ADDR_CPY(&buf->addr3, sta_mgmt_get_peer_addr(sta_idx));
        }
        // Update the sequence number in the frame
        buf->seq = txl_get_seq_ctrl();

        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = vif_idx;
        frame->txdesc.host.staid   = sta_idx;

        length = MAC_SHORT_MAC_HDR_LEN;

        #if NX_MFP
        frame->txdesc.umac.head_len = 0;
        frame->txdesc.umac.tail_len = 0;
        if (MFP_UNICAST_PROT == mfp_protect_mgmt_frame(&frame->txdesc, buf->fctl,
                                                       MAC_BA_ACTION_CATEGORY))
        {
            txu_cntrl_protect_mgmt_frame(&frame->txdesc, (uint32_t)buf,
                                         MAC_SHORT_MAC_HDR_LEN);
            length += frame->txdesc.umac.head_len;
        }
        #endif

        // Build the payload
        switch (action)
        {
            case MAC_BA_ACTION_ADDBA_REQ:
            {
                ac = mac_tid2ac[BAM_BA_PARAM_GET(param, TID)];
                length += me_build_add_ba_req(CPU2HW(buf) + length, bam_env);
            } break;

            case MAC_BA_ACTION_ADDBA_RSP:
            {
                length += me_build_add_ba_rsp(CPU2HW(buf) + length, bam_env,
                                              param, dialog_token, status_code);
            } break;

            case MAC_BA_ACTION_DELBA:
            {
                length += me_build_del_ba(CPU2HW(buf) + length, bam_env,
                                          status_code);
            } break;

            default:
            {
                ASSERT_WARN(0);
            } break;
        }

        #if NX_MFP
        length += frame->txdesc.umac.tail_len;
        #endif
        thd             = &frame->txdesc.lmac.hw_desc->thd;
        thd->dataendptr = (uint32_t)thd->datastartptr + length - 1;
        thd->frmlen     = length + MAC_FCS_LEN;

        // Push the frame for TX
        txl_frame_push(frame, ac);
    }
}

#if NX_AMPDU_TX
void bam_check_ba_agg(struct txdesc *txdesc)
{
    // Get current time
    uint32_t current_time = hal_machw_time();
    uint8_t sta_idx = txdesc->host.staid;
    uint8_t tid = txdesc->host.tid;
    uint8_t bam_idx;

    do
    {
        struct sta_info_tag *sta = &sta_info_tab[sta_idx];

        // Check if peer device supports HT
        if (!(sta->info.capa_flags & STA_HT_CAPA) 
			|| (sta->ctrl_port_state != PORT_OPEN))
        {
            break;
        }

        // Get matching BAM index in order to know if a BA Agreement already exists
        bam_idx = sta_mgmt_get_tx_bam_idx(sta_idx, tid);

        if (bam_idx != BAM_INVALID_TASK_IDX)
        {
            // Update the last activity time
            bam_env[bam_idx].last_activity_time = current_time;

            // Check the current BAM task state in order to know if establishment is over
            if (ke_state_get(KE_BUILD_ID(TASK_BAM, bam_idx)) == BAM_ACTIVE)
            {
                // An agreement already exists, the packet can be part of an A-MPDU
                struct bam_baw *baw = &bam_env[bam_idx].baw;
                txdesc->umac.sn_win = baw->fsn;
                #if RC_ENABLE
                if (rc_check_aggregation(sta))
                #endif
                {
                    txdesc->umac.flags |= AMPDU_BIT;
                }
                txdesc->host.flags |= TXU_CNTRL_UNDER_BA;
                bam_set_baw_state(baw, txdesc->host.sn, BAW_PENDING);
                if (!(txdesc->host.flags & TXU_CNTRL_RETRY))
                    txdesc->host.timestamp = bam_time_get() + me_tx_lft_get();

                // Update the packet counter
                bam_env[bam_idx].pkt_cnt++;
            }
        }
        else
        {
            // Get the last TX time for the specified STAID/TID
            uint32_t last_tx_time = sta_mgmt_get_last_tx_time(sta_idx, tid);
            uint32_t last_ba_time = sta_mgmt_get_add_ba_time(sta_idx, tid);

            if (!hal_machw_time_cmp(current_time, last_ba_time + BAM_ADDBA_REQ_INTERVAL) &&
                hal_machw_time_cmp(current_time, last_tx_time + BAM_BA_AGG_DETECT_DURATION) &&
                (sta->ps_state != PS_MODE_ON))
            {
                // Try to get a free instance of BAM task
                bam_create_ba_agg(sta_idx, tid, txdesc->host.sn);
            }
        }
    } while (0);

    // Store current time
    sta_mgmt_set_last_tx_time(sta_idx, tid, current_time);
}

int8_t bam_tx_cfm(struct txdesc *txdesc, bool success)
{
    uint8_t sta_idx = txdesc->host.staid;
    uint8_t tid = txdesc->host.tid;
    uint8_t bam_idx;
    int8_t credits = 1;

    do
    {
        // Check if the frame was transmitted under BA agreement
        if (!(txdesc->host.flags & TXU_CNTRL_UNDER_BA))
            break;

        // Get matching BAM index in order to know if the BA Agreement still exists
        bam_idx = sta_mgmt_get_tx_bam_idx(sta_idx, tid);

        // Check if we have an agreement
        if (bam_idx == BAM_INVALID_TASK_IDX)
            break;

        // Check the current BAM task state in order to know if the agreement is active
        if (ke_state_get(KE_BUILD_ID(TASK_BAM, bam_idx)) != BAM_ACTIVE)
            break;

        // Update the packet counter
        bam_env[bam_idx].pkt_cnt--;

        // We have an active agreement, so we need to check the state of our transmission
        // window
        credits = bam_check_tx_baw(txdesc, bam_idx, success);
    } while (0);

//    // Update the credit count
//    credits += sta_mgmt_get_and_clear_credit_oft(sta_idx, tid);

    return credits;
}

void bam_baw_init(struct bam_env_tag *bam_env)
{
    struct bam_baw *baw = &bam_env->baw;

    // Reset the BAW states
    memset(&baw->states, BAW_FREE, BAM_MAX_TX_WIN_SIZE * sizeof(uint8_t));

    // Initializes the BAW
    baw->fsn = sta_mgmt_get_tx_ssn(bam_env->sta_idx, bam_env->tid);
    baw->fsn_idx = 0;
    baw->buf_size = bam_env->buffer_size;
}

int8_t bam_flush_baw(struct bam_baw *baw)
{
    int8_t credits = 0;
    int i;

    for (i = 0; i < baw->buf_size; i++) {
        if (baw->states[i] == BAW_CONFIRMED)
            credits++;
    }

    return(credits);
}

#endif

#if NX_REORD
void bam_rx_active(uint8_t sta_idx, uint8_t tid)
{
    // Retrieve BAM instance index
    uint8_t bam_idx = sta_mgmt_get_rx_bam_idx(sta_idx, tid);

    ASSERT_ERR(bam_idx < BAM_INVALID_TASK_IDX);

    bam_env[bam_idx].last_activity_time = hal_machw_time();
}
#endif

/// @} end of group
