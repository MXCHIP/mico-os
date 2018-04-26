/**
 ****************************************************************************************
 *
 * @file tdls.c
 *
 * @brief Tunneled direct-link setup (TDLS) module implementation.
 *
 * Copyright (C) RivieraWaves 2016
 *
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h" 
#include "mac_defs.h"
#include "mac_frame.h"
#include "mm.h"
#include "mm_timer.h"
#include "mm_bcn.h"
#include "co_endian.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "phy.h"
#include "rd.h"
#include "ps.h"
#include "txl_cntrl.h"
#include "txl_frame.h"
#include "rxl_cntrl.h"
#include "hal_machw.h"
#include "scan.h"
#include "chan.h"
#include "hal_dma.h"
#if (NX_P2P)
#include "p2p.h"
#endif //(NX_P2P)
#if (NX_TD)
#include "td.h"
#endif //(NX_TD)
#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#include "reg_mac_core.h"
#include "reg_mac_pl.h"

#include "tdls.h"

// For IE access functions
#include "mac_ie.h"


#if (TDLS_ENABLE)

/**
****************************************************************************************
* @brief
****************************************************************************************
*/

/**
 ****************************************************************************************
 * @brief Callback function indicating the completion of the TDLS Channel Switch Request
 * frame transmission.
 *
 * @param[in] env     Pointer to the VIF entry
 * @param[in] status  Status of the transmission
 ****************************************************************************************
 */
static void tdls_chsw_req_tx_cfm(void *env, uint32_t status)
{
    // If TX is successful set new state TDLS_CHSW_REQ_TX,
    // otherwise set state TDLS_BASE_CHANNEL
    if (status & FRAME_SUCCESSFUL_TX_BIT)
    {
        ke_state_set(TASK_TDLS, TDLS_CHSW_REQ_TX);
    }
    else
    {
        ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
    }
}

/**
 ****************************************************************************************
 * @brief Callback function indicating the completion of the TDLS Channel Switch Response
 * frame transmission.
 *
 * @param[in] env     Pointer to the VIF entry
 * @param[in] status  Status of the transmission
 ****************************************************************************************
 */
static void tdls_chsw_rsp_tx_cfm(void *env, uint32_t status)
{
    struct vif_info_tag *p_vif_entry = (struct vif_info_tag *)env;

     //Switch to the new channel just after ack received
    if (status & FRAME_SUCCESSFUL_TX_BIT)
    {
        struct mm_remain_on_channel_req param;

        //force_trigger("Off-channel\n");

        mm_timer_set(&p_vif_entry->u.sta.sta_tdls->chsw_time_timer,
                ke_time() + p_vif_entry->u.sta.sta_tdls->chsw_time);

        param.op_code = MM_ROC_OP_START;
        param.vif_index = p_vif_entry->index;
        param.band = p_vif_entry->u.sta.sta_tdls->chsw_band;
        param.type = p_vif_entry->u.sta.sta_tdls->chsw_type;
        param.prim20_freq = p_vif_entry->u.sta.sta_tdls->chsw_prim20_freq;
        param.center1_freq = p_vif_entry->u.sta.sta_tdls->chsw_center1_freq;
        param.center2_freq = p_vif_entry->u.sta.sta_tdls->chsw_center2_freq;
        param.tx_power = p_vif_entry->u.sta.sta_tdls->chsw_tx_power;
        param.duration_ms = (p_vif_entry->tbtt_timer.time - ke_time() -
                p_vif_entry->u.sta.sta_tdls->chsw_time - 10000)/1000;

        chan_roc_req(&param, TASK_TDLS);
        ke_state_set(TASK_TDLS, TDLS_OFF_CHANNEL);
    }
}

/**
 ****************************************************************************************
 * @brief Check if the received frame is a TDLS Channel Switch Request.
 *
 * @param[in] frame   Pointer to the received frame
 * @param[in] sta_tdls Pointer to the TDLS station informations
 * @param[in] vif_mac_addr MAC address of the VIF
 * @param[in] next_tbtt Time in us of the next TBTT
 ****************************************************************************************
 */
