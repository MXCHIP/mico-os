/**
 ****************************************************************************************
 *
 * @file txl_frame.c
 *
 * @brief Management of internal frame generation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/*
 ****************************************************************************************
 * @addtogroup TX_FRM
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h"
#include "rwnx_config.h"
 
#include <stddef.h>
#include "txl_cntrl.h"
#include "txl_frame.h"
#include "ke_event.h"
#include "mac_frame.h"
#include "txl_buffer.h"
#include "co_utils.h"
#include "reg_mac_core.h"
#include "vif_mgmt.h"
#include "phy.h"
#include "tpc.h"
#include "tdls.h"

#include "include.h"
#include "arm_arch.h"
#include "uart_pub.h"

#if CFG_TX_EVM_TEST
#include "tx_evm.h"
#endif

#if NX_TX_FRAME
/*
 * STRUCTURE DEFINITION
 ****************************************************************************************
 */
/*
 * GLOBAL VARIABLE DEFINITION
 ****************************************************************************************
 */
/// Pool of TX frame descriptors
static struct txl_frame_desc_tag txl_frame_desc[NX_TXFRAME_CNT];

/// TX frame environment variables
struct txl_frame_env_tag txl_frame_env;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void txl_frame_init_desc(struct txl_frame_desc_tag *frame,
                         struct txl_buffer_tag *buffer,
                         struct tx_hw_desc_s *hwdesc,
                         struct txl_buffer_control *bufctrl)
{
    struct tx_hd *thd = &hwdesc->thd;

    // Reset the frame descriptors
    memset(frame, 0, sizeof(*frame));

    // Initialize the frame buffer
    thd->upatterntx = TX_HEADER_DESC_PATTERN;
    thd->datastartptr = CPU2HW(buffer->payload);
    thd->frmlifetime = 0;
    thd->optlen[0] = 0;
    thd->optlen[1] = 0;
    thd->optlen[2] = 0;
    bufctrl->policy_tbl.upatterntx = POLICY_TABLE_PATTERN;

    // Initialize the descriptor
    #if NX_AMSDU_TX
    frame->txdesc.lmac.buffer[0] = buffer;
    #else
    frame->txdesc.lmac.buffer = buffer;
    #endif
    
    frame->txdesc.umac.buf_control = bufctrl;
    
    frame->txdesc.lmac.hw_desc = hwdesc;
    frame->type = TX_EXT;
}