static uint16_t tdls_check_tdls_channel_switch_request(uint8_t *frame,
                                                       uint32_t offset,
                                                       struct sta_tdls_tag *sta_tdls,
                                                       struct mac_addr *vif_mac_addr,
                                                       uint32_t next_tbtt)
{
    uint16_t status = TDLS_CHANSW_REQUEST_ACCEPTED;
    struct mac_addr initiator;
    struct mac_addr responder;
    uint8_t i;
    uint32_t dt = tdls_get_dt_us(next_tbtt, ke_time());
    uint16_t ch_switch_time = TDLS_CHSW_SWITCH_TIME_US;
    uint16_t chsw_timeout;
    uint32_t frame_addr = CPU2HW(frame);

    do
    {
        // Get Target Channel Number
        sta_tdls->chsw_num = co_read8p(frame_addr + offset + TDLS_CHANSW_REQ_TARGET_CH_OFFSET);
        // Get Operating Class
        sta_tdls->chsw_op_class = co_read8p(frame_addr + offset + TDLS_CHANSW_REQ_OP_CLASS);
        offset += TDLS_CHANSW_REQ_IES_OFFSET;
        // Get Secondary Channel Offset (optional)
        if (co_read8p(frame_addr + offset) == MAC_ELTID_SEC_CH_OFFSET)
        {
            sta_tdls->chsw_band = PHY_BAND_2G4;
            sta_tdls->chsw_prim20_freq = phy_channel_to_freq(PHY_BAND_2G4, sta_tdls->chsw_num);
            sta_tdls->chsw_type = PHY_CHNL_BW_40;
            if (co_read8p(frame_addr + offset + MAC_INFOELT_SEC_CH_OFFSET_SEC_CH_OFT) == MAC_INFOELT_SEC_CH_OFFSET_SEC_ABOVE)
            {
                sta_tdls->chsw_center1_freq = sta_tdls->chsw_prim20_freq + 20;
            }
            else
            {
                sta_tdls->chsw_center1_freq = sta_tdls->chsw_prim20_freq - 20;
            }
            offset += TDLS_CHANSW_REQ_IE_SEC_CH_OFT_LEN;
        }

        // Get Link Identifier IE (mandatory)
        if (co_read8p(frame_addr + offset) != MAC_ELTID_LINK_IDENTIFIER)
        {
            status = TDLS_CHANSW_REQUEST_DECLINED;
            break;
        }
        // Check Link Identifier information element
        for (i = 0; i < MAC_ADDR_LEN/2; i++)
        {
            initiator.array[i] = co_read16p(frame_addr + offset + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i);
            responder.array[i] = co_read16p(frame_addr + offset + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i);
        }
        if (MAC_ADDR_CMP(&initiator.array[0], &vif_mac_addr->array[0]))
        {
            sta_tdls->initiator = false;
            MAC_ADDR_CPY(&sta_tdls->peer_mac_addr.array[0], &responder.array[0]);
        }
        else
        {
            sta_tdls->initiator = true;
            MAC_ADDR_CPY(&sta_tdls->peer_mac_addr.array[0], &initiator.array[0]);
        }
        offset += TDLS_CHANSW_REQ_IE_LINK_ID_LEN;

        // Get Channel Switch Timing IE (mandatory)
        if (co_read8p(frame_addr + offset) != MAC_ELTID_CHANNEL_SWITCH_TIMING)
        {
            status = TDLS_CHANSW_REQUEST_DECLINED;
            break;
        }
        sta_tdls->chsw_time = co_read16p(frame_addr + offset + MAC_INFOELT_CH_SWITCH_TIMING_SWTIME_OFT);
        sta_tdls->chsw_timeout = co_read16p(frame_addr + offset + MAC_INFOELT_CH_SWITCH_TIMING_SWTOUT_OFT);
        offset += TDLS_CHANSW_REQ_IE_CH_SWITCH_TIMING_LEN;
        // Update switch time and switch timeout
        if (sta_tdls->chsw_time < ch_switch_time)
        {
            sta_tdls->chsw_time = ch_switch_time;
        }
        // Check if we have time to complete the channel switch
        if (dt < (2*(uint32_t)sta_tdls->chsw_time + 2*TDLS_CHSW_TX_FRAME_TIME_US +
                sta_tdls->chsw_timeout))
        {
            status = TDLS_CHANSW_REQUEST_DECLINED;
            break;
        }
        chsw_timeout = (uint16_t)(dt - 2*(uint32_t)sta_tdls->chsw_time -
                2*TDLS_CHSW_TX_FRAME_TIME_US);
        if (chsw_timeout > TDLS_MAX_CHSW_SWITCH_TIME_US)
        {
            chsw_timeout = TDLS_MAX_CHSW_SWITCH_TIME_US;
        }
        if (sta_tdls->chsw_timeout < chsw_timeout)
        {
            sta_tdls->chsw_timeout = chsw_timeout;
        }

        // Get Wide Bandwidth Channel Switch IE (optional)
        if (co_read8p(frame_addr + offset) == MAC_ELTID_WIDE_BANDWIDTH_CHAN_SWITCH)
        {
            sta_tdls->chsw_band = PHY_BAND_5G;
            sta_tdls->chsw_type = 1 + co_read8p(frame_addr + offset + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CW_OFT);
            sta_tdls->chsw_prim20_freq = phy_channel_to_freq(PHY_BAND_5G, sta_tdls->chsw_num);
            sta_tdls->chsw_center1_freq = co_read8p(frame_addr + offset + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CENTER1_OFT);
            sta_tdls->chsw_center2_freq = co_read8p(frame_addr + offset + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CENTER2_OFT);
        }
        else
        {
            sta_tdls->chsw_band = PHY_BAND_2G4;
            sta_tdls->chsw_type = PHY_CHNL_BW_20;
            sta_tdls->chsw_prim20_freq = phy_channel_to_freq(PHY_BAND_2G4, sta_tdls->chsw_num);
            sta_tdls->chsw_center1_freq = sta_tdls->chsw_prim20_freq;
        }

    } while (0);

    return status;
}

/**
 ****************************************************************************************
 * @brief Check if the received frame is a TDLS Channel Switch Response.
 *
 * @param[in] frame   Pointer to the received frame
 * @param[in] sta_tdls Pointer to the TDLS station informations
 * @param[in] initiator_mac_addr MAC address of the TDLS initiator
 * @param[in] responder_mac_addr MAC address of the TDLS responder
 * @param[in] next_tbtt Time in us of the next TBTT
 *
 * @return CO_OK if the frame is correct, CO_FAIL otherwise
 ****************************************************************************************
 */
static uint32_t tdls_check_tdls_channel_switch_response(uint8_t *frame,
                                                        uint32_t offset,
                                                        struct sta_tdls_tag *sta_tdls,
                                                        struct mac_addr *initiator_mac_addr,
                                                        struct mac_addr *responder_mac_addr,
                                                        uint32_t next_tbtt)
{
    uint32_t res = CO_FAIL;
    uint8_t i;
    struct mac_addr initiator;
    struct mac_addr responder;
    uint32_t dt = tdls_get_dt_us(next_tbtt, ke_time());
    uint32_t frame_addr = CPU2HW(frame);

    do
    {
        // Get status
        if (co_read8p(frame_addr + offset + TDLS_CHANSW_RSP_STATUS_OFFSET) != 0)
        {
            res = CO_FAIL;
            break;
        }
        offset += TDLS_CHANSW_RSP_IES_OFFSET;

        // Get Link Identifier IE (mandatory)
        if (co_read8p(frame_addr + offset) != MAC_ELTID_LINK_IDENTIFIER)
        {
            res = CO_NOT_FOUND;
            break;
        }
        // Check Link Identifier IE
        for (i = 0; i < MAC_ADDR_LEN/2; i++)
        {
            initiator.array[i] = co_read16p(frame_addr + offset + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i);
            responder.array[i] = co_read16p(frame_addr + offset + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i);
        }
        if ((!MAC_ADDR_CMP(&initiator.array[0], &initiator_mac_addr->array[0])) ||
            (!MAC_ADDR_CMP(&responder.array[0], &responder_mac_addr->array[0])))
        {
            res = CO_BAD_PARAM;
            break;
        }
        offset += TDLS_CHANSW_REQ_IE_LINK_ID_LEN;

        // Get Channel Switch Timing IE (mandatory)
        if (co_read8p(frame_addr + offset) != MAC_ELTID_CHANNEL_SWITCH_TIMING)
        {
            res = CO_NOT_FOUND;
            break;
        }
        // Update time and timeout
        sta_tdls->chsw_time = co_read16p(frame_addr + offset + MAC_INFOELT_CH_SWITCH_TIMING_SWTIME_OFT);
        sta_tdls->chsw_timeout = co_read16p(frame_addr + offset + MAC_INFOELT_CH_SWITCH_TIMING_SWTOUT_OFT);
        // Check if we have time to complete the channel switch
        if (dt < (2*(uint32_t)sta_tdls->chsw_time + TDLS_CHSW_TX_FRAME_TIME_US +
                sta_tdls->chsw_timeout))
        {
            res = CO_BAD_PARAM;
            break;
        }
        offset += TDLS_CHANSW_REQ_IE_CH_SWITCH_TIMING_LEN;

        res = CO_OK;

    } while (0);

    return res;
}