void txl_frame_init(bool reset)
{
    int i;
    struct tx_policy_tbl *p_pol_tbl;

    // Initialize the lists
    co_list_init(&txl_frame_env.desc_free);
    co_list_init(&txl_frame_env.desc_done);

    // Populate the free list and initialize the TX frame descriptors
    for (i = 0; i < NX_TXFRAME_CNT; i ++)
    {
        struct txl_frame_desc_tag *frame = &txl_frame_desc[i];
        struct txl_buffer_tag *buffer = (struct txl_buffer_tag *)&txl_frame_pool[i];
        struct tx_hw_desc_s *hwdesc = &txl_frame_hwdesc_pool[i];
        struct tx_hd *thd = &hwdesc->thd;
        
        struct txl_buffer_control *bufctrl = &txl_frame_buf_ctrl[i];

        // Reset the frame descriptor only during init or when frame has not been postponed
        if (!reset || (reset && !frame->postponed))
        {
            // Reset the frame descriptor
            memset(frame, 0, sizeof(struct txl_frame_desc_tag));

            // Initialize the frame buffer
            thd->upatterntx = TX_HEADER_DESC_PATTERN;
            thd->datastartptr = CPU2HW(buffer->payload);
            thd->frmlifetime = 0;
            thd->optlen[0] = 0;
            thd->optlen[1] = 0;
            thd->optlen[2] = 0;
            bufctrl->policy_tbl.upatterntx = POLICY_TABLE_PATTERN;

            // Initialize the descriptor
            #if NX_AMSDU_TX
            frame->txdesc.lmac.buffer[0] = buffer;
            #else
            frame->txdesc.lmac.buffer = buffer;
            #endif

            frame->txdesc.umac.buf_control = bufctrl;
            frame->txdesc.lmac.hw_desc = hwdesc;
            frame->type = TX_INT;

            // Push it in the free list
            co_list_push_back(&txl_frame_env.desc_free, &frame->txdesc.list_hdr);
        }
    }

    // Initialize default policy tables (2.4 GHz)
    p_pol_tbl = &txl_buffer_control_24G.policy_tbl;
    txl_buffer_control_24G.mac_control_info = 0;
    txl_buffer_control_24G.phy_control_info = 0;

    p_pol_tbl->upatterntx        = POLICY_TABLE_PATTERN;
    p_pol_tbl->phycntrlinfo1     = phy_get_ntx() << NX_TX_PT_OFT;
    p_pol_tbl->phycntrlinfo2     = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
    p_pol_tbl->maccntrlinfo1     = 0;
    p_pol_tbl->maccntrlinfo2     = 0xFFFF0704;
    p_pol_tbl->ratecntrlinfo[0]  = (HW_RATE_1MBPS << MCS_INDEX_TX_RCX_OFT) | PRE_TYPE_TX_RCX_MASK;
    p_pol_tbl->ratecntrlinfo[1]  = 0;
    p_pol_tbl->ratecntrlinfo[2]  = 0;
    p_pol_tbl->ratecntrlinfo[3]  = 0;
    p_pol_tbl->powercntrlinfo[1] = 0;
    p_pol_tbl->powercntrlinfo[2] = 0;
    p_pol_tbl->powercntrlinfo[3] = 0;

    // Initialize default policy tables (5 GHz)
    p_pol_tbl = &txl_buffer_control_5G.policy_tbl;
    txl_buffer_control_5G.mac_control_info = 0;
    txl_buffer_control_5G.phy_control_info = 0;

    p_pol_tbl->upatterntx         = POLICY_TABLE_PATTERN;
    p_pol_tbl->phycntrlinfo1      = phy_get_ntx() << NX_TX_PT_OFT;
    p_pol_tbl->phycntrlinfo2      = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
    p_pol_tbl->maccntrlinfo1      = 0;
    p_pol_tbl->maccntrlinfo2      = 0xFFFF0704;
    p_pol_tbl->ratecntrlinfo[0]   = HW_RATE_6MBPS << MCS_INDEX_TX_RCX_OFT;
    p_pol_tbl->ratecntrlinfo[1]   = 0;
    p_pol_tbl->ratecntrlinfo[2]   = 0;
    p_pol_tbl->ratecntrlinfo[3]   = 0;
    p_pol_tbl->powercntrlinfo[1]  = 0;
    p_pol_tbl->powercntrlinfo[2]  = 0;
    p_pol_tbl->powercntrlinfo[3]  = 0;

    #if (RW_BFMER_EN)
    p_pol_tbl = &txl_buffer_control_ndpa_brp.policy_tbl;
    txl_buffer_control_ndpa_brp.mac_control_info = 0;
    txl_buffer_control_ndpa_brp.phy_control_info = 0;

    p_pol_tbl->upatterntx         = POLICY_TABLE_PATTERN;
    p_pol_tbl->phycntrlinfo1      = phy_get_ntx() << NX_TX_PT_OFT;
    p_pol_tbl->phycntrlinfo2      = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
    p_pol_tbl->maccntrlinfo1      = 0;
    p_pol_tbl->maccntrlinfo2      = 0xFFFF0000;
    p_pol_tbl->ratecntrlinfo[0]   = BW_80MHZ << BW_TX_RCX_OFT;
    p_pol_tbl->ratecntrlinfo[0]   |= FORMATMOD_VHT << FORMAT_MOD_TX_RCX_OFT;
    p_pol_tbl->ratecntrlinfo[0]   |= (0x0 << 4) << MCS_INDEX_TX_RCX_OFT;
    p_pol_tbl->ratecntrlinfo[1]   = 0;
    p_pol_tbl->ratecntrlinfo[2]   = 0;
    p_pol_tbl->ratecntrlinfo[3]   = 0;
    p_pol_tbl->powercntrlinfo[1]  = 0;
    p_pol_tbl->powercntrlinfo[2]  = 0;
    p_pol_tbl->powercntrlinfo[3]  = 0;

    p_pol_tbl = &txl_buffer_control_ndp.policy_tbl;
    txl_buffer_control_ndp.mac_control_info = 0;
    txl_buffer_control_ndp.phy_control_info = 0;

    p_pol_tbl->upatterntx         = POLICY_TABLE_PATTERN;
    p_pol_tbl->phycntrlinfo1      = phy_get_ntx() << NX_TX_PT_OFT;
    p_pol_tbl->phycntrlinfo2      = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
    p_pol_tbl->maccntrlinfo1      = 0;
    p_pol_tbl->maccntrlinfo2      = 0xFFFF0000;
    p_pol_tbl->ratecntrlinfo[0]   = 0;
    p_pol_tbl->ratecntrlinfo[0]   |= FORMATMOD_VHT << FORMAT_MOD_TX_RCX_OFT;
    p_pol_tbl->ratecntrlinfo[0]   |= (phy_get_nss() << 4) << MCS_INDEX_TX_RCX_OFT;
    p_pol_tbl->ratecntrlinfo[1]   = 0;
    p_pol_tbl->ratecntrlinfo[2]   = 0;
    p_pol_tbl->ratecntrlinfo[3]   = 0;
    p_pol_tbl->powercntrlinfo[1]  = 0;
    p_pol_tbl->powercntrlinfo[2]  = 0;
    p_pol_tbl->powercntrlinfo[3]  = 0;
    #endif //(RW_BFMER_EN)
}

#if CFG_TX_EVM_TEST
void txl_evm_set_hw_rate(HW_RATE_E rate, uint32_t is_2_4G)
{
	if(is_2_4G)
	{
		txl_frame_pol_24G.ratecntrlinfo[0] = (rate << MCS_INDEX_TX_RCX_OFT) | PRE_TYPE_TX_RCX_MASK; 
	}
	else
	{
		txl_frame_pol_5G.ratecntrlinfo[0] = rate << MCS_INDEX_TX_RCX_OFT;
	}
}
#endif

struct txl_frame_desc_tag *txl_frame_get(int type, int len)
{
    // Allocate a new frame
    struct txl_frame_desc_tag *frame;

    // Allocate a new frame
    frame = (struct txl_frame_desc_tag *)co_list_pop_front(&txl_frame_env.desc_free);

    // Check if allocation was successful
    if (frame != NULL)
    {
        struct txl_buffer_control *bufctrl = txl_buffer_control_get(&frame->txdesc);
        struct tx_hd *thd = &frame->txdesc.lmac.hw_desc->thd;
        struct tx_policy_tbl *pol;

        // Fill-up the TX header descriptor
        #if (RW_BFMER_EN)
        thd->frmlen = len;

        if (len == 0)
        {
            thd->datastartptr = 0;
            thd->dataendptr = 0;
        }
        else
        {
            // Get buffer
            struct txl_buffer_tag *buffer = txl_buffer_get(&frame->txdesc);

            thd->frmlen += MAC_FCS_LEN;
            thd->datastartptr = CPU2HW(buffer->payload);
            thd->dataendptr = (uint32_t)thd->datastartptr + len - 1;
        }
        #else
        thd->dataendptr = (uint32_t)thd->datastartptr + len - 1;
        thd->frmlen = len + MAC_FCS_LEN;
        #endif //(RW_BFMER_EN)

        switch (type)
        {
            case TX_DEFAULT_24G:
                pol = &txl_buffer_control_24G.policy_tbl;	
                break;
                
            case TX_DEFAULT_5G:
                pol = &txl_buffer_control_5G.policy_tbl;
                break;
                
            #if (RW_BFMER_EN)
            case TX_DEFAULT_NDPA_BRP:
                pol = &txl_buffer_control_ndpa_brp.policy_tbl;
                break;
                
            case TX_DEFAULT_NDP:
                pol = &txl_buffer_control_ndp.policy_tbl;
                break;
            #endif //(RW_BFMER_EN)
            
            default:
                pol = &bufctrl->policy_tbl;
                break;
        }

        #if NX_MFP
        os_memcpy(&bufctrl->policy_tbl, pol, sizeof(struct tx_policy_tbl));
        pol = &bufctrl->policy_tbl;
        #endif

        // Set TX power
        pol->powercntrlinfo[0] = nxmac_ofdm_max_pwr_level_getf() << TX_PWR_LEVEL_PT_RCX_OFT;
        thd->policyentryaddr = CPU2HW(pol);
        thd->phyctrlinfo = 0;
        thd->macctrlinfo2 = 0;
        thd->first_pbd_ptr = 0;

        // Reset the confirmation structure
        frame->cfm.cfm_func = NULL;
        frame->cfm.env = NULL;
    }

    return frame;
}

bool txl_frame_push(struct txl_frame_desc_tag *frame, uint8_t ac)
{
    struct tx_hd *thd = &frame->txdesc.lmac.hw_desc->thd;
    struct mac_hdr *hdr;

    // Sanity check - the pushed frame cannot be NULL
    ASSERT_ERR(frame != NULL);
    ASSERT_ERR((thd->datastartptr & 0x01) == 0);

    // Perform last settings
    hdr = HW2CPU(thd->datastartptr);
    thd->nextfrmexseq_ptr = 0;
    thd->nextmpdudesc_ptr = 0;
    thd->macctrlinfo2 &= ~(WHICHDESC_MSK | UNDER_BA_SETUP_BIT);
    
    if (MAC_ADDR_GROUP(&hdr->addr1))
        thd->macctrlinfo1 = EXPECTED_ACK_NO_ACK;
    else
        thd->macctrlinfo1 = EXPECTED_ACK_NORMAL_ACK;

    thd->statinfo = 0;

    // Push the descriptor to the TXL controller for transmission
    return (txl_cntrl_push_int((struct txdesc *)frame, ac));
}

void txl_frame_cfm(struct txdesc *txdesc)
{
    // Push the descriptor into the confirmation queue
    co_list_push_back(&txl_frame_env.desc_done, &txdesc->list_hdr);

    // Trigger the frame confirmation event
    ke_evt_set(KE_EVT_TXFRAME_CFM_BIT);
}