/**
 ****************************************************************************************
 * @brief Check if the received frame is a TDLS Peer Traffic Response.
 *
 * @param[in] frame   Pointer to the received frame
 * @param[in] sta_tdls Pointer to the TDLS station informations
 * @param[in] sta_mac_addr MAC address of the TDLS peer
 *
 * @return CO_OK if the frame is correct, CO_FAIL otherwise
 ****************************************************************************************
 */
static uint32_t tdls_check_tdls_peer_traffic_response(uint8_t *frame,
                                                      uint32_t offset,
                                                      struct sta_tdls_tag *sta_tdls,
                                                      struct mac_addr *sta_mac_addr)
{
    uint32_t res = CO_FAIL;
    uint8_t i;
    struct mac_addr initiator;
    struct mac_addr responder;
    struct mac_addr *p_initiator_sta;
    struct mac_addr *p_responder_sta;
    uint32_t frame_addr = CPU2HW(frame);

    do
    {
        // Get Dialog Token
        if (co_read8p(frame_addr + offset + MAC_ACTION_TOKEN_OFT) != sta_tdls->dialog_token)
        {
            res = CO_FAIL;
            break;
        }
        offset += TDLS_CHANSW_RSP_IES_OFFSET;

        // Get Link Identifier IE (mandatory)
        if (co_read8p(frame_addr + offset) != MAC_ELTID_LINK_IDENTIFIER)
        {
            res = CO_NOT_FOUND;
            break;
        }
        // Check Link Identifier IE
        for (i = 0; i < MAC_ADDR_LEN/2; i++)
        {
            initiator.array[i] = co_read16p(frame_addr + offset + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i);
            responder.array[i] = co_read16p(frame_addr + offset + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i);
        }
        if (sta_tdls->initiator)
        {
            p_initiator_sta = &sta_tdls->peer_mac_addr;
            p_responder_sta = sta_mac_addr;
        }
        else
        {
            p_initiator_sta = sta_mac_addr;
            p_responder_sta = &sta_tdls->peer_mac_addr;
        }
        if ((!MAC_ADDR_CMP(&initiator.array[0], &p_initiator_sta->array[0])) ||
            (!MAC_ADDR_CMP(&responder.array[0], &p_responder_sta->array[0])))
        {
            res = CO_BAD_PARAM;
            break;
        }

        res = CO_OK;

    } while (0);

    return res;
}

/**
 ****************************************************************************************
 * @brief Callback called upon TDLS channel switch time timer expiration.
 * This timer is started as soon as TDLS Channel Switch Response is transmitted.
 * This function checks if the switch to the off-channel has been completed, starts the
 * timeout timer and sends the TDLS_CHAN_SWITCH_IND message.
 * In case the switch to the off channel is not completed, it cancels the switch to the
 * off-channel.
 *
 * @param[in] env   Pointer the VIF entry
 ****************************************************************************************
 */