void txl_frame_release(struct txdesc *txdesc, bool postponed)
{
    struct txl_frame_desc_tag *p_frame = (struct txl_frame_desc_tag *)txdesc;

    // If the descriptor was not statically allocated, push it back in the list of free descriptors
    if (p_frame->type == TX_INT)
    {
        // Push the descriptor into the confirmation queue
        co_list_push_back(&txl_frame_env.desc_free, &txdesc->list_hdr);
    }

    // If frame is released after being postponed, call the associated callback
    if (postponed)
    {
        // Call the callback if any
        if (p_frame->cfm.cfm_func != NULL)
        {
            // Status bits show that the frame has not been sent
            p_frame->cfm.cfm_func(p_frame->cfm.env, 0);
        }
    }
}

void txl_frame_evt(int dummy)
{
    struct txl_frame_desc_tag *frame;
    
    GLOBAL_INT_DECLARATION();

    // Trigger the frame confirmation event
    ke_evt_clear(KE_EVT_TXFRAME_CFM_BIT);

    // Go through the list of done descriptors
    while (1)
    {
        // Pop a descriptor from the list
        GLOBAL_INT_DISABLE();
        
        frame = (struct txl_frame_desc_tag *)co_list_pop_front(&txl_frame_env.desc_done);
        GLOBAL_INT_RESTORE();

        // Check if we reached the end of the list
        if (frame == NULL)
            break;

        
        #if NX_POWERSAVE
        // Decrease the number of packets in the TX path
        txl_cntrl_env.pck_cnt--;
        #endif

        // Call the confirmation callback if any
        if (frame->cfm.cfm_func != NULL)
        {
            frame->cfm.cfm_func(frame->cfm.env, frame->txdesc.lmac.hw_desc->thd.statinfo);

            /*
             * Check if descriptor can be pushed back in list of free descriptor. If not it means that the
             * packet has been pushed back in the TX path for retransmission
             */
            if (frame->keep_desc)
            {
                // Reset the status
                frame->keep_desc = false;
                continue;
            }
        }

        // Check if we need to the free the descriptor or not
        if (frame->type == TX_INT)
        {
            // Free the frame descriptor
            co_list_push_back(&txl_frame_env.desc_free, &frame->txdesc.list_hdr);
        }
    }
}

uint8_t txl_frame_send_null_frame(uint8_t sta_idx, cfm_func_ptr cfm, void *env)
{
    struct txl_frame_desc_tag *frame;
    struct mac_hdr *buf;
    uint8_t status = CO_FAIL;
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct vif_info_tag *vif_entry = &vif_info_tab[sta_entry->inst_nbr];
    struct phy_channel_info phy_info;
    uint8_t band;
    int txtype;

    do
    {
        // Get the RF band on which we are to know which rate to use
        phy_get_channel(&phy_info, PHY_PRIM);

        #if (NX_P2P)
        if (vif_entry->p2p)
        {
            // If P2P interface, 11b rates are forbidden
            txtype = TX_DEFAULT_5G;
        }
        else
        #endif //(NX_P2P)
        {
            band = phy_info.info1 & 0xFF;
            // Chose the right rate according to the band
            txtype = (band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
        }

        // Allocate a frame descriptor from the TX path
        frame = txl_frame_get(txtype, MAC_NULL_FRAME_SIZE);
        if (frame == NULL)
            break;

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif_entry, frame);

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the frame
        buf->fctl = MAC_FCTRL_NULL_FUNCTION | MAC_FCTRL_TODS;
        buf->durid = 0;
        buf->addr1 = sta_entry->mac_addr;
        buf->addr2 = vif_entry->mac_addr;
        buf->addr3 = sta_entry->mac_addr;
        buf->seq = txl_get_seq_ctrl();

        // Fill-in the confirmation structure
        frame->cfm.cfm_func = cfm;
        frame->cfm.env = env;

        #if (NX_CHNL_CTXT || NX_P2P)
        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = sta_entry->inst_nbr;
        frame->txdesc.host.staid   = sta_idx;
        #endif //(NX_CHNL_CTXT || NX_P2P)

        // Push the frame for TX
        txl_frame_push(frame, AC_VO);

        status = CO_OK;
    } while(0);

    return(status);
}