static void tdls_chsw_time_evt(void *env)
{
    struct vif_info_tag *vif_entry = (struct vif_info_tag *)env;

    if ((chan_is_on_channel(vif_entry)) && (ke_state_get(TASK_TDLS) == TDLS_OFF_CHANNEL))
    {
        // Start Channel Switch Timeout timer
        mm_timer_set(&vif_entry->u.sta.sta_tdls->chsw_timeout_timer,
                ke_time() + vif_entry->u.sta.sta_tdls->chsw_timeout);

        // Send Channel Switch Indication
        struct tdls_chan_switch_ind *ind = KE_MSG_ALLOC(TDLS_CHAN_SWITCH_IND,
                TASK_API, TASK_TDLS,
                tdls_chan_switch_ind);
        ind->vif_index = vif_entry->index;
        ind->chan_ctxt_index = vif_entry->chan_ctxt->idx;
        ind->status = CO_OK;

        ke_msg_send(ind);

        ke_state_set(TASK_TDLS, TDLS_OFF_CHANNEL);
    }
    else
    {
        struct mm_remain_on_channel_req param;

        param.vif_index = vif_entry->index;
        param.op_code = MM_ROC_OP_CANCEL;

        chan_roc_req(&param, TASK_TDLS);

        ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
    }
}

/**
 ****************************************************************************************
 * @brief Callback called upon TDLS channel switch timeout timer expiration.
 * This timer is started when channel switch time timer is expired and the switch to the
 * off channel has been done.
 * This function is called only if no successful frame exchange is performed in the timeout
 * timer time: in this case the switch to off channel is cancelled.
 *
 * @param[in] env   Pointer the VIF entry
 ****************************************************************************************
 */
static void tdls_chsw_timeout_evt(void *env)
{
    struct vif_info_tag *vif_entry = (struct vif_info_tag *)env;
    struct mm_remain_on_channel_req param;

    // If no successfull frame exchange within Switch Timeout, switch back to base channel
    param.vif_index = vif_entry->index;
    param.op_code = MM_ROC_OP_CANCEL;

    chan_roc_req(&param, TASK_TDLS);

    ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
}

/**
 ****************************************************************************************
 * @brief Callback called upon TDLS channel switch timeout timer expiration.
 * This timer is started when channel switch time timer is expired and the switch to the
 * off channel has been done.
 * This function is called only if no successful frame exchange is performed in the timeout
 * timer time: in this case the switch to off channel is cancelled.
 *
 * @param[in] env   Pointer the VIF entry
 *
 * @return
 ****************************************************************************************
 */
static bool tdls_check_frame_action(uint8_t *frame, struct vif_info_tag *vif_entry)
{
    bool upload = true;
    uint16_t framectrl = co_read16(frame);
    uint32_t offset;
    uint8_t tdls_action;
    uint32_t frame_addr = CPU2HW(frame);

    do
    {
        if ((framectrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_DATA_T)
        {
            if ((ke_state_get(TASK_TDLS) == TDLS_OFF_CHANNEL) &&
                (!hal_machw_time_past(vif_entry->u.sta.sta_tdls->chsw_timeout_timer.time)))
            {
                mm_timer_clear(&vif_entry->u.sta.sta_tdls->chsw_timeout_timer);
            }

            if (!(framectrl & MAC_QOS_ST_BIT))
            {
                break;
            }
        }

        offset = MAC_SHORT_QOS_MAC_HDR_LEN + ((framectrl & MAC_FCTRL_ORDER) ? MAC_HTCTRL_LEN : 0);

        if (!((co_read32p(frame_addr + offset) == FRAME_BODY_LLC_H) &&
              (co_read32p(frame_addr + offset + MAC_ENCAPSULATED_LLC_L_OFT) == FRAME_BODY_LLC_L) &&
              (co_read8p(frame_addr + offset + MAC_ENCAPSULATED_PAYLOAD_TYPE_OFT) == PAYLOAD_TYPE_TDLS)))
        {
            break;
        }

        offset += MAC_ENCAPSULATED_PAYLOAD_OFT;
        if (co_read8p(frame_addr + offset) != MAC_TDLS_ACTION_CATEGORY)
        {
            break;
        }

        tdls_action = co_read8p(frame_addr + offset + MAC_ACTION_ACTION_OFT);
        if (tdls_action == MAC_TDLS_ACTION_PEER_TRAFFIC_RSP)
        {
            /////////////////////////////////////
            // Peer Traffic Response received  //
            /////////////////////////////////////

            uint16_t res;
            upload = false;

            // Check if the TDLS peer is active
            if (!vif_entry->u.sta.sta_tdls->active)
            {
                break;
            }

            // Check if TDLS Peer Traffic Indication frame has been sent to AP
            if (ke_state_get(TASK_TDLS) != TDLS_TRAFFIC_IND_TX)
            {
                break;
            }

            res = tdls_check_tdls_peer_traffic_response(frame, offset,
                                                        vif_entry->u.sta.sta_tdls,
                                                        &vif_entry->mac_addr);

            if (res == CO_OK)
            {
                // Inform the host that the TDLS peer is awake
                tdls_send_peer_ps_ind(vif_entry, false);
                // Set state
                ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
            }
        }
        else if (tdls_action == MAC_TDLS_ACTION_CHANSW_REQ)
        {
            /////////////////////////////////////
            // Channel Switch Request received //
            /////////////////////////////////////
            uint16_t status;
            upload = false;

            // Check if the TDLS peer is active
            if (!vif_entry->u.sta.sta_tdls->active)
            {
                break;
            }
            // Init timers
            vif_entry->u.sta.sta_tdls->chsw_time_timer.cb = tdls_chsw_time_evt;
            vif_entry->u.sta.sta_tdls->chsw_time_timer.env = vif_entry;
            vif_entry->u.sta.sta_tdls->chsw_timeout_timer.cb = tdls_chsw_timeout_evt;
            vif_entry->u.sta.sta_tdls->chsw_timeout_timer.env = vif_entry;
            vif_entry->u.sta.sta_tdls->chsw_tx_power = vif_entry->tx_power;

            status = tdls_check_tdls_channel_switch_request(frame, offset,
                                                            vif_entry->u.sta.sta_tdls,
                                                            &vif_entry->mac_addr,
                                                            vif_entry->tbtt_timer.time);
            // If Channel Switch Request is received while waiting for a Channel Switch Response,
            // the STA should reply only if it's the responder STA
            if ((ke_state_get(TASK_TDLS) == TDLS_CHSW_REQ_TX) && (!vif_entry->u.sta.sta_tdls->initiator))
            {
                break;
            }

            if (status == TDLS_CHANSW_REQUEST_ACCEPTED)
            {
                // check if STA is waiting for frames from AP
                if (vif_entry->prevent_sleep &
                       (PS_VIF_WAITING_BCMC | PS_VIF_WAITING_EOSP | PS_VIF_WAITING_UC))
                {
                    status = TDLS_CHANSW_REQUEST_DECLINED;
                }
                else
                {
                    // Set TDLS state
                    ke_state_set(TASK_TDLS, TDLS_CHSW_REQ_RX);
                }
            }

            // Send TDLS Channel Switch Response
            txl_frame_send_tdls_channel_switch_rsp_frame(vif_entry, status, &tdls_chsw_rsp_tx_cfm);

        }
        else if (tdls_action == MAC_TDLS_ACTION_CHANSW_RSP)
        {
            //////////////////////////////////////
            // Channel Switch Response received //
            //////////////////////////////////////
            uint32_t res;
            struct mac_addr *initiator_mac_addr;
            struct mac_addr *responder_mac_addr;
            upload = false;

            // Check if the TDLS peer is active
            if (!vif_entry->u.sta.sta_tdls->active)
            {
                break;
            }

            if (vif_entry->u.sta.sta_tdls->initiator)
            {
                initiator_mac_addr = &vif_entry->u.sta.sta_tdls->peer_mac_addr;
                responder_mac_addr = &vif_entry->mac_addr;
            }
            else
            {
                initiator_mac_addr = &vif_entry->mac_addr;
                responder_mac_addr = &vif_entry->u.sta.sta_tdls->peer_mac_addr;
            }

            res = tdls_check_tdls_channel_switch_response(frame, offset,
                                                          vif_entry->u.sta.sta_tdls,
                                                          initiator_mac_addr, responder_mac_addr,
                                                          vif_entry->tbtt_timer.time);
            // Switch to the new channel just after Channel Switch Response frame has been received
            if (res != CO_OK)
            {
                ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
                break;
            }
            // do not switch to the new channel if already on the requested channel
            if ((vif_entry->chan_ctxt->channel.band != vif_entry->u.sta.sta_tdls->chsw_band) ||
                (vif_entry->chan_ctxt->channel.prim20_freq != vif_entry->u.sta.sta_tdls->chsw_prim20_freq) ||
                (vif_entry->chan_ctxt->channel.type != vif_entry->u.sta.sta_tdls->chsw_type))
            {
                struct mm_remain_on_channel_req param;

                param.vif_index = vif_entry->index;
                param.band = vif_entry->u.sta.sta_tdls->chsw_band;
                param.type = vif_entry->u.sta.sta_tdls->chsw_type;
                param.prim20_freq = vif_entry->u.sta.sta_tdls->chsw_prim20_freq;
                param.center1_freq = vif_entry->u.sta.sta_tdls->chsw_center1_freq;
                param.center2_freq = vif_entry->u.sta.sta_tdls->chsw_center2_freq;
                param.tx_power = vif_entry->u.sta.sta_tdls->chsw_tx_power;
                param.duration_ms = (tdls_get_dt_us(vif_entry->tbtt_timer.time, ke_time()) -
                                     2*(uint32_t)vif_entry->u.sta.sta_tdls->chsw_time -
                                     TDLS_CHSW_TX_FRAME_TIME_US) / 1000;

                if (ke_state_get(TASK_TDLS) == TDLS_CHSW_REQ_TX)
                {
                    //force_trigger("Off-channel\n");
                    param.op_code = MM_ROC_OP_START;

                    chan_roc_req(&param, TASK_TDLS);

                    ke_state_set(TASK_TDLS, TDLS_OFF_CHANNEL);
                }
                // unsolicited Channel Switch Response frame
                else if (ke_state_get(TASK_TDLS) == TDLS_OFF_CHANNEL)
                {
                    mm_timer_clear(&vif_entry->u.sta.sta_tdls->chsw_req_timer);
                    param.op_code = MM_ROC_OP_CANCEL;
                    chan_roc_req(&param, TASK_TDLS);

                    ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
                }
            }
        }
    } while(0);

    return upload;
}

/**
 ****************************************************************************************
 * @brief This function checks if the received frame is a NULL function indicating that
 * the TDLS peer is going to sleep and informs the host.
 *
 * @param[in] frame     Pointer to the frame
 * @param[in] vif_entry Pointer to the VIF entry
 ****************************************************************************************
 */
static void tdls_check_peer_ps(uint8_t *frame, struct vif_info_tag *vif_entry)
{
    struct mac_hdr *machdr = (struct mac_hdr *)frame;
    uint16_t framectrl = machdr->fctl;

    do
    {
        // Check if the TDLS peer is active
        if (!vif_entry->u.sta.sta_tdls->active)
        {
            break;
        }

        // Check frame control NULL function
        if ((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) != MAC_FCTRL_NULL_FUNCTION)
        {
            break;
        }
        // Check From DS and To DS flags
        if (framectrl & MAC_FCTRL_TODS_FROMDS)
        {
            break;
        }

        // Check Power Management flag
        if (!(framectrl & MAC_FCTRL_PWRMGT))
        {
            break;
        }

        // Inform the host that the TDLS peer is going to sleep
        tdls_send_peer_ps_ind(vif_entry, true);


    } while (0);
}

void tdls_chsw_req_evt(void *env)
{
    struct vif_info_tag *vif_entry = (struct vif_info_tag *)env;
    struct tdls_chan_switch_req req;
    uint32_t next_evt;

    // Check if there is traffic with the AP
    if ((!vif_entry->u.sta.sta_tdls->chsw_delay) &&
        (vif_entry->prevent_sleep &
         (PS_VIF_WAITING_BCMC | PS_VIF_WAITING_EOSP | PS_VIF_WAITING_UC)))
    {
        // Delay channel switch request
        next_evt = hal_machw_time() + TDLS_CHSW_REQ_DELAY_AP_TRAFFIC_US;//(p_sta_entry->bcn_int - TDLS_CHSW_REQ_DELAY_US) / 2;
        vif_entry->u.sta.sta_tdls->chsw_delay = true;
    }
    else
    {
        next_evt = vif_entry->tbtt_timer.time + TDLS_CHSW_REQ_DELAY_US;
        // Check if we are on the base channel
        if (ke_state_get(TASK_TDLS) == TDLS_BASE_CHANNEL)
        {
            req.vif_index = vif_entry->index;
            MAC_ADDR_CPY(&req.peer_mac_addr, &vif_entry->u.sta.sta_tdls->peer_mac_addr);
            req.initiator = vif_entry->u.sta.sta_tdls->initiator;
            req.band = vif_entry->u.sta.sta_tdls->chsw_band;
            req.type = vif_entry->u.sta.sta_tdls->chsw_type;
            req.prim20_freq = vif_entry->u.sta.sta_tdls->chsw_prim20_freq;
            req.center1_freq = vif_entry->u.sta.sta_tdls->chsw_center1_freq;
            req.center2_freq = vif_entry->u.sta.sta_tdls->chsw_center2_freq;
            req.tx_power = vif_entry->u.sta.sta_tdls->chsw_tx_power;
            req.op_class = vif_entry->u.sta.sta_tdls->chsw_op_class;

            // send TDLS Channel Switch Request frame
            txl_frame_send_tdls_channel_switch_req_frame(&req, &tdls_chsw_req_tx_cfm);
        }
        // Check if we are waiting to complete TDLS channel switch procedure
        // (i.e. TDLS ChSw Response not yet received)
        else if (ke_state_get(TASK_TDLS) == TDLS_CHSW_REQ_TX)
        {
            ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
        }

        vif_entry->u.sta.sta_tdls->chsw_delay = false;
    }

    // Set next channel switch request
    mm_timer_set(&vif_entry->u.sta.sta_tdls->chsw_req_timer, next_evt);

}

bool tdls_check_frame(uint8_t *frame, struct vif_info_tag *vif_entry)
{
    bool upload;

    // Check if the TDLS peer is going to sleep
    tdls_check_peer_ps(frame, vif_entry);

    // Check if TDLS Channel Switch Request, TDLS Channel Switch Response,
    // TDLS Peer Traffic Response and update TDLS info
    upload = tdls_check_frame_action(frame, vif_entry);

    return upload;
}

void
tdls_send_chan_switch_base_ind(struct chan_ctxt_tag *roc_chan_ctxt)
{
    // Inform the host that the remain on channel has expired
    struct tdls_chan_switch_base_ind *ind = KE_MSG_ALLOC(TDLS_CHAN_SWITCH_BASE_IND,
                                                         TASK_API, roc_chan_ctxt->taskid,
                                                         tdls_chan_switch_base_ind);
    ind->chan_ctxt_index = roc_chan_ctxt->idx;
    ind->vif_index       = roc_chan_ctxt->vif_index;

    ke_msg_send(ind);

    ke_state_set(TASK_TDLS, TDLS_BASE_CHANNEL);
}

void
tdls_send_peer_ps_ind(struct vif_info_tag *vif_entry, bool ps_on)
{

    // Inform the host that the TDLS peer is going to sleep
    struct tdls_peer_ps_ind *ind = KE_MSG_ALLOC(TDLS_PEER_PS_IND,
                                                TASK_API, TASK_TDLS,
                                                tdls_peer_ps_ind);

    ind->vif_index = vif_entry->index;
    MAC_ADDR_CPY(ind->peer_mac_addr.array, &vif_entry->u.sta.sta_tdls->peer_mac_addr);
    ind->ps_on = ps_on;

    ke_msg_send(ind);

    // set TDLS STA status
    ke_state_set(TASK_TDLS, TDLS_PEER_SLEEPING);
}


#endif // TDLS_ENABLE