uint8_t txl_frame_send_qosnull_frame(uint8_t sta_idx, uint16_t qos, cfm_func_ptr cfm, void *env)
{
    struct txl_frame_desc_tag *frame;
    struct mac_hdr_qos *buf;
    uint8_t status = CO_FAIL;
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct vif_info_tag *vif_entry = &vif_info_tab[sta_entry->inst_nbr];
    struct phy_channel_info phy_info;
    uint8_t band;
    int txtype;

    do
    {
        // Get the RF band on which we are to know which rate to use
        phy_get_channel(&phy_info, PHY_PRIM);

        #if (NX_P2P)
        if (vif_entry->p2p)
        {
            // If P2P interface, 11b rates are forbidden
            txtype = TX_DEFAULT_5G;
        }
        else
        #endif //(NX_P2P)
        {
            band = phy_info.info1 & 0xFF;
            // Chose the right rate according to the band
            txtype = (band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
        }

        // Allocate a frame descriptor from the TX path
        frame = txl_frame_get(txtype, MAC_QOS_NULL_FRAME_SIZE);
        if (frame == NULL)
            break;

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif_entry, frame);

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the frame
        buf->fctl = MAC_FCTRL_QOS_NULL;
        buf->durid = 0;
        buf->addr1 = sta_entry->mac_addr;
        buf->addr2 = vif_entry->mac_addr;
        if (vif_entry->type == VIF_STA)
        {
            buf->fctl |= MAC_FCTRL_TODS;
            buf->addr3 = sta_entry->mac_addr;
        }
        else
        {
            buf->fctl |= MAC_FCTRL_FROMDS;
            buf->addr3 = vif_entry->mac_addr;
        }
        buf->seq = 0;
        buf->qos = qos;

        // Fill-in the confirmation structure
        frame->cfm.cfm_func = cfm;
        frame->cfm.env = env;

        #if (NX_CHNL_CTXT || NX_P2P)
        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = sta_entry->inst_nbr;
        frame->txdesc.host.staid   = sta_idx;
        #endif //(NX_CHNL_CTXT || NX_P2P)

        // Push the frame for TX
        txl_frame_push(frame, AC_VO);

        status = CO_OK;
    } while(0);

    return(status);
}

#if (TDLS_ENABLE)
uint8_t txl_frame_send_tdls_channel_switch_req_frame(struct tdls_chan_switch_req const *param, cfm_func_ptr cfm)
{
    struct txl_frame_desc_tag *frame;
    struct mac_hdr_qos *buf;
    uint8_t status = CO_FAIL;
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_index];
    struct phy_channel_info phy_info;
    uint8_t band;
    int txtype;
    uint32_t payload;
    uint8_t i;
    uint16_t ch_switch_time = TDLS_CHSW_SWITCH_TIME_US;
    uint16_t ch_switch_timeout = tdls_get_dt_us(vif_entry->tbtt_timer.time, hal_machw_time()) -
                                 2*ch_switch_time -
                                 3*TDLS_CHSW_TX_FRAME_TIME_US;

    if (ch_switch_timeout > TDLS_MAX_CHSW_SWITCH_TIME_US)
    {
        ch_switch_timeout = TDLS_MAX_CHSW_SWITCH_TIME_US;
    }

    do
    {
        // Get the RF band on which we are to know which rate to use
        phy_get_channel(&phy_info, PHY_PRIM);

        #if (NX_P2P)
        if (vif_entry->p2p)
        {
            // If P2P interface, 11b rates are forbidden
            txtype = TX_DEFAULT_5G;
        }
        else
        #endif //(NX_P2P)
        {
            band = phy_info.info1 & 0xFF;
            // Chose the right rate according to the band
            txtype = (band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
        }

        // Allocate a frame descriptor from the TX path
        frame = txl_frame_get(txtype, MAC_SHORT_QOS_MAC_HDR_LEN +
                MAC_ENCAPSULATED_PAYLOAD_OFT + 38);
        if (frame == NULL)
        {
            break;
        }

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif_entry, frame);

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the action frame to be incapsulated into a data frame
        buf->fctl = MAC_FCTRL_QOS_DATA;
        buf->durid = 0;
        MAC_ADDR_CPY(&buf->addr1, &param->peer_mac_addr);
        MAC_ADDR_CPY(&buf->addr2, &vif_entry->mac_addr);
        MAC_ADDR_CPY(&buf->addr3, &vif_entry->bssid);
        buf->seq = txl_get_seq_ctrl();
        buf->qos = 0x0005; // AC_VI

        payload = CPU2HW(buf) + MAC_SHORT_QOS_MAC_HDR_LEN;

        // Build LLC, SNAP and Payload Type fields
        co_write32p(payload, FRAME_BODY_LLC_H);
        co_write32p(payload + MAC_ENCAPSULATED_LLC_L_OFT, FRAME_BODY_LLC_L);
        co_write8p(payload + MAC_ENCAPSULATED_PAYLOAD_TYPE_OFT, PAYLOAD_TYPE_TDLS);

        // Build the payload
        payload += MAC_ENCAPSULATED_PAYLOAD_OFT;
        // Category
        co_write8p(payload, MAC_TDLS_ACTION_CATEGORY);
        // Action
        co_write8p(payload + MAC_ACTION_ACTION_OFT, MAC_TDLS_ACTION_CHANSW_REQ);
        // Target Channel
        co_write8p(payload + TDLS_CHANSW_REQ_TARGET_CH_OFFSET,
                (uint8_t)phy_freq_to_channel(param->band, param->prim20_freq));
        // Operating Class
        co_write8p(payload + TDLS_CHANSW_REQ_OP_CLASS, param->op_class);
        // Information Elements
        payload += TDLS_CHANSW_REQ_IES_OFFSET;
        // Secondary Channel Offset IE
        if (param->type == PHY_CHNL_BW_40)
        {
            co_write8p(payload, MAC_ELTID_SEC_CH_OFFSET);
            co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_SEC_CH_OFFSET_INFO_LEN);
            if (param->center1_freq > param->prim20_freq)
            {
                co_write8p(payload + MAC_INFOELT_SEC_CH_OFFSET_SEC_CH_OFT, MAC_INFOELT_SEC_CH_OFFSET_SEC_ABOVE);
            }
            else
            {
                co_write8p(payload + MAC_INFOELT_SEC_CH_OFFSET_SEC_CH_OFT, MAC_INFOELT_SEC_CH_OFFSET_SEC_BELOW);
            }
            payload += TDLS_CHANSW_REQ_IE_SEC_CH_OFT_LEN;
        }
        // Link Identifier IE
        co_write8p(payload, MAC_ELTID_LINK_IDENTIFIER);
        co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_LINK_ID_LEN);
        for (i = 0; i < MAC_ADDR_LEN/2; i++)
        {
            co_write16p(payload + MAC_INFOELT_LINK_ID_BSSID_OFT + 2*i, vif_entry->bssid.array[i]);
            if (param->initiator)
            {
                co_write16p(payload + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i, param->peer_mac_addr.array[i]);
                co_write16p(payload + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i, vif_entry->mac_addr.array[i]);
            }
            else
            {
                co_write16p(payload + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i, vif_entry->mac_addr.array[i]);
                co_write16p(payload + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i, param->peer_mac_addr.array[i]);
            }
        }
        payload += TDLS_CHANSW_REQ_IE_LINK_ID_LEN;
        // Channel Switch Timing IE
        co_write8p(payload, MAC_ELTID_CHANNEL_SWITCH_TIMING);
        co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_CH_SWITCH_TIMING_LEN);
        co_write16p(payload + MAC_INFOELT_CH_SWITCH_TIMING_SWTIME_OFT, ch_switch_time);
        co_write16p(payload + MAC_INFOELT_CH_SWITCH_TIMING_SWTOUT_OFT, ch_switch_timeout);
        payload += TDLS_CHANSW_REQ_IE_CH_SWITCH_TIMING_LEN;
        if (param->type > PHY_CHNL_BW_40)
        {
            // Wide Bandwidth Channel Switch IE
            co_write8p(payload, MAC_ELTID_WIDE_BANDWIDTH_CHAN_SWITCH);
            co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_WIDE_BW_CHAN_SWITCH_INFO_LEN);
            co_write8p(payload + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CW_OFT, (param->type == PHY_CHNL_BW_20) ? 0 : (param->type - 1));
            co_write8p(payload + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CENTER1_OFT, param->center1_freq);
            co_write8p(payload + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CENTER2_OFT, param->center2_freq);

            // Country IE (optional)

            // VHT Transmit Power Envelope IE (optional)
        }

        frame->txdesc.lmac.hw_desc->thd.dataendptr =
                (uint32_t)frame->txdesc.lmac.hw_desc->thd.datastartptr + payload - 1;
        frame->txdesc.lmac.hw_desc->thd.frmlen = payload + MAC_FCS_LEN - CPU2HW(buf);

        // Fill-in the confirmation structure
        frame->cfm.cfm_func = cfm;
        frame->cfm.env = vif_entry;

        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = vif_entry->index;
        frame->txdesc.host.staid = vif_entry->u.sta.ap_id;

        // Push the frame for TX
        txl_frame_push(frame, AC_VI);

        status = CO_OK;
    } while(0);

    return (status);
}

uint8_t txl_frame_send_tdls_channel_switch_rsp_frame(struct vif_info_tag *vif_entry, uint16_t status_code, cfm_func_ptr cfm)
{
    struct txl_frame_desc_tag *frame;
    struct mac_hdr_qos *buf;
    uint8_t status = CO_FAIL;
    struct phy_channel_info phy_info;
    uint8_t band;
    int txtype;
    uint32_t payload;
    uint8_t i;
    struct sta_tdls_tag *p_sta_tdls = vif_entry->u.sta.sta_tdls;

    do
    {
        // Get the RF band on which we are to know which rate to use
        phy_get_channel(&phy_info, PHY_PRIM);

        #if (NX_P2P)
        if (vif_entry->p2p)
        {
            // If P2P interface, 11b rates are forbidden
            txtype = TX_DEFAULT_5G;
        }
        else
        #endif //(NX_P2P)
        {
            band = phy_info.info1 & 0xFF;
            // Choose the right rate according to the band
            txtype = (band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
        }

        // Allocate a frame descriptor from the TX path
        frame = txl_frame_get(txtype, MAC_SHORT_QOS_MAC_HDR_LEN +
                MAC_ENCAPSULATED_PAYLOAD_OFT + 30);
        if (frame == NULL)
        {
            break;
        }

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif_entry, frame);

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the action frame to be incapsulated into a data frame
        buf->fctl = MAC_FCTRL_QOS_DATA;
        buf->durid = 0;
        MAC_ADDR_CPY(&buf->addr1, &p_sta_tdls->peer_mac_addr.array[0]);
        MAC_ADDR_CPY(&buf->addr2, &vif_entry->mac_addr);
        MAC_ADDR_CPY(&buf->addr3, &vif_entry->bssid);
        buf->seq = txl_get_seq_ctrl();
        buf->qos = 0x0005; // AC_VI

        payload = CPU2HW(buf) + MAC_SHORT_QOS_MAC_HDR_LEN;

        // Build LLC, SNAP and Payload Type fields
        co_write32p(payload, FRAME_BODY_LLC_H);
        co_write32p(payload + MAC_ENCAPSULATED_LLC_L_OFT, FRAME_BODY_LLC_L);
        co_write8p(payload + MAC_ENCAPSULATED_PAYLOAD_TYPE_OFT, PAYLOAD_TYPE_TDLS);

        // Build the payload
        payload += MAC_ENCAPSULATED_PAYLOAD_OFT;
        co_write8p(payload, MAC_TDLS_ACTION_CATEGORY);

        co_write8p(payload + MAC_ACTION_ACTION_OFT, MAC_TDLS_ACTION_CHANSW_RSP);
        co_write16p(payload + TDLS_CHANSW_RSP_STATUS_OFFSET, status_code);
        payload += TDLS_CHANSW_RSP_IES_OFFSET;
        co_write8p(payload, MAC_ELTID_LINK_IDENTIFIER);
        co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_LINK_ID_LEN);
        for (i = 0; i < MAC_ADDR_LEN/2; i++)
        {
            co_write16p(payload + MAC_INFOELT_LINK_ID_BSSID_OFT + 2*i, vif_entry->bssid.array[i]);
            if (p_sta_tdls->initiator == true)
            {
                co_write16p(payload + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i, p_sta_tdls->peer_mac_addr.array[i]);
                co_write16p(payload + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i, vif_entry->mac_addr.array[i]);
            }
            else
            {
                co_write16p(payload + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i, vif_entry->mac_addr.array[i]);
                co_write16p(payload + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i, p_sta_tdls->peer_mac_addr.array[i]);
            }
        }
        payload += TDLS_CHANSW_REQ_IE_LINK_ID_LEN;
        co_write8p(payload, MAC_ELTID_CHANNEL_SWITCH_TIMING);
        co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_CH_SWITCH_TIMING_LEN);
        co_write16p(payload + MAC_INFOELT_CH_SWITCH_TIMING_SWTIME_OFT, p_sta_tdls->chsw_time);
        co_write16p(payload + MAC_INFOELT_CH_SWITCH_TIMING_SWTOUT_OFT, p_sta_tdls->chsw_timeout);
        payload += TDLS_CHANSW_REQ_IE_CH_SWITCH_TIMING_LEN;

        frame->txdesc.lmac.hw_desc->thd.dataendptr =
                (uint32_t)frame->txdesc.lmac.hw_desc->thd.datastartptr + payload - 1;
        frame->txdesc.lmac.hw_desc->thd.frmlen = payload + MAC_FCS_LEN - CPU2HW(buf);

        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = vif_entry->index;
        frame->txdesc.host.staid = vif_entry->u.sta.ap_id;

        if (status_code == 0)
        {
            // Fill-in the confirmation structure
            frame->cfm.cfm_func = cfm;
            frame->cfm.env = vif_entry;
            status = CO_OK;
        }

        // Push the frame for TX
        txl_frame_push(frame, AC_VI);

    } while(0);

    return (status);
}

uint8_t txl_frame_send_tdls_peer_traffic_ind_frame(struct tdls_peer_traffic_ind_req const *param, cfm_func_ptr cfm)
{
    struct txl_frame_desc_tag *frame;
    struct mac_hdr_qos *buf;
    uint8_t status = CO_FAIL;
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_index];
    struct phy_channel_info phy_info;
    uint8_t band;
    int txtype;
    // Payload length
    uint32_t payload;
    uint8_t i;

    do
    {
        // Get the RF band on which we are to know which rate to use
        phy_get_channel(&phy_info, PHY_PRIM);

        #if (NX_P2P)
        if (vif_entry->p2p)
        {
            // If P2P interface, 11b rates are forbidden
            txtype = TX_DEFAULT_5G;
        }
        else
        #endif //(NX_P2P)
        {
            band = phy_info.info1 & 0xFF;
            // Chose the right rate according to the band
            txtype = (band == PHY_BAND_2G4)?TX_DEFAULT_24G:TX_DEFAULT_5G;
        }

        // Allocate a frame descriptor from the TX path
        frame = txl_frame_get(txtype, MAC_SHORT_QOS_MAC_HDR_LEN +
                MAC_ENCAPSULATED_PAYLOAD_OFT + 32);
        if (frame == NULL)
        {
            break;
        }

        // update Tx power in policy table
        tpc_update_frame_tx_power(vif_entry, frame);

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct mac_hdr_qos *)frame->txdesc.lmac.buffer->payload;
        #endif

        // Fill-in the action frame to be incapsulated into a data frame
        buf->fctl = MAC_FCTRL_QOS_DATA;
        buf->durid = 0;
        MAC_ADDR_CPY(&buf->addr1, &vif_entry->bssid);
        MAC_ADDR_CPY(&buf->addr2, &vif_entry->mac_addr);
        buf->fctl |= MAC_FCTRL_TODS;
        MAC_ADDR_CPY(&buf->addr3, &param->peer_mac_addr);
        buf->seq = txl_get_seq_ctrl();
        buf->qos = 0x0005; // AC_VI

        payload = CPU2HW(buf) + MAC_SHORT_QOS_MAC_HDR_LEN;

        // Build LLC, SNAP and Payload Type fields
        co_write32p(payload, FRAME_BODY_LLC_H);
        co_write32p(payload + MAC_ENCAPSULATED_LLC_L_OFT, FRAME_BODY_LLC_L);
        co_write8p(payload + MAC_ENCAPSULATED_PAYLOAD_TYPE_OFT, PAYLOAD_TYPE_TDLS);

        // Build the payload
        payload += MAC_ENCAPSULATED_PAYLOAD_OFT;
        // Category
        co_write8p(payload, MAC_TDLS_ACTION_CATEGORY);
        // Action
        co_write8p(payload + MAC_ACTION_ACTION_OFT, MAC_TDLS_ACTION_PEER_TRAFFIC_IND);
        // Dialog token
        co_write16p(payload + MAC_ACTION_TOKEN_OFT, param->dialog_token);

        // Information Elements
        payload += TDLS_PEER_TRAFFIC_IND_IES_OFFSET;
        // Link Identifier IE
        co_write8p(payload, MAC_ELTID_LINK_IDENTIFIER);
        co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_LINK_ID_LEN);
        for (i = 0; i < MAC_ADDR_LEN/2; i++)
        {
            co_write16p(payload + MAC_INFOELT_LINK_ID_BSSID_OFT + 2*i, vif_entry->bssid.array[i]);
            if (vif_entry->u.sta.sta_tdls->initiator)
            {
                co_write16p(payload + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i, param->peer_mac_addr.array[i]);
                co_write16p(payload + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i, vif_entry->mac_addr.array[i]);
            }
            else
            {
                co_write16p(payload + MAC_INFOELT_LINK_ID_INIT_STA_OFT + 2*i, vif_entry->mac_addr.array[i]);
                co_write16p(payload + MAC_INFOELT_LINK_ID_RESP_STA_OFT + 2*i, param->peer_mac_addr.array[i]);
            }
        }
        payload += TDLS_PEER_TRAFFIC_IND_IE_LINK_ID_LEN;
        // PTI Control IE (optional)
        co_write8p(payload, MAC_ELTID_PTI_CONTROL);
        co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_PTI_CONTROL_LEN);
        co_write8p(payload + MAC_INFOELT_PTI_CONTROL_TID_OFT, param->last_tid);
        co_write16p(payload + MAC_INFOELT_PTI_CONTROL_SEQ_CTRL_OFT, param->last_sn);
        payload += TDLS_PEER_TRAFFIC_IND_IE_PTI_CTRL_LEN;
        // TPU Buffer Status IE
        co_write8p(payload, MAC_ELTID_TPU_BUFFER_STATUS);
        co_write8p(payload + MAC_INFOELT_LEN_OFT, MAC_INFOELT_TPU_BUF_STATUS_LEN);
        co_write8p(payload + MAC_INFOELT_TPU_BUF_STATUS_AC_STATUS, vif_entry->u.sta.uapsd_queues);

        frame->txdesc.lmac.hw_desc->thd.dataendptr =
                (uint32_t)frame->txdesc.lmac.hw_desc->thd.datastartptr + payload - 1;
        frame->txdesc.lmac.hw_desc->thd.frmlen = payload + MAC_FCS_LEN - CPU2HW(buf);

        // Fill-in the confirmation structure
        frame->cfm.cfm_func = cfm;
        frame->cfm.env = vif_entry;

        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = vif_entry->index;
        frame->txdesc.host.staid = vif_entry->u.sta.ap_id;

        // Push the frame for TX
        txl_frame_push(frame, AC_VI);

        status = CO_OK;

    } while(0);

    return (status);
}

#endif // TDLS_ENABLE
#endif // NX_TX_FRAME
// eof

