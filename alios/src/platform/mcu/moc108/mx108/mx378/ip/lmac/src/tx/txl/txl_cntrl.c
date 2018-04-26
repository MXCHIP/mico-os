/**
 ****************************************************************************************
 *
 * @file txl_cntrl.c
 *
 * @brief LMAC TxPath implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */
#include "co_int.h"
#include "co_bool.h"
#include <string.h>

#include "arch.h"
#include "mac.h"

#include "mac_defs.h"
#include "mac_frame.h"
#include "txl_hwdesc.h"
#include "mm.h"
#include "txl_buffer.h"

#if NX_TX_FRAME
#include "txl_frame.h"
#endif

#include "tx_swdesc.h"
#include "txl_cfm.h"
#include "txl_cntrl.h"
#include "rxl_cntrl.h"
#include "reg_mac_pl.h"
#include "reg_mac_core.h"
#include "ps.h"

#if (NX_P2P)
#include "p2p.h"
#endif //(NX_P2P)

#if (NX_TD)
#include "td.h"
#endif //(NX_TD)

#include "txu_cntrl.h"

#if NX_MFP
#include "mfp.h"
#endif

#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#include "include.h"
#include "arm_arch.h"
#include "sdio_intf_pub.h"
#include "intc_pub.h"

#include "uart_pub.h"
#include "tx_evm_pub.h"
#include "tx_swdesc.h"


#ifdef CFG_BK7221_MDM_WATCHDOG_PATCH
void rc_reset_patch(void);
#endif

/*
 * DEFINES
 ****************************************************************************************
 */
/// TX IRQ bits enabled
#if NX_BEACONING
#define TX_IRQ_BITS  ( NXMAC_AC_0_TX_TRIGGER_BIT | NXMAC_AC_1_TX_TRIGGER_BIT |           \
                       NXMAC_AC_2_TX_TRIGGER_BIT | NXMAC_AC_3_TX_TRIGGER_BIT |           \
                       NXMAC_BCN_TX_TRIGGER_BIT )
#else
#define TX_IRQ_BITS  ( NXMAC_AC_0_TX_TRIGGER_BIT | NXMAC_AC_1_TX_TRIGGER_BIT |           \
                       NXMAC_AC_2_TX_TRIGGER_BIT | NXMAC_AC_3_TX_TRIGGER_BIT)
#endif

#if RW_MUMIMO_TX_EN
/// Bits indicating activity on secondary users (merged trigger and buffer)
#define TX_SEC_IRQ_BITS_MERGED  ( NXMAC_SEC_U_3AC_3_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_3AC_2_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_3AC_1_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_3AC_0_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_2AC_3_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_2AC_2_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_2AC_1_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_2AC_0_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_1AC_3_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_1AC_2_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_1AC_1_TX_TRIGGER_BIT     |                 \
                                  NXMAC_SEC_U_1AC_0_TX_TRIGGER_BIT )
#endif

/// Flags indicating that a frame has been correctly transmitted by the HW
#define FRAME_OK  (DESC_DONE_TX_BIT | FRAME_SUCCESSFUL_TX_BIT)

#if RW_MUMIMO_TX_EN
#define MU_AMPDU_CLOSE()                                                       \
{                                                                              \
    txl_mumimo_ampdu_finish(ac, user_pos);                                     \
    /* Check if the other user positions are still open */                     \
    if (txlist->mumimo.open & txlist->mumimo.users)                            \
    {                                                                          \
        /* We could still receive packets on other user positions, */          \
        /* so save the current packet */                                       \
        ASSERT_ERR(txlist->mumimo.txdesc[user_pos] == NULL);                   \
        txlist->mumimo.txdesc[user_pos] = txdesc;                              \
        status = MU_PAUSED;                                                    \
        break;                                                                 \
    }                                                                          \
    else                                                                       \
    {                                                                          \
        /* Close the MU-MIMO PPDU under construction */                        \
        txl_mumimo_close(ac);                                                  \
                                                                               \
        /* Start a new MU-MIMO PPDU */                                         \
        continue;                                                              \
    }                                                                          \
}
#endif


#if NX_AMPDU_TX
/// Table linking HT or VHT MCS+GI+BW to minimum number of bytes for 1us min MPDU start spacing in AMPDU
/// Index of the table is (MCS_IDX << 3) | (BW << 1) | (GI_400)
///  where BW is 0 for 20 MHz, 1 for 40MHz and 2 for 80MHz
///        GI_400 is 1 if packet is being sent with 400ns GI, 0 if 800ns GI
const uint8_t TX_RATE_TO_MIN_SEP[10*4*2] =
{
    //BW20,GI800  BW20,GI400    BW40,GI800    BW40,GI400   MCS Index
    [  0] =  1,   [  1] =  1,   [  2] =  2,   [  3] =  2, //MCS 0
    [  8] =  2,   [  9] =  2,   [ 10] =  4,   [ 11] =  4, //MCS 1
    [ 16] =  3,   [ 17] =  3,   [ 18] =  6,   [ 19] =  6, //MCS 2
    [ 24] =  4,   [ 25] =  4,   [ 26] =  7,   [ 27] =  8, //MCS 3
    [ 32] =  5,   [ 33] =  6,   [ 34] = 11,   [ 35] = 12, //MCS 4
    [ 40] =  7,   [ 41] =  8,   [ 42] = 14,   [ 43] = 15, //MCS 5
    [ 48] =  8,   [ 49] =  9,   [ 50] = 16,   [ 51] = 17, //MCS 6
    [ 56] =  9,   [ 57] = 10,   [ 58] = 17,   [ 59] = 19, //MCS 7
    [ 64] = 10,   [ 65] = 11,   [ 66] = 21,   [ 67] = 23, //MCS 8
    [ 72] = 11,   [ 73] = 13,   [ 74] = 23,   [ 75] = 25, //MCS 9
    //BW80,GI800  BW80,GI400    BW160,GI800   BW160,GI400
    [  4] =  4,   [  5] =  5,   [  6] =  8,   [  7] =  9, //MCS 0
    [ 12] =  8,   [ 13] =  9,   [ 14] = 15,   [ 15] = 17, //MCS 1
    [ 20] = 11,   [ 21] = 13,   [ 22] = 22,   [ 23] = 25, //MCS 2
    [ 28] = 15,   [ 29] = 17,   [ 30] = 30,   [ 31] = 33, //MCS 3
    [ 36] = 22,   [ 37] = 25,   [ 38] = 44,   [ 39] = 49, //MCS 4
    [ 44] = 30,   [ 45] = 33,   [ 46] = 59,   [ 47] = 65, //MCS 5
    [ 52] = 33,   [ 53] = 37,   [ 54] = 66,   [ 55] = 74, //MCS 6
    [ 60] = 37,   [ 61] = 41,   [ 62] = 74,   [ 63] = 82, //MCS 7
    [ 68] = 44,   [ 69] = 49,   [ 70] = 88,   [ 71] = 98, //MCS 8
    [ 76] = 49,   [ 77] = 55,   [ 78] = 98,   [ 79] = 109, //MCS 9
};


/// Table linking HT or VHT MCS+GI+BW to the number of bytes transmitted in 32us
/// Index of the table is (MCS_IDX << 3) | (BW << 1) | (GI_400)
///  where BW is 0 for 20 MHz, 1 for 40MHz and 2 for 80MHz
///        GI_400 is 1 if packet is being sent with 400ns GI, 0 if 800ns GI
const uint16_t TX_RATE_TO_32US_LEN[10*4*2] =
{
    //BW20,GI800    BW20,GI400      BW40,GI800      BW40,GI400   MCS Index
    [  0] =   26,   [  1] =   28,   [  2] =   54,   [  3] =   60, //MCS 0
    [  8] =   52,   [  9] =   57,   [ 10] =  108,   [ 11] =  120, //MCS 1
    [ 16] =   78,   [ 17] =   86,   [ 18] =  162,   [ 19] =  180, //MCS 2
    [ 24] =  104,   [ 25] =  115,   [ 26] =  216,   [ 27] =  240, //MCS 3
    [ 32] =  156,   [ 33] =  173,   [ 34] =  324,   [ 35] =  360, //MCS 4
    [ 40] =  208,   [ 41] =  231,   [ 42] =  432,   [ 43] =  480, //MCS 5
    [ 48] =  234,   [ 49] =  260,   [ 50] =  486,   [ 51] =  540, //MCS 6
    [ 56] =  260,   [ 57] =  288,   [ 58] =  540,   [ 59] =  600, //MCS 7
    [ 64] =  312,   [ 65] =  346,   [ 66] =  648,   [ 67] =  720, //MCS 8
    [ 72] =  346,   [ 73] =  385,   [ 74] =  720,   [ 75] =  800, //MCS 9
    //BW80,GI800  BW80,GI400    BW160,GI800   BW160,GI400
    [  4] =  117,   [  5] =  130,   [  6] =  234,   [  7] =  260, //MCS 0
    [ 12] =  234,   [ 13] =  260,   [ 14] =  468,   [ 15] =  520, //MCS 1
    [ 20] =  351,   [ 21] =  390,   [ 22] =  702,   [ 23] =  780, //MCS 2
    [ 28] =  468,   [ 29] =  520,   [ 30] =  936,   [ 31] = 1040, //MCS 3
    [ 36] =  702,   [ 37] =  780,   [ 38] = 1404,   [ 39] = 1560, //MCS 4
    [ 44] =  936,   [ 45] = 1040,   [ 46] = 1872,   [ 47] = 2080, //MCS 5
    [ 52] = 1053,   [ 53] = 1170,   [ 54] = 2106,   [ 55] = 2340, //MCS 6
    [ 60] = 1170,   [ 61] = 1300,   [ 62] = 2340,   [ 63] = 2600, //MCS 7
    [ 68] = 1404,   [ 69] = 1560,   [ 70] = 2808,   [ 71] = 3120, //MCS 8
    [ 76] = 1560,   [ 77] = 1733,   [ 78] = 3120,   [ 79] = 3466, //MCS 9
};
#endif

// VHT NSYM
#define VHT_BW   4
#define VHT_MCS  10

// IEEE P802.11ac D3.0  Chptr 22.5 Parameters for VHT MCSs
// Note that some BW, MCS, NSS combinations are not allowed (e.g 20MHz, MCS9, NSS 1,2)
// The NDBPS value given for MCS9 20MHz is for NSS=3
const uint16_t VHT_NDBPS[VHT_BW][VHT_MCS] = {
    // MCS Index:  0     1      2       3      4      5      6      7      8      9
   [BW_20MHZ]  = {26,   52,    78,     104,   156,   208,   234,   260,   312,   1040},
   [BW_40MHZ]  = {54,   108,   162,    216,   324,   432,   486,   540,   648,   720 },
   [BW_80MHZ]  = {117,  234,   351,    468,   702,   936,   1053,  1170,  1404,  1560},
   [BW_160MHZ] = {234,  468,   702,    936,   1404,  1872,  2106,  2340,  2808,  3120},
};

/// Table containing the TX timeout value per TX queue
const uint32_t TX_TIMEOUT[NX_TXQ_CNT] =
{
    TX_AC0_TIMEOUT,
    TX_AC1_TIMEOUT,
    TX_AC2_TIMEOUT,
    TX_AC3_TIMEOUT,
    #if (NX_BEACONING)
    TX_BCN_TIMEOUT
    #endif
};

/*
 * GLOBAL VARIABLE DEFINITION
 ****************************************************************************************
 */
struct txl_cntrl_env_tag txl_cntrl_env;

/*
 * FUNCTION BODIES
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Move the TX timer according to the access category
 *
 * @param[in] ac  Access category for which the timer has to be moved
 *
 ****************************************************************************************
 */
__INLINE void txl_timer_move(uint8_t ac)
{
    uint32_t curr_time = hal_machw_time();

    // Set pattern at end to not validate descriptor until it's ready
    nxmac_abs_timer_set(TX_AC2TIMER(ac), curr_time + TX_TIMEOUT[ac]);
}

#if NX_TX_FRAME
/**
 ****************************************************************************************
 * @param[in] txdesc           Descriptor of the packet to be pushed into the Tx list
 * @param[in] access_category  Access category where to push the descriptor
 *
 ****************************************************************************************
 */
static void txl_int_fake_transfer(struct txdesc *txdesc, uint8_t access_category)
{    
    #if NX_AMSDU_TX
    struct txl_buffer_tag *buf = txdesc->lmac.buffer[0];
    #else
    struct txl_buffer_tag *buf = txdesc->lmac.buffer;
    #endif
    
    buf->txdesc = txdesc;
    txl_buffer_push(access_category, buf);

    ke_evt_set(ke_get_ac_payload_bit(access_category));
}
#endif

/**
 ****************************************************************************************
 * @brief Prepare the transfer of payload from host memory to emb memory
 *
 * This primitive first allocates a buffer of the required size, and then updates the
 * bridge descriptor according to the allocated buffer.
 *
 * @param[in] txdesc           Descriptor of the packet to be pushed into the Tx list
 * @param[in] access_category  Access category where to push the descriptor
 *
 * @return The status of the transfer (TRUE: programmed, FALSE: not programmed due to buffer full)
 *
 ****************************************************************************************
 */
static bool txl_payload_transfer(struct txdesc *txdesc, 
									uint8_t access_category,
									uint8_t user_idx)
{
    struct txl_buffer_tag *buffer = NULL;
    bool buffer_full = true;
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];

    #if NX_TX_FRAME
    if (is_int_frame(txdesc))
    {
        txl_int_fake_transfer(txdesc, access_category);
        return (false);
    }
    #endif

    // Allocate the payload buffer
    #if NX_AMSDU_TX
    buffer = txl_buffer_alloc(txdesc, access_category, user_idx, txlist->dwnld_index[user_idx]);
    #else
    buffer = txl_buffer_alloc(txdesc, access_category, user_idx);
    #endif
    if (buffer != NULL)
    {
        // Update the Tx descriptor
        #if NX_AMSDU_TX
        txdesc->lmac.buffer[txlist->dwnld_index[user_idx]] = buffer;
        #else
        txdesc->lmac.buffer = buffer;
        #endif

        buffer->txdesc = txdesc;

        #if NX_AMSDU_TX
        if (txlist->dwnld_index[user_idx] > 0)
            // Set start and end pointers to the beginning and end of the frame
            txl_buffer_update_tbd(txdesc, access_category, txlist->dwnld_index[user_idx]);
        else
        #endif
            // Set start and end pointers to the beginning and end of the frame
            txl_buffer_update_thd(txdesc, access_category);

        #if NX_AMSDU_TX
        // Increase the packet index
        txlist->dwnld_index[user_idx]++;
        #endif

        // Update the status
        buffer_full = false;
    }
    else
    {
        txlist->first_to_download[user_idx] = txdesc;
    }

    return (buffer_full);
}

/**
 ****************************************************************************************
 * @brief Prepare the transfer of payload from host memory to emb memory
 *
 * This primitive first allocates a buffer of the required size, and then updates the
 * bridge descriptor according to the allocated buffer.
 *
 * @param[in] txdesc           Descriptor of the packet to be pushed into the Tx list
 * @param[in] access_category  Access category where to push the descriptor
 * @param[in] user_idx         User index (for MU-MIMO TX only, 0 otherwise)
 *
 * @return The status of the transfer (TRUE: programmed, FALSE: not programmed due to buffer full)
 *
 ****************************************************************************************
 */
static bool txl_payload_alloc(struct txdesc *txdesc, uint8_t access_category,
                              uint8_t user_idx)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    bool success = false;

    #if NX_AMSDU_TX
    while (1)
    {
        if ((txl_buffer_count(access_category, user_idx) >= TX_MIN_DOWNLOAD_CNT) 
            && (txlist->first_to_download[user_idx] == NULL))
        {
            txlist->first_to_download[user_idx] = txdesc;
            break;
        }

        success = !txl_payload_transfer(txdesc, access_category, user_idx);
        if (!success)
            break;

        if (txlist->dwnld_index[user_idx] == txdesc->host.packet_cnt)
        {
            txlist->dwnld_index[user_idx] = 0;
            break;
        }
    }
    #else
    if (txl_buffer_count(access_category, user_idx) < TX_MIN_DOWNLOAD_CNT)
        // Try to allocate a buffer and transfer it
        success = !txl_payload_transfer(txdesc, access_category, user_idx);
    else
        txlist->first_to_download[user_idx] = txdesc;
    #endif

    return (success);
}

/**
 ****************************************************************************************
 * @brief Initialization of the HW descriptors prior to payload download
 *
 * This primitive configures the HW descriptors prior to the download of the payloads
 *
 * @param[in] txdesc          Descriptor of the packet to be pushed into the Tx queue
 * @param[in] access_category TX queue index
 *
 ****************************************************************************************
 */
static void txl_hwdesc_config_pre(struct txdesc *txdesc, int access_category)
{    
    struct tx_hd *txhd = &txdesc->lmac.hw_desc->thd;
    int add_len = txdesc->umac.head_len + txdesc->umac.tail_len;

    // Set frame length
    #if NX_AMSDU_TX
    txhd->frmlen = add_len + MAC_FCS_LEN;
    for (int i = 0; i < txdesc->host.packet_cnt; i++)
    {
        txhd->frmlen += txdesc->host.packet_len[i];
    }
    #else
    txhd->frmlen = txdesc->host.packet_len + add_len + MAC_FCS_LEN;
    #endif

    // Reset the THD pattern
    txhd->upatterntx = TX_HEADER_DESC_PATTERN;

    // By default no next MPDU descriptor chained
    txhd->nextmpdudesc_ptr = 0;

    // No next Frame Exchange chained
    txhd->nextfrmexseq_ptr = 0;

    // No payload descriptor
    txhd->first_pbd_ptr = 0;

    // Reset policy table pointer
    txhd->policyentryaddr = 0;

    // Set default WhichDesc value
    txhd->macctrlinfo2 = WHICHDESC_UNFRAGMENTED_MSDU;

    // Reset data pointers
    txhd->datastartptr = 0;
    txhd->dataendptr = 0;

    // Set frame lifetime
    txhd->frmlifetime = 0;

    // Set status as 0
    txhd->statinfo = 0;
}


/**
 ****************************************************************************************
 * @brief Initialization of the HW descriptors, after payload download
 *
 * This primitive configures the HW descriptors prior to transmission
 *
 * @param[in] txdesc          Descriptor of the packet to be pushed into the Tx queue
 * @param[in] access_category TX queue index
 *
 ****************************************************************************************
 */
static void txl_hwdesc_config_post(struct txdesc *txdesc, uint8_t access_category)
{
    struct txl_buffer_control *bufctrl = txl_buffer_control_get(txdesc);
    struct tx_hd *txhd = &txdesc->lmac.hw_desc->thd;
    #if (RW_BFMER_EN)
    // Do not set smoothing bit if frame is beamformed
    uint32_t smoothing = (bfr_is_enabled() && txdesc->lmac.p_bfr_node) ? 0 : SMTHN_TX_BIT;
    #else
    uint32_t smoothing = SMTHN_TX_BIT;
    #endif //(RW_BFMER_EN)

    #if NX_AMPDU_TX
    #if RW_MUMIMO_TX_EN
    // Check if the MPDU is inside a MU-MIMO PPDU
    if (is_in_mumimo_ppdu(txdesc))
    {
        // Sanity check - The TX descriptor shall have a beamforming node attached
        ASSERT_ERR(txdesc->lmac.p_bfr_node);

        // Handle differently the primary and secondary users
        if (is_primary_user(txdesc))
        {
            // only on the first descriptor
            if (is_mpdu_first(txdesc))
            {
                struct txl_buffer_tag *buf = txl_buffer_get(txdesc);
                struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;
                struct tx_hd *a_thd = &agg_desc->a_thd;
                struct tx_policy_tbl *umac_pt = &bufctrl->policy_tbl;
                struct tx_policy_tbl *mu_pt = &buf->policy_tbl;

                // first MPDU of the A-MPDU has been downloaded
                agg_desc->status |= AGG_FIRST_DOWNLOADED;

                //set policy table pointer
                a_thd->policyentryaddr = CPU2HW(mu_pt);

                // A-THD phy control information (don't care about GroupId, PAID and MU-MIMO
                // TX bit set by the host as these fields are handled internally in case
                // of MU-MIMO transmission)
                a_thd->phyctrlinfo |= bufctrl->phy_control_info &
                                      ~(PAID_TX_MASK | GID_TX_MASK | USE_MUMIMO_TX_BIT);

                // Copy policy table
                *mu_pt = *umac_pt;

                // Set the rate to the correct value
                ASSERT_ERR((txdesc->umac.phy_flags & VHT_NSS_MASK) == 0);
                mu_pt->ratecntrlinfo[0] = txdesc->umac.phy_flags;

                // Disable the STBC in case of MU-MIMO transmission
                mu_pt->phycntrlinfo1 &= ~STBC_PT_MASK;
            }
        }
        else if (!is_mpdu_agg(txdesc) || is_mpdu_first(txdesc))
        {
            struct txl_buffer_tag *buf = txl_buffer_get(txdesc);
            struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;
            struct tx_policy_tbl *pt = &bufctrl->policy_tbl;
            struct tx_compressed_policy_tbl *cpt = &buf->comp_pol_tbl;
            uint32_t hw_key_idx = (pt->maccntrlinfo1 & KEYSRAM_INDEX_MASK) >> KEYSRAM_INDEX_OFT;
            uint32_t fec_coding = (pt->phycntrlinfo1 & FEC_CODING_PT_BIT) >> FEC_CODING_PT_OFT;
            uint32_t mcs_idx = (txdesc->umac.phy_flags & MCS_INDEX_TX_RCX_MASK)
                                                                     >> MCS_INDEX_TX_RCX_OFT;
            uint32_t smm_idx = txdesc->lmac.p_bfr_node->smm_index;

            // Get TX header descriptor pointer that describe the transmission
            txhd = is_mpdu_agg(txdesc) ? &agg_desc->a_thd : txhd;

            // Set the compressed policy table address
            txhd->policyentryaddr = CPU2HW(cpt);

            // Update the compressed policy table
            cpt->upatterntx = POLICY_TABLE_PATTERN;
            cpt->sec_user_control = (mcs_idx << MCS_IDX_TX_CPT_OFT) |
                                    (fec_coding << FEC_CODING_CPT_OFT) |
                                    (smm_idx << SMM_INDEX_CPT_OFT) |
                                    (hw_key_idx << KEYSRAM_INDEX_CPT_OFT);
        }
    }
    else
    #endif
    // Config HW descriptor of AMPDU and BAR too
    if (is_mpdu_agg(txdesc))
    {
        // only on the first descriptor
        if (is_mpdu_first(txdesc))
        {
            struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;
            struct tx_hd *a_thd = &agg_desc->a_thd;
            #if RW_MUMIMO_TX_EN
            struct txl_buffer_tag *buf = txl_buffer_get(txdesc);
            struct tx_policy_tbl *umac_pt = &bufctrl->policy_tbl;
            struct tx_policy_tbl *mu_pt = &buf->policy_tbl;

            // Check if the A-MPDU was initially planned to be part of a MU PPDU
            if (agg_desc->status & AGG_MU)
            {
                // In such we need to use the PHY flags that were used to create
                // the A-MPDU
                // Copy policy table
                *mu_pt = *umac_pt;
                mu_pt->ratecntrlinfo[0] = txdesc->umac.phy_flags;

                //set policy table pointer
                a_thd->policyentryaddr = CPU2HW(mu_pt);
            }
            else
            #endif
            {
                //set policy table pointer
                a_thd->policyentryaddr = CPU2HW(&bufctrl->policy_tbl);
            }

            // first MPDU of the A-MPDU has been downloaded
            agg_desc->status |= AGG_FIRST_DOWNLOADED;

            //A-THD phy control information
            a_thd->phyctrlinfo = bufctrl->phy_control_info | smoothing;
            //A-THD macctrlinfo1 field
            a_thd->macctrlinfo1 = bufctrl->mac_control_info;
            a_thd->macctrlinfo1 &= (~EXPECTED_ACK_MSK);
            a_thd->macctrlinfo1 |= EXPECTED_ACK_COMPRESSED_BLOCK_ACK;
        }
    }
    else
    #endif
    {
        if (txdesc->host.flags & TXU_CNTRL_MGMT)
        {
            struct txl_buffer_tag *buffer = txl_buffer_get(txdesc);
            struct mac_hdr *hdr;

            // Perform last settings
            hdr = (struct mac_hdr *)buffer->payload  + buffer->padding;

            #if NX_MFP
            if (txdesc->host.flags & TXU_CNTRL_MGMT_ROBUST)
            {
                if (txdesc->umac.head_len)
                {
                    // Unicast frame, need to add security header
                    txu_cntrl_protect_mgmt_frame(txdesc, (uint32_t)hdr,
                                                 MAC_SHORT_MAC_HDR_LEN);
                }
                else if (txdesc->umac.tail_len)
                {
                    #if NX_AMSDU_TX
                    int len = txdesc->host.packet_len[0];
                    #else
                    int len = txdesc->host.packet_len;
                    #endif
                    // Broadcast frame, need to add MGMT MIC
                    mfp_add_mgmt_mic(txdesc, CPU2HW(hdr), len);
                }
            }
            #endif

            txhd->macctrlinfo2 &= ~(WHICHDESC_MSK | UNDER_BA_SETUP_BIT);
            if (MAC_ADDR_GROUP(&hdr->addr1))
                txhd->macctrlinfo1 = EXPECTED_ACK_NO_ACK;
            else
                txhd->macctrlinfo1 = EXPECTED_ACK_NORMAL_ACK;

            txhd->statinfo = 0;
        }
        else
        {
            txu_cntrl_tkip_mic_append(txdesc, access_category);

            // Set MAC control info 1 field
            txhd->macctrlinfo1 = bufctrl->mac_control_info;
            #if NX_AMSDU_TX
            // Check if this packet is split across several buffers
            if (txdesc->host.packet_cnt > 1)
            {
                struct tx_policy_tbl *pol = &bufctrl->policy_tbl;

                // Split packet - Disable HW retransmissions
                pol->maccntrlinfo2 &= ~(LONG_RETRY_LIMIT_MASK | SHORT_RETRY_LIMIT_MASK);
            }
            #endif
        }

        // Set phy control info
        txhd->phyctrlinfo = bufctrl->phy_control_info | smoothing;
        // Set policy table address
        txhd->policyentryaddr = CPU2HW(&bufctrl->policy_tbl);
    }

    #if (RW_BFMER_EN)
    if (bfr_is_enabled())
    {
        bfr_tx_configure(txdesc);
    }
    #endif //(RW_BFMER_EN)
}


/**
 ****************************************************************************************
 * @brief Format the MAC header
 *
 * @param[in] machdrptr  Address of the MAC header in shared memory
 *
 ****************************************************************************************
 */
void txl_machdr_format(uint32_t machdrptr)
{
    // Read the fragmentation number of the frame
    uint16_t seq_ctrl = co_read8p(machdrptr + MAC_HEAD_CTRL_OFT) & MAC_SEQCTRL_FRAG_MSK;

    // Non QoS data - Use common sequence number.
    // Check if we need to increment the sequence number (done for first fragment only)
    if (seq_ctrl == 0)
    {
        // Increment the sequence number
        txl_cntrl_env.seqnbr++;
    }
    seq_ctrl |= txl_cntrl_env.seqnbr << MAC_SEQCTRL_NUM_OFT;

    // Write back the sequence control field
    co_write16p(machdrptr + MAC_HEAD_CTRL_OFT, seq_ctrl);
}

/**
 ****************************************************************************************
 * @brief Update the header pointer and set the new head bit for the specified access
 * category.
 *
 * @param[in] desc              Pointer to the header descriptor
 * @param[in] access_category   Access category
 *
 ****************************************************************************************
 */
static void txl_cntrl_newhead(uint32_t desc,
                              uint8_t access_category)
{
    int timer_id = TX_AC2TIMER(access_category);
    uint32_t timer_bit = CO_BIT(timer_id);
    uint32_t curr_time = hal_machw_time();

    // Set the header pointer and the new head bit according to the access category
    switch (access_category)
    {
        #if NX_BEACONING
        case AC_BCN:
            ASSERT_REC(nxmac_tx_bcn_state_getf() != 2);
            nxmac_tx_bcn_head_ptr_set(desc);
            nxmac_dma_cntrl_set(NXMAC_TX_BCN_NEW_HEAD_BIT);
            nxmac_abs_timer_set(timer_id, curr_time + TX_BCN_TIMEOUT);
            break;
        #endif
        
        case AC_VO:            
            ASSERT_REC(nxmac_tx_ac_3_state_getf() != 2);
            nxmac_tx_ac_3_head_ptr_set(desc);
            nxmac_dma_cntrl_set(NXMAC_TX_AC_3_NEW_HEAD_BIT);
            nxmac_abs_timer_set(timer_id, curr_time + TX_AC3_TIMEOUT);
            break;
            
        case AC_VI:
            ASSERT_REC(nxmac_tx_ac_2_state_getf() != 2);
            nxmac_tx_ac_2_head_ptr_set(desc);
            nxmac_dma_cntrl_set(NXMAC_TX_AC_2_NEW_HEAD_BIT);
            nxmac_abs_timer_set(timer_id, curr_time + TX_AC2_TIMEOUT);
            break;
            
        case AC_BE:
            ASSERT_REC(nxmac_tx_ac_1_state_getf() != 2);
            nxmac_tx_ac_1_head_ptr_set(desc);
            nxmac_dma_cntrl_set(NXMAC_TX_AC_1_NEW_HEAD_BIT);
            nxmac_abs_timer_set(timer_id, curr_time + TX_AC1_TIMEOUT);
            break;
            
        case AC_BK:
            ASSERT_REC(nxmac_tx_ac_0_state_getf() != 2);
            nxmac_tx_ac_0_head_ptr_set(desc);
            nxmac_dma_cntrl_set(NXMAC_TX_AC_0_NEW_HEAD_BIT);
            nxmac_abs_timer_set(timer_id, curr_time + TX_AC0_TIMEOUT);
            break;
			
        default:
            break;
    }

    // Enable the TX queue timeout
    nxmac_timers_int_event_clear(timer_bit);
    nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() | timer_bit);
}

/**
 ****************************************************************************************
 * @brief Set the new tail bit for the specified access category.
 *
 * @param[in] access_category   Access category
 *
 ****************************************************************************************
 */
static void txl_cntrl_newtail(uint8_t access_category)
{
    // Set the header pointer and the new head bit according to the access category
    switch (access_category)
    {
        #if NX_BEACONING
        case AC_BCN:
            nxmac_dma_cntrl_set(NXMAC_TX_BCN_NEW_TAIL_BIT);
            break;
        #endif
		
        case AC_VO:
            nxmac_dma_cntrl_set(NXMAC_TX_AC_3_NEW_TAIL_BIT);
            break;
			
        case AC_VI:
            nxmac_dma_cntrl_set(NXMAC_TX_AC_2_NEW_TAIL_BIT);
            break;
			
        case AC_BE:
            nxmac_dma_cntrl_set(NXMAC_TX_AC_1_NEW_TAIL_BIT);
            break;
			
        case AC_BK:
            nxmac_dma_cntrl_set(NXMAC_TX_AC_0_NEW_TAIL_BIT);
            break;
			
		default:
			break;
    }
}


/**
 ****************************************************************************************
 * @brief Chain a new frame exchange to the MAC HW.
 * Depending if there is already a frame exchange ongoing or not, this function will
 * proceed to a newTail or newHead action.
 *
 * @param[in] first_thd         Pointer to the first THD of the frame exchange
 * @param[in] last_thd          Pointer to the last THD of the frame exchange
 * @param[in] access_category   Access category
 *
 ****************************************************************************************
 */
static void txl_frame_exchange_chain(struct tx_hd *first_thd,
                                     struct tx_hd *last_thd,
                                     uint8_t access_category)
{
    struct tx_hd *prev_hd;

    GLOBAL_INT_DECLARATION();

    #if CFG_BK7221_MDM_WATCHDOG_PATCH
        rc_reset_patch();
    #endif
    
    GLOBAL_INT_DISABLE();
	
    prev_hd = txl_cntrl_env.txlist[access_category].last_frame_exch;
	
    // Previous frame exchange, if existing, must be chained to the new frame exchange
    if (prev_hd != NULL)
    {        
        // Chain the new header descriptor to the previous one
        prev_hd->nextfrmexseq_ptr = CPU2HW(first_thd);
		
        // Set the newTail bit
        txl_cntrl_newtail(access_category);
    }
    else
    {
        // Write the header desc pointer to the HW register and set the newHead bit
        txl_cntrl_newhead(CPU2HW(first_thd), access_category);
    }

    // Remind the last THD of the newly established frame exchange sequence
    txl_cntrl_env.txlist[access_category].last_frame_exch = last_thd;

    GLOBAL_INT_RESTORE();
}

/**
 ****************************************************************************************
 * @brief Lock the TX prepare procedure while it is being executed in background, in order
 * to avoid having it invoked from IRQ at the same time.
 ****************************************************************************************
 */
static void txl_transmit_prep(int access_category, uint8_t user_idx)
{
    bool buffer_full = false;
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];

    // Pursue transfer of pending payloads in the Tx list
    while (1)
    {
        // Pick first descriptor from the pending list
        struct txdesc *txdesc = txlist->first_to_download[user_idx];

        // Check if we have enough data in advance
        if (txl_buffer_count(access_category, user_idx) >= TX_MIN_DOWNLOAD_CNT)
            break;

        if (txdesc == NULL)
            // Exit the loop as there are no more descriptors to handle
            break;

        // Ask for the transfer of the payload(s) corresponding to the current descriptor
        buffer_full = txl_payload_transfer(txdesc, access_category, user_idx);
        if (buffer_full)
            // Exit the loop as we cannot allocate any more buffer space
            break;

        #if NX_AMSDU_TX
        if (txlist->dwnld_index[user_idx] >= txdesc->host.packet_cnt)
        #endif
        {
            // Go to the next element
            txlist->first_to_download[user_idx] = (struct txdesc *)co_list_next(&txdesc->list_hdr);
            #if NX_AMSDU_TX
            // And reset the packet index
            txlist->dwnld_index[user_idx] = 0;
            #endif
        }
    }
}


#if NX_AMPDU_TX
/**
 ****************************************************************************************
 * @brief Check if a txdesc can be aggregated in an AMPDU
 * @param[in] txdesc    Pending txdesc
 * @param[in] agg_desc  Already established params of ongoing aggregate
 *
 * @return true if MPDU can be aggregated, false otherwise
 ****************************************************************************************
 */
static bool txl_desc_is_agg(struct txdesc *txdesc, struct tx_agg_desc * agg_desc)
{
    if (is_mpdu_agg(txdesc) && !is_mpdu_first(txdesc))
    {
        //check if its (staid, tid) matches that of the started AMPDU
        if ( (agg_desc->sta_idx == txdesc->host.staid)  &&
             (agg_desc->tid     == txdesc->host.tid) )
        {
            return true;
        }
    }
    return false;
}

/**
 ****************************************************************************************
 * @brief Check if the current A-MPDU header descriptor indicates that the RTS/CTS retry
 * limit has been reached. In such case, reprogram the A-MPDU transmission with protection
 * changed to CTS-to-self to ensure that packet will go out.
 *
 * @param[in] a_thd              Pointer to the A-MPDU header descriptor
 * @param[in] access_category    Queue on which the A-MPDU is transmitted
 ****************************************************************************************
 */
__INLINE void txl_check_rtscts_retry_limit(struct tx_hd *a_thd, uint8_t access_category)
{
    uint32_t a_thd_status = a_thd->statinfo;

    // Check if A-MPDU descriptor is done
    if (a_thd_status & DESC_DONE_TX_BIT)
    {
        struct tx_policy_tbl *pol = HW2CPU(a_thd->policyentryaddr);

        // Sanity check - the only valid case coming here corresponds
        // to an RTS/CTS frame exchanged that failed
        ASSERT_REC(a_thd_status & RETRY_LIMIT_REACHED_BIT);

        // Reset the status of the A-MPDU descriptor
        a_thd->statinfo = 0;

        // Change the protection to CTS-to-self to ensure that the A-MPDU
        // will be transmitted on next attempt
        pol->ratecntrlinfo[0] &= ~PROT_FRM_EX_RCX_MASK;
        pol->ratecntrlinfo[0] |= PROT_SELF_CTS;

        // Reprogram the A-MPDU in TX (by setting the newHead bit)
        txl_cntrl_newhead(CPU2HW(a_thd), access_category);
    }
}

/**
 ****************************************************************************************
 * @brief Calculate AMPDU subframe length for an MPDU (without blank delimiters)
 * @param[in] thd   Pointer to the THD whose length +Delimiter(4B)+FCS(4)+PAD(0-3) is calculated.
 * @return AMPDU Subframe length.
 ****************************************************************************************
 */
__INLINE uint16_t txl_mpdu_subframe_len(struct tx_hd *thd)
{
    // Align packet length and add front delimiter length
    return (CO_ALIGN4_HI(thd->frmlen) + DELIMITER_LEN);
}

/**
 ****************************************************************************************
 * @brief Calculate number of blank delimiters to ensure MMSS after a txdesc
 * This number will be set in the THD of the NEXT txdesc in AMPDU
 * @param[in] txdesc   Pointer to txdesc whose length number of blank delimiters is calculated
 * @param[in] ac       Access Category
 * @param[in] agg      Pointer to the A-MPDU descriptor under builing process
 ****************************************************************************************
 */
static void txl_ampdu_constraints_get(struct txdesc *txdesc,
                                      uint8_t ac,
                                      struct txl_agg_build_tag *agg)
{
    uint32_t rate_idx_delim;
    uint32_t rate_idx_len;
    uint32_t max_len_sta;
    uint32_t max_len_phy;
    uint32_t nss;
    uint32_t mcs_idx = (txdesc->umac.phy_flags & MCS_INDEX_TX_RCX_MASK)
                                                             >> MCS_INDEX_TX_RCX_OFT;
    uint32_t bw = (txdesc->umac.phy_flags & BW_TX_RCX_MASK) >> BW_TX_RCX_OFT;
    uint32_t sgi = (txdesc->umac.phy_flags & SHORT_GI_TX_RCX_MASK) >> SHORT_GI_TX_RCX_OFT;
    struct sta_info_tag *sta_info = &sta_info_tab[txdesc->host.staid];
    uint8_t  tid = txdesc->host.tid;
    #if NX_BW_LEN_ADAPT
    int i;
    #endif

    // Sanity check: A-MPDUs cannot be sent at legacy rates
    ASSERT_ERR(((txdesc->umac.phy_flags & FORMAT_MOD_TX_RCX_MASK)
                                   >> FORMAT_MOD_TX_RCX_OFT) > FORMATMOD_NON_HT_DUP_OFDM);

    // Check if modulation is HT or VHT
    if (((txdesc->umac.phy_flags & FORMAT_MOD_TX_RCX_MASK)
                             >> FORMAT_MOD_TX_RCX_OFT) == FORMATMOD_VHT)
    {
        // Extract NSS
        nss = ((mcs_idx & VHT_NSS_MASK) >> VHT_NSS_OFT) + 1;

        // Extract MCS index
        mcs_idx = (mcs_idx & VHT_MCS_MASK) >> VHT_MCS_OFT;

        // Sanity checks: Maximum 4 SS supported, and MCS shall be between 0 and 9
        ASSERT_ERR((nss > 0) && (nss <= 4));
        ASSERT_ERR(mcs_idx <= 9);

        // Get the maximum VHT A-MPDU size supported by the receiver
        max_len_sta = sta_info->ampdu_size_max_vht;
    }
    else
    {
        // Sanity check: The current implementation only supports MCS0-31
        ASSERT_ERR(mcs_idx <= 31);

        // Extract NSS
        nss = ((mcs_idx & HT_NSS_MASK) >> HT_NSS_OFT) + 1;

        // Extract MCS index in 0-7 range
        mcs_idx = (mcs_idx & HT_MCS_MASK) >> HT_MCS_OFT;

        // Get the maximum HT A-MPDU size supported by the receiver
        max_len_sta = sta_info->ampdu_size_max_ht;
    }

    // Compute the rate indexes
    agg->bw = bw;
    // To compute the maximum length we take the minimal bandwidth, to ensure that the
    // packet won't cross the L-SIG duration limit in case the MAC HW would have to
    // reduce the transmission bandwidth
    rate_idx_len = (mcs_idx << 3) | sgi;
    // To compute the minimal A-MPDU subframe length we take the maximal bandwidth
    rate_idx_delim = rate_idx_len | (bw << 1);

    #if NX_BW_LEN_ADAPT
    agg->bw_idx = 0;

    // Compute the maximum length for each BW
    for (i = 0; i <= agg->bw; i++)
    {
        // Compute the maximum length of the A-MPDU depending on the PHY rate
        max_len_phy = ((uint32_t)TX_RATE_TO_32US_LEN[rate_idx_len | (i << 1)])
                                                     * nss * mm_env.ampdu_max_dur[ac];

        // Get the constraints of the A-MPDU based on the PHY rate that will be used
        agg->max_len[i] = co_min(max_len_phy, max_len_sta);
    }

    #else
    // Compute the maximum length of the A-MPDU depending on the PHY rate
    max_len_phy = ((uint32_t)TX_RATE_TO_32US_LEN[rate_idx_len]) * nss * mm_env.ampdu_max_dur[ac];

    // Get the constraints of the A-MPDU based on the PHY rate that will be used
    agg->max_len[0] = co_min(max_len_phy, max_len_sta);
    #endif

    // Compute the minimum number of bytes for a sub frame
    agg->mmss_bytes = ((uint16_t)TX_RATE_TO_MIN_SEP[rate_idx_delim]) * nss *
                                                   (uint16_t)sta_info->ampdu_spacing_min;

    // Get the maximum number of MPDUs inside the A-MPDU from BA agreement
    agg->max_cnt = sta_mgmt_get_tx_buff_size(txdesc->host.staid, tid);
}

/**
 ****************************************************************************************
 * @brief Calculate number of blank delimiters to ensure MMSS after a THD
 * This number will be set in the THD of the NEXT txdesc in AMPDU
 *
 * @param[in] thd   Pointer to THD whose length number of blank delimiters is calculated
 * @param[in] mmss_bytes Minimal number of bytes in the A-MPDU subframe
 *
 * @return Number of blank delimiters to ensure Min MPDU Start Spacing
 ****************************************************************************************
 */
static uint16_t txl_mpdu_nb_delims(struct tx_hd *thd, uint16_t mmss_bytes)
{
    uint16_t nb_delims = 0;
    uint16_t subfrm_len = txl_mpdu_subframe_len(thd);

    //calculate nb delimiters
    if (subfrm_len < mmss_bytes)
    {
        //keep only byte length which must be covered by blank delimiters
        mmss_bytes -= subfrm_len;

        //not a multiple of 4 bytes(blank delim length) -> add 1 after division to ensure space is reached
        nb_delims = (mmss_bytes + DELIMITER_LEN - 1) / DELIMITER_LEN;
    }
    return nb_delims;
}

/**
 ****************************************************************************************
 * @brief Fill BAR frame (THD and payload)
 * @param[in] agg_desc  Aggregate descriptor in which BAR THD and payload is found
 * @param[in] sn        Sequence number to be put in the BAR frame
 * @param[in] staid     Station ID for this AMPDU
 * @param[in] tid       TID for this AMPDU
 * @param bw        Transmission bandwidth of this AMPDU
 ****************************************************************************************
 */
static void txl_fill_bar(struct tx_agg_desc *agg_desc, uint16_t sn,
                         uint8_t staid, uint8_t tid, uint8_t bw)
{
    struct tx_hd *bar_thd = &agg_desc->bar_thd;
    struct tx_policy_tbl *bar_pol = &agg_desc->bar_pol_tbl;
    struct bar_frame *bar_payl = &agg_desc->bar_payl;
    struct sta_info_tag *sta_entry = &sta_info_tab[staid];
    struct vif_info_tag *vif_entry = &vif_info_tab[sta_entry->inst_nbr];

    //------------------------------------------------------------------------------------
    //FILL PAYLOAD
    //------------------------------------------------------------------------------------
    //RA
    bar_payl->h.addr1 = sta_entry->mac_addr;
    //TA - our own
    bar_payl->h.addr2 = vif_entry->mac_addr;

    //bar_cntl - Normal ACK, Non-Multi tID, compressed BAR
    bar_payl->bar_cntrl = BAR_CNTL_COMPRESSED_BIT|(tid << BAR_CNTL_TID_OFT);
    //BAR Info - SSC
    //smallest in A-MPDU - that of 1st MPDU in AMPDU while there are no retries
    bar_payl->bar_information = sn << 4;

    //------------------------------------------------------------------------------------
    //Format THD
    //------------------------------------------------------------------------------------
    // Reset chaining between THD
    bar_thd->nextfrmexseq_ptr = 0;

    // Set status as 0
    bar_thd->statinfo = 0;

    //------------------------------------------------------------------------------------
    //Format policy table
    //------------------------------------------------------------------------------------
    // Set TX power
    bar_pol->powercntrlinfo[0] = nxmac_ofdm_max_pwr_level_getf() << TX_PWR_LEVEL_PT_RCX_OFT;
    // Keep same bandwidth for BAR
    bar_pol->ratecntrlinfo[0] = (bw << BW_TX_RCX_OFT) |
                                (FORMATMOD_NON_HT_DUP_OFDM << FORMAT_MOD_TX_RCX_OFT) |
                                (HW_RATE_24MBPS << MCS_INDEX_TX_RCX_OFT);
}

#if RW_MUMIMO_TX_EN
/**
 ****************************************************************************************
 * @brief Compute the duration of the data part of a MU-MIMO A-MPDU.
 *
 * @param[in] length Total data length, in bytes, of the A-MPDU
 * @param[in] mcs MCS used fot the transmission
 * @param[in] sgi Flag indicating whether Short GI is used or not
 * @param[in] bw The transmission bandwidth
 * @param[in] nss The number of Spatial Streams used for the transmission
 *
 * @return The duration of the A-MPDU in ns
 ****************************************************************************************
 */
static void txl_mumimo_convert(uint8_t ac, uint8_t user_pos)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[ac];
    struct txl_mumimo_build_info_tag *mumimo = &txlist->mumimo;
    struct txdesc *txdesc = (struct txdesc *)co_list_pick(&mumimo->tx[user_pos]);
    struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;

    // We will now have some data programmed for transmission, so we can activate the A-MPDU
    // optimization
    txlist->ppdu_cnt++;

    // Chain the data THD to the primary A-MPDU THD
    if (is_mpdu_agg(txdesc))
    {
        struct txdesc *last = (struct txdesc *)co_list_pick_last(&mumimo->tx[user_pos]);
        struct tx_hd *thd = &last->lmac.hw_desc->thd;

        // Chain the BAR descriptor to the last MPDU
        thd->nextmpdudesc_ptr = CPU2HW(&agg_desc->bar_thd);

        // Put secondary aggregate in used list
        co_list_push_back(&txlist->aggregates, &agg_desc->list_hdr);
    }
    else
    {
        // Put back the aggregate in the free list
        txl_agg_desc_free(agg_desc, ac);

        // Unchain the aggregate descriptor from the TX descriptor
        txdesc->lmac.agg_desc = NULL;
    }

    // Check if we can allocate TX buffers for the MPDUs of this user
    while (txlist->first_to_download[0] == NULL)
    {
        if (!txl_payload_alloc(txdesc, ac, 0))
            break;

        txdesc = (struct txdesc *)co_list_next(&txdesc->list_hdr);
        if (txdesc == NULL)
            break;
    }
    // Concatenate the MU-MIMO TX descriptor list with the transmitting list
    co_list_concat(&txlist->transmitting[0], &mumimo->tx[user_pos]);
}

/**
 ****************************************************************************************
 * @brief Compute the duration of the data part of a MU-MIMO A-MPDU.
 *
 * @param[in] length Total data length, in bytes, of the A-MPDU
 * @param[in] mcs MCS used fot the transmission
 * @param[in] sgi Flag indicating whether Short GI is used or not
 * @param[in] bw The transmission bandwidth
 * @param[in] nss The number of Spatial Streams used for the transmission
 *
 * @return The duration of the A-MPDU in ns
 ****************************************************************************************
 */
static uint32_t txl_mumimo_duration_ns(uint32_t length, uint8_t mcs, bool sgi,
                                       uint8_t bw, uint8_t nss)
{
    // Total TXTIME
    uint32_t duration;

    // IEEE coresponding variables
    uint32_t ndbps;
    uint32_t nes;
    uint32_t bit_cnt;

    // Number of DATA BIT PER SYMBOL
    ndbps = ((uint16_t)nss) * VHT_NDBPS[bw][mcs];

    // nES = 6*NES
    // VHT IEEE P802.11ac D4.0
    // 20MHz and  40MHz Bandwidth
    if (bw < BW_80MHZ)
    {
        nes = (ndbps <= 2160) ? 6 : (ndbps <= 4320) ? 12 : 18;
    }
    // 80MHz Bandwidth
    else if (bw == BW_80MHZ)
    {
        if ((nss == 7) && (ndbps == 8190))
        {
            nes = 36;
        }
        else if((nss == 7) && (ndbps == 2457))
        {
            nes = 18;
        }
        else
        {
            nes = (ndbps <= 2106) ? 6  :
                  (ndbps <= 4212) ? 12 :
                  (ndbps <= 6318) ? 18 :
                  (ndbps <= 8424) ? 24 :
                                    36;
        }
    }
    // 160MHz Bandwidth
    else
    {
        if (((nss == 4) && (ndbps == 9360)) || ((nss == 7) && (ndbps == 9828)))
        {
            nes = 36;
        }
        else if ((ndbps == 14040) && ((nss == 5) || (nss == 6)))
        {
            nes = 48;
        }
        else if ((nss == 7) && (ndbps == 16380))
        {
            nes = 54;
        }
        else
        {
            nes = (ndbps <= 2106)  ? 6  :
                  (ndbps <= 4212)  ? 12 :
                  (ndbps <= 6318)  ? 18 :
                  (ndbps <= 8424)  ? 24 :
                  (ndbps <= 10530) ? 30 :
                  (ndbps <= 12636) ? 36 :
                  (ndbps <= 14742) ? 42 :
                  (ndbps <= 16848) ? 48 :
                  (ndbps <= 18720) ? 54 :
                                     72 ;
        }
    }

    // Data Time on Air for a MUMIMO A-MPDU
    // 1) The full data length in bit is length_i*8 + 16 bits of service field + 6*nES tail bits (IEEE Std 802.11 - 2012 Chapter 18.3.5.x)
    // 2) Convert into Nsymbols according to the MCS (round up the value)
    // 3) Convert Nsymbols into time duration according to Normal GI or Short GI (round up the value)

    // Total of Symbols = (length_i*8 + 16 + 6*NES) / NDBPS
    bit_cnt = (length << 3) + 16 + nes;
    duration =  bit_cnt / ndbps;
    duration += (duration * ndbps < bit_cnt) ? 1 : 0;

    // Compute actual duration
    duration *= sgi ? 3600 : 4000;

    // Return the value
    return (duration);
}

/**
 ****************************************************************************************
 * @brief Terminates the MU-MIMO A-MPDU under construction
 *
 * @param[in] ac  Access Category
 * @param[in] user_pos User Position in the group (only for MU-MIMO, 0 if not MU-MIMO)
 *
 * @return The duration of the A-MPDU in us
 ****************************************************************************************
 */
static void txl_mumimo_ampdu_finish(uint8_t ac, uint8_t user_pos)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[ac];
    struct txl_agg_build_tag *agg = &txlist->agg[user_pos];
    struct tx_agg_desc *agg_desc = agg->desc;
    struct txdesc *txdesc = agg->txdesc;
    uint32_t length;

    // Check if the A-MPDU has only one MPDU
    if (agg->curr_cnt == 1)
    {
        struct tx_hd *thd = &txdesc->lmac.hw_desc->thd;

        // In such case, transform it to a singleton MPDU
        txdesc->umac.flags = WHICHDESC_UNFRAGMENTED_MSDU;

        // Clear the next_mpdu field (that was pointing to the BAR THD)
        thd->nextmpdudesc_ptr = 0;
        // Update the MAC Control Information 2 field to the correct value
        thd->macctrlinfo2 = WHICHDESC_UNFRAGMENTED_MSDU | INTERRUPT_EN_TX;

        // Get the length of the packet
        length = thd->frmlen;
    }
    else
    {
        struct tx_hd *thd = &txdesc->lmac.hw_desc->thd;
        struct tx_hd *a_thd = &agg_desc->a_thd;

        // Re-mark last MPDU as LAST not MIDDLE
        set_mpdu_pos(txdesc, WHICHDESC_AMPDU_LAST);

        // And update the MAC Control Information 2 field to the correct value
        thd->macctrlinfo2 = txdesc->umac.flags | INTERRUPT_EN_TX;

        // Current aggregate is now completed
        agg_desc->status |= AGG_FORMATTED;

        // Get the length of the A-MPDU
        length = a_thd->frmlen;
    }

    // BAR DESCRIPTOR
    txl_fill_bar(agg_desc, txdesc->umac.sn_win, agg_desc->sta_idx, agg_desc->tid, agg->bw);

    // Indicate that this user position is now closed for this MU-MIMO PPDU
    txlist->mumimo.open &= ~CO_BIT(user_pos);

    // Save the A-MPDU length (used later for duration computation)
    txlist->mumimo.length[user_pos] = length;

    // Reinitialize the build state machine
    agg->desc = NULL;

}

static void txl_mumimo_close(uint8_t ac)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[ac];
    struct txl_mumimo_build_info_tag *mumimo = &txlist->mumimo;
    int i;
    uint32_t dur_max = 0;
    uint8_t prim_user_pos = 0;
    uint8_t nb_users = 0;
    bool mumimook = true;

    // Go through the A-MPDU which are ready for this MU-MIMO PPDU
    for (i = 0; i < RW_USER_MAX; i++)
    {
        if (mumimo->users & CO_BIT(i))
        {
            struct txl_agg_build_tag *agg = &txlist->agg[i];
            struct txdesc *txdesc = (struct txdesc *)co_list_pick(&mumimo->tx[i]);
            uint32_t rate_info = mumimo->rateinfo[i];
            uint32_t duration;

            // Check if we have to close the A-MPDU under formation
            if (agg->desc != NULL)
            {
                txl_mumimo_ampdu_finish(ac, i);
            }

            // Now we need to ensure that the primary user is an A-MPDU
            if (!is_mpdu_agg(txdesc))
                mumimook = false;

            // Compute the duration of the A-MPDU
            duration = txl_mumimo_duration_ns(mumimo->length[i],
                                              (rate_info & VHT_MCS_MASK) >> VHT_MCS_OFT,
                                              (rate_info & SHORT_GI_TX_RCX_MASK) != 0,
                                              (rate_info & BW_TX_RCX_MASK) >> BW_TX_RCX_OFT,
                                              ((rate_info & VHT_NSS_MASK) >> VHT_NSS_OFT) + 1);

            // Check if the duration of the present A-MPDU is the greatest one
            if (duration > dur_max)
            {
                // The present A-MPDU duration is the greatest one, so save its duration
                // as well as the user position
                dur_max = duration;
                prim_user_pos = i;
            }

            // One more user in this MU-MIMO PPDU
            nb_users++;
        }
    }

    // Check if the MU-MIMO PPDU has several users
    if ((nb_users > 1) && (mumimook))
    {
        struct txdesc *txdesc = (struct txdesc *)co_list_pick(&mumimo->tx[prim_user_pos]);

        // Now we need to ensure that the primary user is an A-MPDU
        if (is_mpdu_agg(txdesc))
        {
            struct tx_agg_desc *prim_agg_desc = txdesc->lmac.agg_desc;
            struct tx_hd *a_thd = &prim_agg_desc->a_thd;
            struct tx_hd *bar_thd = &prim_agg_desc->bar_thd;
            struct txdesc *last = (struct txdesc *)co_list_pick_last(&mumimo->tx[prim_user_pos]);
            struct tx_hd *thd = &last->lmac.hw_desc->thd;
            int sec_idx = 1;
            uint32_t *sec_user_ptr = &a_thd->sec_user1_ptr;
            struct tx_agg_desc *agg_desc = prim_agg_desc;

            // Chain the BAR descriptor to the last MPDU
            thd->nextmpdudesc_ptr = CPU2HW(&prim_agg_desc->bar_thd);

            // This descriptor will be an intermediate one
            prim_agg_desc->status |= AGG_INT;
            prim_agg_desc->download = 0;
            prim_agg_desc->prim_agg_desc = prim_agg_desc;

            // Initialize the THD fields
            a_thd->phyctrlinfo = (mumimo->group_id << GID_TX_OFT) |
                                 (prim_user_pos << USER_POS_OFT) |
                                 USE_MUMIMO_TX_BIT;

            // Put primary aggregate in used list
            co_list_push_back(&txlist->aggregates, &prim_agg_desc->list_hdr);

            // We will now have some data programmed for transmission, so we can activate the A-MPDU
            // optimization
            txlist->ppdu_cnt++;

            // Perform the chaining to the primary A-MPDU and BAR THDs
            for (i = 0; i < RW_USER_MAX; i++)
            {
                if (mumimo->users & CO_BIT(i))
                {
                    int user_q_idx;
                    txdesc = (struct txdesc *)co_list_pick(&mumimo->tx[i]);

                    if (i != prim_user_pos)
                    {
                        agg_desc = txdesc->lmac.agg_desc;

                        // Chain the data THD to the primary A-MPDU THD
                        if (is_mpdu_agg(txdesc))
                        {
                            thd = &agg_desc->a_thd;

                            // If the secondary PSDU is an A-MPDU chain the A-THD
                            *sec_user_ptr++ = CPU2HW(&agg_desc->a_thd);
                        }
                        else
                        {
                            thd = &txdesc->lmac.hw_desc->thd;

                            // Otherwise chain the Singleton THD
                            *sec_user_ptr++ = CPU2HW(&txdesc->lmac.hw_desc->thd);
                        }

                        // Chain the BAR THD to the previous BAR THD
                        bar_thd->nextfrmexseq_ptr = CPU2HW(&agg_desc->bar_thd);
                        bar_thd = &agg_desc->bar_thd;

                        // Initialize the THD fields
                        thd->phyctrlinfo = (i << USER_POS_OFT) | USE_MUMIMO_TX_BIT;

                        // The user queue index will map to the secondary index
                        user_q_idx = sec_idx++;

                        // Point the secondary AGG desc to the primary one
                        agg_desc->prim_agg_desc = prim_agg_desc;
                        agg_desc->status |= AGG_INT;

                        // Put secondary aggregate in used list
                        co_list_push_back(&txlist->aggregates, &agg_desc->list_hdr);
                    }
                    else
                    {
                        // The primary A-MPDU has the queue index 0
                        user_q_idx = 0;
                    }

                    // Indicate that some payload download are expected on this user index
                    prim_agg_desc->download |= CO_BIT(user_q_idx);

                    // Check if we can allocate TX buffers for the MPDUs of this user
                    while (txlist->first_to_download[user_q_idx] == NULL)
                    {
                        if (!txl_payload_alloc(txdesc, ac, user_q_idx))
                            break;

                        txdesc = (struct txdesc *)co_list_next(&txdesc->list_hdr);
                        if (txdesc == NULL)
                            break;
                    }
                    // Concatenate the MU-MIMO TX descriptor list with the transmitting list
                    co_list_concat(&txlist->transmitting[user_q_idx], &mumimo->tx[i]);
                }
            }
            // Reset the intermediate status of the last AGG descriptor
            agg_desc->status &= ~AGG_INT;
            // Save the last user BAR THD
            prim_agg_desc->last_bar_thd = bar_thd;
        }
        else
        {
            // The primary user is not an A-MPDU, so chain all the packets to the default
            // user queue (i.e 0)
            for (i = 0; i < RW_USER_MAX; i++)
            {
                if (mumimo->users & CO_BIT(i))
                {
                    // Convert the MU-MIMO A-MPDU or MPDU into a SU one
                    txl_mumimo_convert(ac, i);
                }
            }
        }
    }
    else
    {
        // The primary user is not an A-MPDU, so chain all the packets to the default
        // user queue (i.e 0)
        for (i = 0; i < RW_USER_MAX; i++)
        {
            if (mumimo->users & CO_BIT(i))
            {
                // Convert the MU-MIMO A-MPDU or MPDU into a SU one
                txl_mumimo_convert(ac, i);
            }
        }
    }

    // Clear the MU-MIMO preparation structure
    mumimo->nb_users = 0;
    mumimo->users = 0;
    mumimo->first_user_pos = (mumimo->first_user_pos + 1) % RW_USER_MAX;
    mumimo->open = MU_USER_MASK;

    // If IPC queues were paused and packets saved, handle them now
    for (i = 0; i < RW_USER_MAX; i++)
    {
        int pos = (mumimo->first_user_pos + i) % RW_USER_MAX;
        struct txdesc *txdesc = mumimo->txdesc[pos];

        if (txdesc != NULL)
        {
            int status;

            mumimo->txdesc[pos] = NULL;

            status = txl_aggregate(txdesc, ac);

            if (status == SU_PACKET)
            {
                // Check if we can try to allocate a Tx buffer
                if (txlist->first_to_download[0] == NULL)
                {
                    txl_payload_alloc(txdesc, ac, 0);
                }
                co_list_push_back(&txlist->transmitting[0], &txdesc->list_hdr);
            }
        }
    }

    // Reenable IPC queues
    ipc_emb_enable_users(ac, mumimo->open);
}

int txl_mumimo_check(struct txdesc * txdesc, uint8_t ac, uint8_t *user_pos)
{
    int status = SU_PACKET;
    uint8_t group_id = get_group_id(txdesc);
    struct txl_list *txlist = &txl_cntrl_env.txlist[ac];

    // Check if the frame can be MU-MIMOed
    if (is_mpdu_agg(txdesc) && is_mumimo_group_id(group_id) &&
        bfr_is_calibrated(txdesc->host.staid))
    {
        // Frame can be MU-MIMOed
        status = MU_PACKET;
        *user_pos = get_user_pos(txdesc);

        // Check if some users are already part of an ongoing construction
        if (txlist->mumimo.users)
        {
            // Check if the pushed frame is part of the same group
            if (group_id != txlist->mumimo.group_id)
            {
                // Close this user position
                if (txlist->mumimo.users & CO_BIT(*user_pos))
                    txl_mumimo_ampdu_finish(ac, *user_pos);
                else
                    txlist->mumimo.open &= ~CO_BIT(*user_pos);

                // Check if the other user positions are still open
                if (txlist->mumimo.open & txlist->mumimo.users)
                {
                    // We could still receive packets on other user positions,
                    // so save the current packet
                    ASSERT_ERR(txlist->mumimo.txdesc[*user_pos] == NULL);
                    txlist->mumimo.txdesc[*user_pos] = txdesc;
                    status = MU_PAUSED;
                }
                else
                {
                    // Close the MU-MIMO PPDU under construction
                    txl_mumimo_close(ac);

                    // Start a new MU-MIMO PPDU
                    txlist->mumimo.group_id = group_id;
                    // Remember the BW and GI of the actual transmission
                    txlist->mumimo.phy_flags = txdesc->umac.phy_flags & ~MCS_INDEX_TX_RCX_MASK;
                    txdesc->umac.phy_flags &= ~VHT_NSS_MASK;
                }
            }
            else
            {
                txdesc->umac.phy_flags &= VHT_MCS_MASK;
                txdesc->umac.phy_flags |= txlist->mumimo.phy_flags;
            }
        }
        else
        {
            // Start a new MU-MIMO PPDU
            txlist->mumimo.group_id = group_id;
            // Remember the BW and GI of the actual transmission
            txlist->mumimo.phy_flags = txdesc->umac.phy_flags & ~MCS_INDEX_TX_RCX_MASK;
            txdesc->umac.phy_flags &= ~VHT_NSS_MASK;
        }
    }
    else
    {
        // Check if a MU-MIMO PPDU is under construction
        if (txlist->mumimo.users)
        {
            *user_pos = get_user_pos(txdesc);
            // Close this user position
            if (txlist->mumimo.users & CO_BIT(*user_pos))
                txl_mumimo_ampdu_finish(ac, *user_pos);
            else
                txlist->mumimo.open &= ~CO_BIT(*user_pos);

            // Check if the other user positions are still open
            if (txlist->mumimo.open & txlist->mumimo.users)
            {
                // We could still receive packets on other user positions,
                // so save the current packet
                ASSERT_ERR(txlist->mumimo.txdesc[*user_pos] == NULL);
                txlist->mumimo.txdesc[*user_pos] = txdesc;
                status = MU_PAUSED;
            }
            else
            {
                // Close the MU-MIMO PPDU under construction
                txl_mumimo_close(ac);
                *user_pos = 0;
            }
        }
        else
        {
            *user_pos = 0;
        }
    }

    return (status);
}

/**
 ****************************************************************************************
 * @brief Unchains the frame exchanges that have been completed
 *
 * This primitive goes through the list of completed frame exchanges, check their status,
 * and put them in the correct list (cfm or waiting block-ack)
 *
 * @param[in] access_category  Access category for the frame exchanges to unchain
 * @param[in] user_idx         User index (for MU-MIMO TX only, 0 otherwise)
 *
 ****************************************************************************************
 */
static void txl_mumimo_secondary_done(uint8_t access_category, uint8_t user_idx)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];

    // Sanity check - This handler is only for secondary users
    ASSERT_ERR(user_idx != 0);

    // Walk through the list of completed SW descriptors
    while (1)
    {
        struct txdesc *txdesc;
        struct tx_hd *txhd;
        uint32_t txstatus;
        struct tx_agg_desc *agg_desc;

        //pick first txdesc in transmitting list to check its doneTx status
        txdesc = (struct txdesc *)co_list_pick(&(txlist->transmitting[user_idx]));

        // a descriptor is waiting to be confirmed
        if (txdesc == NULL)
            break;

        txhd = &txdesc->lmac.hw_desc->thd;
        txstatus = txhd->statinfo;
        agg_desc = txdesc->lmac.agg_desc;

        //---------------
        //STATUS ANALYSIS
        //---------------
        // Check the doneTx status bit
        if (txstatus & DESC_DONE_TX_BIT)
        {
            #if (RW_MUMIMO_TX_EN)
            #endif

            #if NX_AMSDU_TX
            // Free all payloads associated to this MPDU
            while (txlist->tx_index[user_idx] < txdesc->host.packet_cnt)
            {
                struct txl_buffer_tag *buf = txdesc->lmac.buffer[txlist->tx_index[user_idx]];

                // Free the buffer associated with the descriptor
                if (buf != NULL)
                {
                    if (txl_buffer_free(buf, access_category))
                        txl_transmit_prep(access_category, user_idx);

                    // Reset the buffer pointer for this index
                    txdesc->lmac.buffer[txlist->tx_index[user_idx]] = NULL;
                }
                txlist->tx_index[user_idx]++;
            };

            // Reset the TX packet index
            txlist->tx_index[user_idx] = 0;
            #else
            if (txdesc->lmac.buffer != NULL)
            {
                // Free the buffer associated with the descriptor
                if (txl_buffer_free(txdesc->lmac.buffer, access_category))
                    txl_transmit_prep(access_category, user_idx);
                txdesc->lmac.buffer = NULL;
            }
            #endif

            // Put a first status to the descriptor here, indicating it has been
            // already handled by the HW
            txdesc->lmac.hw_desc->cfm.status = txstatus;

        }
        // THD not yet transmitted, stop here and come back later
        else
        {
            #if NX_AMSDU_TX
            if (txdesc->host.packet_cnt > 1)
            {
                // Check if payload buffer descriptor has been handled by HW
                while (txlist->tx_index[user_idx] < txdesc->host.packet_cnt)
                {
                    struct txl_buffer_tag *buf = txdesc->lmac.buffer[txlist->tx_index[user_idx]];
                    struct tx_pbd *tbd;

                    // Check if the buffer is available
                    if (buf == NULL)
                        break;

                    // Get the transmit buffer descriptor linked to this payload
                    tbd = &buf->tbd;
                    if (buf->flags & BUF_SPLIT)
                        tbd = HW2CPU(tbd->next);

                    // Check if this TBD is done by the HW
                    if (!(tbd->bufctrlinfo & TBD_DONE_HW))
                        break;

                    // Free the buffer associated with the descriptor
                    if (txl_buffer_free(buf, access_category))
                        txl_transmit_prep(access_category, user_idx);

                    // Reset the buffer pointer for this index
                    txdesc->lmac.buffer[txlist->tx_index[user_idx]] = NULL;

                    txlist->tx_index[user_idx]++;
                }
            }
            #endif
            break;
        }

        // Pop picked txdesc definitively from list
        co_list_pop_front(&(txlist->transmitting[user_idx]));

        // Save the status and put the descriptor in the temporary confirmation queue
        txdesc->lmac.hw_desc->cfm.status = txstatus;
        co_list_push_back(&agg_desc->cfm, (struct co_list_hdr *)txdesc);
    }
}
#endif

/**
 ****************************************************************************************
 * @brief Terminates the non MU-MIMO A-MPDU under construction
 * This function can be called in several cases:
 * - The new TX descriptor pushed is not compliant with the current A-MPDU
 * - The length of the current A-MPDU has reached the limit
 * - The number of MPDUs in the current A-MPDU has reached the limit
 * - The TX queue will become empty and new data shall be programmed for transmission
 * @param[in] ac  Access Category
 ****************************************************************************************
 */
static void txl_aggregate_finish(uint8_t ac)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[ac];
    struct txl_agg_build_tag *agg = &txlist->agg[0];
    struct tx_agg_desc *agg_desc = agg->desc;
    struct txdesc *txdesc = agg->txdesc;

    
    // Check if the A-MPDU has only one MPDU
    if (agg->curr_cnt == 1)
    {
        struct tx_hd *thd = &txdesc->lmac.hw_desc->thd;
        struct txl_buffer_control *bufctrl = txl_buffer_control_get(txdesc);
        #if NX_AMSDU_TX
        struct txl_buffer_tag *buffer = txdesc->lmac.buffer[0];
        #else
        struct txl_buffer_tag *buffer = txdesc->lmac.buffer;
        #endif
        // In such case, transform it to a singleton MPDU
        txdesc->umac.flags = WHICHDESC_UNFRAGMENTED_MSDU;

        // Clear the next_mpdu field (that was pointing to the BAR THD)
        thd->nextmpdudesc_ptr = 0;
        // Update the MAC Control Information 2 field to the correct value
        thd->macctrlinfo2 = WHICHDESC_UNFRAGMENTED_MSDU | INTERRUPT_EN_TX;

        // If buffer is already allocated for this descriptor, update the MAC_CONTROL_INFO2 field
        if (buffer != NULL)
        {
            // Check if the MPDU is already downloaded
            if (agg_desc->status & AGG_FIRST_DOWNLOADED)
            {
                // Set phy control info
                thd->phyctrlinfo = bufctrl->phy_control_info;
                // Set policy table address
                thd->policyentryaddr = CPU2HW(&bufctrl->policy_tbl);
                // Set MAC control info 1 field
                thd->macctrlinfo1 = bufctrl->mac_control_info;

                #if NX_AMSDU_TX
                // Check if this packet is split across multiple buffers
                if (txdesc->host.packet_cnt > 1)
                {
                    struct tx_policy_tbl *pol = &bufctrl->policy_tbl;

                    // Split packet - Disable HW retransmissions
                    pol->maccntrlinfo2 &= ~(LONG_RETRY_LIMIT_MASK | SHORT_RETRY_LIMIT_MASK);
                }
                #endif

                // Chain the THD to the MAC HW
                txl_frame_exchange_chain(thd, thd, ac);
            }
        }
        // Put back the aggregate in the free list
        txl_agg_desc_free(agg_desc, ac);

        // Unchain the aggregate descriptor from the TX descriptor
        txdesc->lmac.agg_desc = NULL;
    }
    else
    {
        struct tx_hd *thd = &txdesc->lmac.hw_desc->thd;
        #if NX_AMSDU_TX
        struct txl_buffer_tag *buffer = txdesc->lmac.buffer[0];
        #else
        struct txl_buffer_tag *buffer = txdesc->lmac.buffer;
        #endif

        // Re-mark last MPDU as LAST not MIDDLE
        set_mpdu_pos(txdesc, WHICHDESC_AMPDU_LAST);

        //BAR DESCRIPTOR
        txl_fill_bar(agg_desc, txdesc->umac.sn_win, agg_desc->sta_idx, agg_desc->tid, agg->bw);

        // Chain the BAR descriptor to the last MPDU
        thd->nextmpdudesc_ptr = CPU2HW(&agg_desc->bar_thd);

        // And update the MAC Control Information 2 field to the correct value
        thd->macctrlinfo2 = txdesc->umac.flags | INTERRUPT_EN_TX;

        // Current aggregate is now completed
        agg_desc->status |= AGG_FORMATTED;

        // If buffer is already allocated for this descriptor, check if it is
        // already downloaded or not
        if (buffer != NULL)
        {
            // Check if the last MPDU is already downloaded
            if (!(agg_desc->status & AGG_ALLOC))
                // Last MPDU is downloaded
                // So update the status of the A-MPDU
                agg_desc->status |= AGG_DOWNLOADED;
        }

        // Check if aggregate descriptor is ready to be programmed
        if (agg_desc->status & AGG_DOWNLOADED)
        {
            struct tx_hd *next_prev_hd = &agg_desc->bar_thd;

            // Chain the THD to the MAC HW
            txl_frame_exchange_chain(&agg_desc->a_thd, next_prev_hd, ac);
        }

        // Put aggregate in used list
        co_list_push_back(&txl_cntrl_env.txlist[ac].aggregates, &agg_desc->list_hdr);
    }

    // We will now have some data programmed for transmission, so we can activate the A-MPDU
    // optimization
    txl_cntrl_env.txlist[ac].ppdu_cnt++;

    // Reinitialize the build state machine
    agg->desc = NULL;

    
}

/**
 ****************************************************************************************
 * @brief Try and aggregate txdesc, several checks are made, when they fail the MPDU
 * goes on to be downloaded as a normal singleton MPDU
 *
 * The idea is to check if at least two txdescs in a row
 * - can be aggregated (UMAC set the flag AND txdesc not marked as part of AMPDU)
 * - have same (staid,tid)
 * - that (staid, tid) matches and existing BA agreement
 * - the SN fits within the current bitmap window of the BA agreement
 * - not both AMPDU header descriptors for this STA have been already prepared
 *
 * Then, the next txdescs are looped over to see if they can be aggregated too,
 * if so they are marked and the AMPDU desc length is increased.
 *
 * @param[in] txdesc    Descriptor of the first packet in pending list for an AC, analysed
 *                  for aggregation possibility
 * @param[in] ac        Access category for which creation of an AMPDU is being analysed
 *
 ****************************************************************************************
 */
static int txl_aggregate(struct txdesc * txdesc, uint8_t ac)
{
    struct tx_hd *thd = &txdesc->lmac.hw_desc->thd;
    uint8_t user_pos = 0;
    int status = SU_PACKET;

    //make local copy of staid and tid of MPDU given by UMAC
    uint8_t  tid   = txdesc->host.tid;
    uint8_t  staid = txdesc->host.staid;

    do
    {
        struct txl_list *txlist = &txl_cntrl_env.txlist[ac];
        struct txl_agg_build_tag *agg;

        #if RW_MUMIMO_TX_EN
        status = txl_mumimo_check(txdesc, ac, &user_pos);

        // Check if the operation on this queue shall be paused
        if (status == MU_PAUSED)
            break;
        #endif

        // Retrieve the A-MPDU construction structure for this user position
        agg = &txlist->agg[user_pos];

        // Check if we are currently building an aggregate
        if (agg->desc == NULL)
        {
            // No build ongoing, try to start a new one
            // Make sure the MPDU can be part of an aggregate
            if (!is_mpdu_agg(txdesc))
            {
                // We will now have some data programmed for transmission, so we can
                // activate the A-MPDU optimization
                txl_cntrl_env.txlist[ac].ppdu_cnt++;
                break;
            }

            // Get a free aggregate descriptor
            agg->desc = txl_agg_desc_alloc(ac);

            // No more free aggregate descriptors, handle the MPDU as a normal one
            if (agg->desc == NULL)
            {
                #if RW_MUMIMO_TX_EN
                // Check if a MU-MIMO PPDU is under construction. If yes we need
                // to close it
                if (txlist->mumimo.users)
                {
                    txl_mumimo_close(ac);
                }
                // This packet cannot be part of a MU-MIMO PPDU as no AGG descriptor
                // is available
                status = SU_PACKET;
                #endif
                //mark it as non aggregatable for the rest of the procedure
                txdesc->umac.flags = WHICHDESC_UNFRAGMENTED_MSDU;
                // We will now have some data programmed for transmission, so we can
                // activate the A-MPDU optimization
                txl_cntrl_env.txlist[ac].ppdu_cnt++;
                break;
            }

            
            // Initialize aggregate descriptor
            agg->desc->status = 0;
            agg->desc->available_len = 0;
            agg->desc->available_cnt = 0;
            agg->desc->sta_idx = staid;
            agg->desc->tid = tid;
            agg->desc->ba_desc= NULL;
            agg->desc->user_cnt = 1;
            #if RW_MUMIMO_TX_EN
            agg->desc->download = 0;
            agg->desc->prim_agg_desc = NULL;
            agg->desc->last_bar_thd = NULL;
            co_list_init(&agg->desc->cfm);
            #endif

            // Get the constraints of the A-MPDU based on the PHY rate that will be used
            // and the receiving STA
            txl_ampdu_constraints_get(txdesc, ac, agg);

            
            
            // Initialize the current A-MPDU length
            agg->desc->a_thd.frmlen = txl_mpdu_subframe_len(thd);
            //reset A-THD status
            agg->desc->a_thd.statinfo = 0;
            agg->desc->a_thd.nextmpdudesc_ptr = CPU2HW(thd);
            agg->desc->a_thd.first_pbd_ptr = 0;
            agg->desc->a_thd.optlen[0] = 0;
            agg->desc->a_thd.optlen[1] = 0;
            agg->desc->a_thd.optlen[2] = 0;

            // Initialize the current A-MPDU count
            agg->curr_cnt = 1;

            // Initialize the number of delimiters to be put in the next HW descriptor
            agg->nb_delims = txl_mpdu_nb_delims(thd, agg->mmss_bytes);

            // Save the pointer to the last handled TX descriptor
            agg->txdesc = txdesc;

            //mark the txdesc as first of AMPDU
            set_mpdu_pos(txdesc, WHICHDESC_AMPDU_FIRST);

            //link txdesc to agg_cntrl
            txdesc->lmac.agg_desc = agg->desc;
            txdesc->umac.flags |= UNDER_BA_SETUP_BIT;

            //set WhichDesc field in the THD
            thd->macctrlinfo2 = txdesc->umac.flags;

            #if RW_MUMIMO_TX_EN
            if (status == MU_PACKET)
            {
                co_list_push_back(&txlist->mumimo.tx[user_pos], &txdesc->list_hdr);
                // Remember that this user position has an A-MPDU under creation on it
                txlist->mumimo.users |= CO_BIT(user_pos);
                txlist->mumimo.rateinfo[user_pos] = txdesc->umac.phy_flags;
                agg->desc->status |= AGG_MU;
            }
            #endif
            break;
        }
        else
        {
            struct tx_agg_desc *agg_desc = agg->desc;
            struct tx_hd *a_thd = &agg_desc->a_thd;
            struct tx_hd *prev_thd = &agg->txdesc->lmac.hw_desc->thd;
            uint16_t ampdu_subfrmlen;
            #if NX_BW_LEN_ADAPT
            uint8_t bw_idx = agg->bw_idx;
            #else
            uint8_t bw_idx = 0;
            #endif

            // txdesc must be compatible with current AMPDU
            if (!txl_desc_is_agg(txdesc, agg_desc))
            {
                #if RW_MUMIMO_TX_EN
                // Close this user position
                if (agg_desc->status & AGG_MU)
                {
                    MU_AMPDU_CLOSE();
                }
                else
                {
                #endif
                    // Close the A-MPDU under construction
                    txl_aggregate_finish(ac);

                    // And try to start a new one
                    continue;
                #if RW_MUMIMO_TX_EN
                }
                #endif
            }

            // Compute the MPDU subframe length (including delimiters)
            ampdu_subfrmlen = txl_mpdu_subframe_len(thd) + agg->nb_delims * DELIMITER_LEN;

            //check for AMPDU max len
            if (((uint32_t)ampdu_subfrmlen + a_thd->frmlen)
                                         > agg->max_len[bw_idx])
            {
                #if NX_BW_LEN_ADAPT
                if (bw_idx < agg->bw)
                {
                    // Check if we have at least 2 MPDUs inside the A-MPDU to ensure that
                    // in case of BW drop at lowest BW the transmitted packet will still
                    // be an A-MPDU
                    if (agg->curr_cnt == 1)
                    {
                        #if RW_MUMIMO_TX_EN
                        // Close this user position
                        if (agg_desc->status & AGG_MU)
                        {
                            MU_AMPDU_CLOSE();
                        }
                        else
                        {
                        #endif
                            // Close the A-MPDU under construction
                            txl_aggregate_finish(ac);
                            // And try to start a new one
                            continue;
                        #if RW_MUMIMO_TX_EN
                        }
                        #endif
                    }

                    // Save the last TX descriptor of this BW step
                    agg_desc->txdesc[bw_idx] = agg->txdesc;

                    // Update the THD with the optional length of this BW step
                    a_thd->optlen[bw_idx] = a_thd->frmlen;

                    // Increase the current BW index
                    agg->bw_idx++;
                }
                else
                #endif
                {
                    // Maximum length reached, finish current A-MPDU
                    #if RW_MUMIMO_TX_EN
                    // Close this user position
                    if (agg_desc->status & AGG_MU)
                    {
                        MU_AMPDU_CLOSE();
                    }
                    else
                    {
                    #endif
                        // Close the A-MPDU under construction
                        txl_aggregate_finish(ac);
                        // And try to start a new one
                        continue;
                    #if RW_MUMIMO_TX_EN
                    }
                    #endif
                }
            }

            
            //link txdesc to agg_cntrl
            txdesc->lmac.agg_desc = agg_desc;

            //set number of delims for previous txdesc into this one
            txdesc->umac.flags |= (agg->nb_delims << NB_BLANK_DELIM_OFT) | UNDER_BA_SETUP_BIT;

            //mark the txdesc as part of AMPDU
            set_mpdu_pos(txdesc, WHICHDESC_AMPDU_INT);

            //set WhichDesc and delimiter fields in the THD
            thd->macctrlinfo2 = txdesc->umac.flags;

            //chain the new THD to the previous one
            prev_thd->nextmpdudesc_ptr = CPU2HW(thd);

            //increase ampdu_desc length value
            a_thd->frmlen += ampdu_subfrmlen;

            // Compute the number of delimiters to be put in the next HW descriptor
            agg->nb_delims = txl_mpdu_nb_delims(thd, agg->mmss_bytes);

            // Save the pointer to the last handled TX descriptor
            agg->txdesc = txdesc;

            #if RW_MUMIMO_TX_EN
            if (status == MU_PACKET)
            {
                co_list_push_back(&txlist->mumimo.tx[user_pos], &txdesc->list_hdr);
            }
            #endif

            //increase nb MPDUs in AMPDU counter
            agg->curr_cnt++;

            // Check if we have reached the maximum number of packets
            if (agg->curr_cnt >= agg->max_cnt)
            {
                // We have reached the maximum MPDU count, finish current A-MPDU
                #if RW_MUMIMO_TX_EN
                if (status == MU_PACKET)
                {
                    txl_mumimo_ampdu_finish(ac, user_pos);
                    // Check if other user positions are still open
                    if (txlist->mumimo.open & txlist->mumimo.users)
                    {
                        // We could still receive packets on other user positions,
                        // so pause this user queue
                        status = MU_PAUSED;
                    }
                    else
                    {
                        // Close the MU-MIMO PPDU under construction
                        txl_mumimo_close(ac);
                    }
                }
                else
                #endif
                    txl_aggregate_finish(ac);
            }

            // Exit the loop
            break;
        }
    } while (1);

    return (status);
}


#if (NX_BW_LEN_ADAPT)
/**
 ****************************************************************************************
 * @brief Unchains the frame exchanges that have been completed
 *
 * This primitive goes through the list of completed frame exchanges, check their status,
 * and put them in the correct list (cfm or waiting block-ack)
 *
 * @param[in]  access_category  Access category for the frame exchanges to unchain
 *
 ****************************************************************************************
 */
static void txl_bw_drop_handle(uint8_t access_category)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    struct txdesc * txdesc = (struct txdesc *)co_list_pick(&(txlist->transmitting[0]));
    struct tx_agg_desc *agg_desc;
    struct tx_hd *thd, *a_thd, *prev_thd, *next_thd;
    uint8_t bw;

    // Sanity check - we shall have a descriptor in the list
    ASSERT_ERR(txdesc != NULL);

    // Check if the HW triggered the mechanism for a singleton - maybe not correct
    while (!is_mpdu_agg(txdesc))
    {
        struct tx_hd *txhd = &txdesc->lmac.hw_desc->thd;
        uint32_t txstatus = txhd->statinfo;
        if (!(txstatus & DESC_DONE_TX_BIT))
            return;
        txdesc = (struct txdesc *)co_list_next(&txdesc->list_hdr);

        if (txdesc == NULL)
            return;
    }

    // Sanity check - the mechanism works only for A-MPDUs
    ASSERT_ERR(is_mpdu_first(txdesc));

    // Get the A-MPDU descriptor pointer
    agg_desc = txdesc->lmac.agg_desc;
    a_thd = &agg_desc->a_thd;

    // Get the actual BW of transmission and the last TX desc that will be transmitted
    bw = nxmac_tx_bw_after_drop_getf();
    txdesc = agg_desc->txdesc[bw];
    thd = &txdesc->lmac.hw_desc->thd;

    ASSERT_ERR(!is_mpdu_last(txdesc));

    // Split the A-MPDU
    // Re-mark last MPDU as LAST not MIDDLE
    set_mpdu_pos(txdesc, WHICHDESC_AMPDU_LAST);

    // Chain the BAR descriptor to the last MPDU
    thd->nextmpdudesc_ptr = CPU2HW(&agg_desc->bar_thd);

    // And update the MAC Control Information 2 field to the correct value
    thd->macctrlinfo2 = txdesc->umac.flags | INTERRUPT_EN_TX;

    prev_thd = &agg_desc->bar_thd;
    next_thd = HW2CPU(agg_desc->bar_thd.nextfrmexseq_ptr);

    // Now reform an A-MPDU with the following descriptors
    txdesc = (struct txdesc *)co_list_next(&txdesc->list_hdr);
    ASSERT_ERR(txdesc != NULL);
    while (1)
    {
        struct txl_buffer_control *bufctrl = txl_buffer_control_get(txdesc);
        #if NX_AMSDU_TX
        struct txl_buffer_tag *buffer = txdesc->lmac.buffer[0];
        #else
        struct txl_buffer_tag *buffer = txdesc->lmac.buffer;
        #endif
        thd = &txdesc->lmac.hw_desc->thd;
        bool last = false;

        if (is_mpdu_last(txdesc))
            last = true;

        // Increase the number of PPDU we have pushed into the data path
        txlist->ppdu_cnt++;

        // In such case, transform it to a singleton MPDU
        txdesc->umac.flags = WHICHDESC_UNFRAGMENTED_MSDU;

        // Unchain the aggregate descriptor from the TX descriptor
        txdesc->lmac.agg_desc = NULL;

        // Clear the next_mpdu field
        thd->nextmpdudesc_ptr = 0;

        // Update the MAC Control Information 2 field to the correct value
        thd->macctrlinfo2 = WHICHDESC_UNFRAGMENTED_MSDU | INTERRUPT_EN_TX;

        if (buffer)
        {
            // Set MAC control info 1 field
            thd->macctrlinfo1 = a_thd->macctrlinfo1;
            // Set phy control info
            thd->phyctrlinfo = a_thd->phyctrlinfo;
            // Set policy table address
            thd->policyentryaddr = CPU2HW(&bufctrl->policy_tbl);
            #if NX_AMSDU_TX
            // Check if this packet is split across several buffers
            if (txdesc->host.packet_cnt > 1)
            {
                struct tx_policy_tbl *pol = &bufctrl->policy_tbl;

                // Split packet - Disable HW retransmissions
                pol->maccntrlinfo2 &= ~(LONG_RETRY_LIMIT_MASK | SHORT_RETRY_LIMIT_MASK);
            }
            #endif
            prev_thd->nextfrmexseq_ptr = CPU2HW(thd);
            if (txl_cntrl_env.txlist[access_category].last_frame_exch == prev_thd)
                txl_cntrl_env.txlist[access_category].last_frame_exch = thd;

            prev_thd = thd;
        }

        if (last)
        {
            thd->nextfrmexseq_ptr = CPU2HW(next_thd);
            break;
        }

        txdesc = (struct txdesc *)co_list_next(&txdesc->list_hdr);
        ASSERT_ERR(txdesc != NULL);
    }
}
#endif
#endif
/**
 ****************************************************************************************
 * @brief Unchains the frame exchanges that have been completed
 *
 * This primitive goes through the list of completed frame exchanges, check their status,
 * and put them in the correct list (cfm or waiting block-ack)
 *
 * @param[in]  access_category  Access category for the frame exchanges to unchain
 *
 ****************************************************************************************
 */
uint32_t txl_check_hd_is_current(uint32_t ac, struct txdesc *txd)
{
	struct tx_hd *current_hd_ptr = NULL;

	switch(ac)
	{
		case AC_BK:
			current_hd_ptr = (struct tx_hd *)nxmac_debug_ac_0s_ptr_get();
			break;
			
		case AC_BE:
			current_hd_ptr = (struct tx_hd *)nxmac_debug_ac_1s_ptr_get();
			break;
			
		case AC_VI:
			current_hd_ptr = (struct tx_hd *)nxmac_debug_ac_2s_ptr_get();
			break;
			
		case AC_VO:
			current_hd_ptr = (struct tx_hd *)nxmac_debug_ac_3s_ptr_get();
			break;
			
		case 4:
			current_hd_ptr = (struct tx_hd *)nxmac_debug_bcn_s_ptr_get();
			break;
			
		default:
			break;
	}

	return (&txd->lmac.hw_desc->thd == current_hd_ptr);
}

static void txl_frame_exchange_done(uint8_t access_category)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];

    // Walk through the list of completed SW descriptors
    while (1)
    {
        struct txdesc * txdesc = NULL;

        //-----------------------
        //MPDU THD status check
        //-----------------------
        #if NX_AMPDU_TX
        if (txlist->chk_state == THD_CHK_STATE)
        {
        #endif
            //pick first txdesc in transmitting list to check its doneTx status
            txdesc = (struct txdesc *)co_list_pick(&(txlist->transmitting[0]));

            // a descriptor is waiting to be confirmed
            if (txdesc != NULL)
            {
                struct tx_hd *txhd = &txdesc->lmac.hw_desc->thd;
                uint32_t txstatus = txhd->statinfo;

                //---------------
                //STATUS ANALYSIS
                //---------------
                // Check the doneTx status bit
                if (txstatus & DESC_DONE_TX_BIT)
                {
                    #if NX_TX_FRAME
                    #if NX_AMSDU_TX
                    uint32_t packet_addr = txdesc->host.packet_addr[0];
                    #else
                    uint32_t packet_addr = txdesc->host.packet_addr;
                    #endif
                    #endif                    
                    
                    #if NX_TX_FRAME
                    // Check if the frame was generated internally
                    if (packet_addr != 0)
                    {
                    #endif
					
                        #if NX_AMSDU_TX
                        // Free all payloads associated to this MPDU
                        while (txlist->tx_index[0] < txdesc->host.packet_cnt)
                        {
                            struct txl_buffer_tag *buf = txdesc->lmac.buffer[txlist->tx_index[0]];

                            // Free the buffer associated with the descriptor
                            if (buf != NULL)
                            {
                                if (txl_buffer_free(buf, access_category))
                                    txl_transmit_prep(access_category, 0);

                                // Reset the buffer pointer for this index
                                txdesc->lmac.buffer[txlist->tx_index[0]] = NULL;
                            }
                            txlist->tx_index[0]++;
                        };
                        #else
                        if (txdesc->lmac.buffer != NULL)
                        {
                            // Free the buffer associated with the descriptor
                            if (txl_buffer_free(txdesc->lmac.buffer, access_category))
                                txl_transmit_prep(access_category, 0);
                            txdesc->lmac.buffer = NULL;
                        }
                        #endif
						
                    #if NX_TX_FRAME
                    }
                    #endif

                    // Put a first status to the descriptor here, indicating it has been
                    // already handled by the HW
                    txdesc->lmac.hw_desc->cfm.status = txstatus;

                    #if NX_AMSDU_TX
                    // Reset the TX packet index
                    txlist->tx_index[0] = 0;
                    #endif

                    #if NX_AMPDU_TX
                    // Check if we have to free the previous A-MPDU descriptor
                    if (txlist->agg_desc_prev != NULL)
                    {
                        struct tx_agg_desc *agg_desc_prev = txlist->agg_desc_prev;

                        // Decrease the user count
                        agg_desc_prev->user_cnt--;

                        // Free the descriptor
                        if (agg_desc_prev->user_cnt == 0)
                        {
                            txl_agg_desc_free(agg_desc_prev, access_category);
                        }

                        // No more previous A-MPDU descriptor to be considered for now
                        txlist->agg_desc_prev = NULL;
                    }

                    if (!is_mpdu_agg(txdesc))
                    #endif
                    {
                        //get the next frame exchange pointer
                        struct tx_hd *nextdesc = HW2CPU(txhd->nextfrmexseq_ptr);

                        // Check if next descriptor is ongoing or not
                        if (nextdesc != NULL)
                        {
                            #if NX_AMSDU_TX
                            struct tx_pbd *next_pbd;
                            #endif
                            #if NX_AMPDU_TX
                            struct tx_hd *a_thd = NULL;
                            // If it's an AMPDU THD, get its first MPDU, only it will have
                            // its doneTx status updated by HW
                            if ((nextdesc->macctrlinfo2 & WHICHDESC_MSK) == WHICHDESC_AMPDU_EXTRA)
                            {
                                // Save the pointer to the A-MPDU header descriptor
                                a_thd = nextdesc;

                                nextdesc = HW2CPU(nextdesc->nextmpdudesc_ptr);

                                // Sanity check: The A-THD and first MPDU THD are linked together
                                // prior to A-MPDU chaining to HW, so nextdesc cannot be NULL
                                ASSERT_ERR(nextdesc != NULL);
                            }
                            #endif

                            // Next descriptor is ongoing, don't release current descriptor
                            #if NX_AMSDU_TX
                            next_pbd = HW2CPU(nextdesc->first_pbd_ptr);
                            if (!(next_pbd->bufctrlinfo & TBD_DONE_HW) &&
                                !(nextdesc->statinfo & DESC_DONE_TX_BIT))
                            #else
                            if (!(nextdesc->statinfo & DESC_DONE_TX_BIT))
                            #endif
                            {
                                #if NX_AMPDU_TX
                                if (a_thd != NULL)
                                {
                                    // Check if the current A-MPDU has reached the RTS/CTS retry limit
                                    txl_check_rtscts_retry_limit(a_thd, access_category);
                                }
                                #endif
                                
                                // Move the TX activity timeout
                                txl_timer_move(access_category);
                                break;
                            }
                        }
                        else
                        {
                            uint32_t timer_msk = nxmac_timers_int_un_mask_get();
                            uint32_t timer_bit = CO_BIT(TX_AC2TIMER(access_category));

                            txlist->last_frame_exch = NULL;
                            nxmac_timers_int_un_mask_set(timer_msk & ~timer_bit);

                            #if NX_AMPDU_TX
                            if (txlist->ppdu_cnt == 1)
                            {
                                #if RW_MUMIMO_TX_EN
                                // Data path will become empty, so finish building the pending A-MPDU
                                if (txlist->mumimo.users)
                                {
                                    txl_mumimo_close(access_category);
                                }
                                else
                                #endif
                                if (txlist->agg[0].desc != NULL)
                                {
                                    // Close the pending aggregate
                                    txl_aggregate_finish(access_category);
                                }
                            }
                            #endif
                        }
                    }

                    
                }
                // THD not yet transmitted, stop here and come back later
                else
                {
                    #if NX_AMSDU_TX
                    if (txdesc->host.packet_cnt > 1)
                    {
                        // Check if payload buffer descriptor has been handled by HW
                        while (txlist->tx_index[0] < txdesc->host.packet_cnt)
                        {
                            struct txl_buffer_tag *buf = txdesc->lmac.buffer[txlist->tx_index[0]];
                            struct tx_pbd *tbd;

                            // Check if the buffer is available
                            if (buf == NULL)
                                break;

                            // Get the transmit buffer descriptor linked to this payload
                            tbd = &buf->tbd;
                            if (buf->flags & BUF_SPLIT)
                                tbd = HW2CPU(tbd->next);

                            // Check if this TBD is done by the HW
                            if (!(tbd->bufctrlinfo & TBD_DONE_HW))
                                break;

                            // Free the buffer associated with the descriptor
                            if (txl_buffer_free(buf, access_category))
                                txl_transmit_prep(access_category, 0);

                            // Reset the buffer pointer for this index
                            txdesc->lmac.buffer[txlist->tx_index[0]] = NULL;

                            txlist->tx_index[0]++;
                        }
                    }
                    #endif
                    
                    #if NX_AMPDU_TX
                    // Check if the MPDU is the first of the A-MPDU
                    if (is_mpdu_first(txdesc))
                    {
                        // Check if the current A-MPDU has reached the RTS/CTS retry limit
                        txl_check_rtscts_retry_limit(&txdesc->lmac.agg_desc->a_thd,
                                                     access_category);
                    }
                    #endif
                    
                    break;
                }

                //------------
                //CONFIRMATION
                //------------
                #if NX_AMPDU_TX
                //at last MPDU in AMPDU, we stop pushing CFMs until BAR status done;keep last mpdu pointer
                if (is_mpdu_last(txdesc))
                {
                    txlist->agg_desc = txdesc->lmac.agg_desc;
                    txlist->chk_state = ATHD_CHK_STATE;
                }
                #endif

                //pop picked txdesc definitively from list
                co_list_pop_front(&(txlist->transmitting[0]));

                #if NX_AMPDU_TX
                if (!is_mpdu_agg(txdesc))
                {
                    // We have one PPDU less in the TX path
                    txlist->ppdu_cnt--;
                }
                #endif

                #if NX_TX_FRAME
                // Check if the frame was generated internally
                #if NX_AMSDU_TX
                if (txdesc->host.packet_addr[0] == 0)
                #else
                if (txdesc->host.packet_addr == 0)
                #endif
                {
                    // Put the descriptor in the CFM queue
                    txl_frame_cfm(txdesc);
                }
                else
                #endif
                {
                    // Put the descriptor in the CFM queue
                    txl_cfm_push(txdesc, txstatus, access_category);
                }
                
                // Move the TX activity timeout
                txl_timer_move(access_category);
            //no more descriptors in transmitting
            }
            else
            {
                uint32_t timer_msk = nxmac_timers_int_un_mask_get();
                uint32_t timer_bit = CO_BIT(TX_AC2TIMER(access_category));

                // Last frame exchange, we reset the last_frame_exch flag
                txlist->last_frame_exch = NULL;
                // And we disable the TX timeout interrupt
                nxmac_timers_int_un_mask_set(timer_msk & ~timer_bit);
                // No more descriptors, exit the loop
                break;
            }

        #if NX_AMPDU_TX
        //-----------------------
        //BAR THD status check
        //-----------------------
        }
        else
        {
            //get the last txdesc queued into cfm list for this ac, its agg_desc and then bar_desc
            struct tx_agg_desc *agg_desc = txlist->agg_desc;
            struct tx_hd *bar_thd = &agg_desc->bar_thd;
            uint32_t bar_thd_status = bar_thd->statinfo;

            #if RW_MUMIMO_TX_EN
            if (!(agg_desc->status & AGG_INT))
            #endif
            {
                // get the next frame exchange pointer
                struct tx_hd *nextdesc = HW2CPU(bar_thd->nextfrmexseq_ptr);

                // Check if the current A-MPDU is the last one
                if ((nextdesc == NULL) && (txlist->ppdu_cnt == 1))
                {
                    #if RW_MUMIMO_TX_EN
                    // Data path will become empty, so finish building the pending A-MPDU
                    if (txlist->mumimo.users)
                    {
                        txl_mumimo_close(access_category);
                    }
                    else
                    #endif
                    if (txlist->agg[0].desc != NULL)
                    {
                        // Close the pending aggregate
                        txl_aggregate_finish(access_category);
                    }
                }
            }

            //BAR status is updated received
            if (bar_thd_status & DESC_DONE_TX_BIT)
            {
                
                // Move the TX activity timeout
                txl_timer_move(access_category);

                if (bar_thd_status & FRAME_SUCCESSFUL_TX_BIT)
                {
                    // Check if BA has already been received
                    if (!(agg_desc->status & AGG_BA))
                    {
                        int i = 0;
                        // Make a call to the RX interrupt handler as the BA is
                        // available, waiting for the RX timer to expire
                        do
                        {
                            rxl_timer_int_handler();
                            i++;
                        } while ((i < 5) && (agg_desc->ba_desc == NULL));
                    }

                    // Sanity check - The BA shall now be available
                    ASSERT_REC(agg_desc->ba_desc != NULL);
                }

                // Mark aggregate as ready for confirmation
                agg_desc->status |= AGG_DONE;

                // Set an event to handle the confirmation of the queued aggregated MPDU in background
                ke_evt_set(txl_cfm_evt_bit[access_category]);

                #if RW_MUMIMO_TX_EN
                // Check if the A-MPDU was transmitted as a secondary user in a MU-MIMO PPDU
                if (agg_desc->prim_agg_desc && (agg_desc->prim_agg_desc != agg_desc))
                {
                    ASSERT_ERR(!co_list_is_empty(&agg_desc->cfm));
                    // The TX descriptors of this A-MPDU are not in the CFM queue yet,
                    // we need to move them
                    co_list_concat(&txl_cfm_env.cfmlist[access_category], &agg_desc->cfm);
                }

                // Check if this AGG descriptor is an intermediate one
                if (agg_desc->status & AGG_INT)
                {
                    // This AGG descriptor is an intermediate one, i.e it means that we
                    // are waiting for the BA or at least one of the secondary users of
                    // this transmission. We stay in ATHD_CHK_STATE and we just move the
                    // pointer to the AGG descriptor for which the next BA is expected
                    txlist->agg_desc = (struct tx_agg_desc *)co_list_next(&agg_desc->list_hdr);

                    // Sanity check - We shall have a valid pointer
                    ASSERT_ERR(txlist->agg_desc);
                }
                else
                #endif
                {
                    // We have one PPDU less in the TX path
                    txlist->ppdu_cnt--;

                    //bar has been handled, BA reception status will be known at cfm,
                    //change state back to MPDUs check
                    txlist->chk_state = THD_CHK_STATE;
                    txlist->agg_desc = NULL;

                    // Check if we have another PPDU chained to this one
                    if (bar_thd->nextfrmexseq_ptr != 0)
                    {
                        // A PPDU is chained, therefore we have to wait for its transmission
                        // before freeing the current A-MPDU descriptor
                        txlist->agg_desc_prev = agg_desc;

                        // Increase the user count of the A-MPDU descriptor
                        agg_desc->user_cnt++;
                    }
                    else
                    {
                        // Last frame exchange, we reset the last_frame_exch flag
                        txlist->last_frame_exch = NULL;
                    }
                }

                
            }
            //BAR status not updated yet, exit and change nothing
            else
            {
                break;
            }
        }
        #endif
    }
}


/**
 ****************************************************************************************
 * @brief Manage atomic frame exchange formatting and chaining of previous one to it
 *
 * @param[in]       txdesc  First Txdesc of the next frame exchange
 * @param[in]       buffer  Pointer to the buffer that was just downloaded
 * @param[in]       access_category  Access category for the frame exchanges to format
 *
 ****************************************************************************************
 */
static void txl_frame_exchange_manage(struct txdesc *txdesc,
                                      struct txl_buffer_tag *buffer,
                                      uint8_t  access_category)
{
    struct tx_hd *new_hd = NULL;
    struct tx_hd *next_prev_hd = NULL;

    do
    {
        #if NX_AMPDU_TX
        #if RW_MUMIMO_TX_EN
        if (is_in_mumimo_ppdu(txdesc))
        {
            struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;
            struct tx_agg_desc *prim_agg_desc = agg_desc->prim_agg_desc;

            // Check if enough data is allocated
            if (!(buffer->flags & BUF_ALLOC_OK))
                break;

            // Clear the corresponding bit in the primary aggregate descriptor
            prim_agg_desc->download &= ~CO_BIT(buffer->user_idx);

            // Check if enough data has been allocated for all users in this MU-MIMO PPDU
            if (prim_agg_desc->download)
                break;

            // A new frame exchange starts
            new_hd = &prim_agg_desc->a_thd;

            next_prev_hd = prim_agg_desc->last_bar_thd;
        }
        else
        #endif
        // extra THDs chaining
        if (is_mpdu_agg(txdesc))
        {
            struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;

            // Check if enough data is allocated
            if (!(buffer->flags & BUF_ALLOC_OK))
                break;

            // A-MPDU has now enough data downloaded to be chained
            agg_desc->status |= AGG_DOWNLOADED;

            // check if A-MPDU is already formatted
            if (!(agg_desc->status & AGG_FORMATTED))
                // A-MPDU is not yet formatted, therefore it cannot be chained
                // Once formatted, the chaining will be done in txl_aggregate_finish()
                break;

            //a new frame exchange starts
            new_hd = &agg_desc->a_thd;

            next_prev_hd = &agg_desc->bar_thd;
        }
        else
        #endif
        {
            //it will start an atomic frame exchange by itself
            new_hd = &txdesc->lmac.hw_desc->thd;
            next_prev_hd = new_hd;
        }

        // Chain the THD to the MAC HW
        txl_frame_exchange_chain(new_hd, next_prev_hd, access_category);
    } while(0);
}

static bool txl_cntrl_start_pm_mon(struct mac_hdr *p_mac_hdr)
{
    do
    {
        uint16_t type_subtype = p_mac_hdr->fctl & MAC_FCTRL_TYPESUBTYPE_MASK;

        // Check if the transmitted frame is an association or reassociation response
        if ((type_subtype != MAC_FCTRL_ASSOCRSP) && (type_subtype != MAC_FCTRL_REASSOCRSP))
            break;

        // Check if its status is successful or not
        if (co_read16p(CPU2HW(p_mac_hdr) + MAC_SHORT_MAC_HDR_LEN + MAC_ASSO_RSP_STATUS_OFT) != 0)
            break;

        return (true);

    } while(0);

    return (false);
}

void txl_cntrl_init(void)
{
    int i, j;

	intc_service_register(FIQ_MAC_PROT_TRIGGER, PRI_FIQ_MAC_PROT_TRIGGER, txl_transmit_trigger); 
	intc_service_register(FIQ_MAC_TX_TRIGGER, PRI_FIQ_MAC_TX_TRIGGER, txl_transmit_trigger); 

    txl_hwdesc_init();
    tx_txdesc_init();
    txl_buffer_init();
    txl_cfm_init();
    
    #if NX_TX_FRAME
    txl_frame_init(false);
    #endif

    memset(&txl_cntrl_env, 0, sizeof(txl_cntrl_env));

    // Initialize TX Lists
    for (i=0; i<NX_TXQ_CNT; i++)
    {
        for (j=0; j<RW_USER_MAX; j++)
        {
            co_list_init(&(txl_cntrl_env.txlist[i].transmitting[j]));
        }

        #if RW_MUMIMO_TX_EN
        txl_cntrl_env.txlist[i].mumimo.open = MU_USER_MASK;
        #endif
        txl_cntrl_env.txlist[i].last_frame_exch = NULL;
        txl_cntrl_env.txlist[i].bridgedmacnt = 0;
        txl_cntrl_env.txlist[i].chk_state = THD_CHK_STATE;

        #if NX_AMPDU_TX
        txl_cntrl_env.txlist[i].ppdu_cnt = 0;
        txl_cntrl_env.txlist[i].agg_desc = NULL;
        txl_cntrl_env.txlist[i].agg_desc_prev = NULL;
        co_list_init(&(txl_cntrl_env.txlist[i].aggregates));
        #endif
    }

    // Initialize non-QoS sequence number
    txl_cntrl_env.seqnbr = 0;
}

#if (NX_TX_FRAME)
bool txl_cntrl_tx_check(struct vif_info_tag *p_vif_entry)
{
    // Do not accept frames for tx during reset procedure
    if (txl_cntrl_env.reset)
    {
        return (false);
    }

    #if (NX_CHNL_CTXT)
    if (!chan_is_tx_allowed(p_vif_entry))
    {
        return (false);
    }
    #endif //(NX_CHNL_CTXT)

    #if (NX_P2P)
    // If P2P interface, check if GO is present
    if (p_vif_entry->p2p && !p2p_is_present(p_vif_entry->p2p_index))
    {
        return (false);
    }
    #endif //(NX_P2P)

    return (true);
}
#endif //(NX_CHNL_CTXT || NX_P2P)

#if NX_AMPDU_TX
void txl_aggregate_check(uint8_t access_category)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    GLOBAL_INT_DECLARATION();
    
    GLOBAL_INT_DISABLE();
    if (txlist->ppdu_cnt == 0)
    {
        #if RW_MUMIMO_TX_EN
        // Check if a MU-MIMO PPDU is under preparation
        if (txlist->mumimo.users)
        {
            txl_mumimo_close(access_category);
        }
        else
        #endif
        if (txlist->agg[0].desc != NULL)
        {
            // Close the pending aggregate
            txl_aggregate_finish(access_category);
        }
    }
    GLOBAL_INT_RESTORE();
}
#endif

void txl_cntrl_halt_ac(uint8_t access_category)
{
    // Set the header pointer and the new head bit according to the access category
    switch (access_category)
    {
        #if NX_BEACONING
        case AC_BCN:
            nxmac_dma_cntrl_set(NXMAC_HALT_BCN_AFTER_TXOP_BIT);
            while(nxmac_tx_bcn_state_getf() != 0);
            nxmac_dma_cntrl_clear(NXMAC_HALT_BCN_AFTER_TXOP_BIT);
            break;
        #endif
        case AC_VO:
            nxmac_dma_cntrl_set(NXMAC_HALT_AC_3_AFTER_TXOP_BIT);
            while(nxmac_tx_ac_3_state_getf() != 0);
            nxmac_dma_cntrl_clear(NXMAC_HALT_AC_3_AFTER_TXOP_BIT);
            break;
        case AC_VI:
            nxmac_dma_cntrl_set(NXMAC_HALT_AC_2_AFTER_TXOP_BIT);
            while(nxmac_tx_ac_2_state_getf() != 0);
            nxmac_dma_cntrl_clear(NXMAC_HALT_AC_2_AFTER_TXOP_BIT);
            break;
        case AC_BE:
            nxmac_dma_cntrl_set(NXMAC_HALT_AC_1_AFTER_TXOP_BIT);
            while(nxmac_tx_ac_1_state_getf() != 0);
            nxmac_dma_cntrl_clear(NXMAC_HALT_AC_1_AFTER_TXOP_BIT);
            break;
        case AC_BK:
            nxmac_dma_cntrl_set(NXMAC_HALT_AC_0_AFTER_TXOP_BIT);
            while(nxmac_tx_ac_0_state_getf() != 0);
            nxmac_dma_cntrl_clear(NXMAC_HALT_AC_0_AFTER_TXOP_BIT);
            break;
        default:
            break;
    }
}

void txl_cntrl_flush_ac(uint8_t access_category, uint32_t status)
{
    uint32_t timer_msk = nxmac_timers_int_un_mask_get();
    uint32_t timer_bit = CO_BIT(TX_AC2TIMER(access_category));
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    int i;

    // Flush all the queues
    txl_cfm_flush(access_category, &txl_cfm_env.cfmlist[access_category], status);
    for (i=0; i < RW_USER_MAX; i++)
    {
        txl_cfm_flush(access_category, &txlist->transmitting[i], status);
    }

    // Reset the list flags
    txlist->last_frame_exch = NULL;
    for (i=0; i < RW_USER_MAX; i++)
    {
        txlist->first_to_download[i] = NULL;
    }

    // Reset the TX buffer
    txl_buffer_reset(access_category);

    // Clear the TX timer
    nxmac_timers_int_un_mask_set(timer_msk & ~timer_bit);
    nxmac_timers_int_event_clear(timer_bit);
}

bool txl_cntrl_push(struct txdesc *txdesc, uint8_t access_category)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    #if RW_MUMIMO_TX_EN
    int status;
    #endif
    GLOBAL_INT_DECLARATION();

    // Initialize some fields of the THD
    txl_hwdesc_config_pre(txdesc, access_category);

    GLOBAL_INT_DISABLE();
    #if NX_AMPDU_TX
    #if RW_MUMIMO_TX_EN
    status =
    #endif
    txl_aggregate(txdesc, access_category); // Try to aggregate the descriptor
    #endif

    #if RW_MUMIMO_TX_EN
    if (status == SU_PACKET)
    #endif
    {
        // Check if we can try to allocate a Tx buffer
        if (txlist->first_to_download[0] == NULL)
        {
            txl_payload_alloc(txdesc, access_category, 0);
        }
        co_list_push_back(&txlist->transmitting[0], &txdesc->list_hdr);
    }
    
    tx_txdesc_obtain(txdesc, access_category);
    
    GLOBAL_INT_RESTORE();

    #if NX_POWERSAVE
    // Increase the number of packets in the TX path
    txl_cntrl_env.pck_cnt++;
    #endif

    #if (NX_TD)
    td_pck_ind(txdesc->host.vif_idx, txdesc->host.staid, false);
    #endif //(NX_TD)

    #if (NX_UAPSD || NX_DPSM)
    // Check if this frame will be a UAPSD trigger frame
    ps_check_tx_frame(txdesc->host.staid, txdesc->host.tid);
    #endif //(NX_UAPSD || NX_DPSM)

    #if RW_MUMIMO_TX_EN
    return (status == MU_PAUSED);
    #else
    return (false);
    #endif
}

void txl_cntrl_inc_pck_cnt(void)
{
    #if NX_POWERSAVE
    // Increase the number of packets in the TX path
    txl_cntrl_env.pck_cnt++;
    #endif
}

#if NX_TX_FRAME
static void txl_cntrl_postpone(struct txdesc *p_txdesc, uint8_t access_category)
{
    // Get STA Information entry
    struct sta_info_tag *p_sta_entry = &sta_info_tab[p_txdesc->host.staid];
    // Get frame descriptor
    struct txl_frame_desc_tag *p_frame_desc = (struct txl_frame_desc_tag *)p_txdesc;

    // Keep in mind that the frame has been postponed
    p_frame_desc->postponed = true;
    // Store access category in TID field of the TX descriptor
    p_txdesc->host.tid = access_category;

    // Insert descriptor in list of postponed descriptors
    co_list_push_back(&p_sta_entry->tx_desc_post, &p_txdesc->list_hdr);
}

bool txl_cntrl_push_int(struct txdesc *txdesc, uint8_t access_category)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    struct tx_hd *thd = &txdesc->lmac.hw_desc->thd;
    
    GLOBAL_INT_DECLARATION();
    
    #if 0 // (NX_CHNL_CTXT || NX_P2P)
    // Get VIF information entry
    struct vif_info_tag *p_vif_entry = &vif_info_tab[txdesc->host.vif_idx];

    // Check if packet can be transmitted
    if (!txl_cntrl_tx_check(p_vif_entry))
    {
        if (txdesc->host.staid != INVALID_STA_IDX)
        {
            txl_cntrl_postpone(txdesc, access_category);
	    	os_printf("--packet1_cannot_be_txed:%d\r\n\r\n", txdesc->host.vif_idx);

            return (true);
        }
        else
        {
            // Push back the TX Descriptor in the list of free descriptors
            txl_frame_release(txdesc, false);
	    	os_printf("--packet2_cannot_be_txed:%d\r\n\r\n", txdesc->host.vif_idx);

            return (false);
        }
    }
	else
	{
    	os_printf("--cntrl_pkt_can_be_txed\r\n");
	}
    #endif //(NX_CHNL_CTXT || NX_P2P)

    // Force a TX interrupt to be generated when the transmission is completed
    thd->macctrlinfo2 |= INTERRUPT_EN_TX;

    // Check if we can push the descriptor to the transmit queue immediately or if we
    // have to wait because an A-MPDU formatting may be ongoing
    GLOBAL_INT_DISABLE();
    
    #if NX_AMPDU_TX
    // We will now have some data programmed for transmission, so we can
    // activate the A-MPDU optimization
    txl_cntrl_env.txlist[access_category].ppdu_cnt++;
    // Close the pending aggregate
    #if RW_MUMIMO_TX_EN
    if (txlist->mumimo.users)
    {
        txl_mumimo_close(access_category);
    }
    else
    #endif
    if (txlist->agg[0].desc != NULL)
    {
        // Close the pending A-MPDU
        txl_aggregate_finish(access_category);
    }
    #endif

    // If data path is ready, program the fake DMA transfer
    if (txlist->first_to_download[0] == NULL)
        txl_int_fake_transfer(txdesc, access_category);

    // Push the descriptor to the TX list
    co_list_push_back(&txlist->transmitting[0], &txdesc->list_hdr);
    GLOBAL_INT_RESTORE();

    #if (NX_POWERSAVE)
    // Increase the number of packets in the TX path
    txl_cntrl_env.pck_cnt++;
    #endif

    return (true);
}
#endif

void txl_payload_handle(int access_category)
{
    struct txdesc *txdesc;
    struct txl_list *txlist;
    struct txl_buffer_tag *buffer;    
		
    // Walk through all access categories for which DMA IRQ is pending
    // Retrieve the access category from the DMA index
    txlist = &txl_cntrl_env.txlist[access_category];

    // Walk through the Tx list to handle payloads that have been downloaded
    do
    {
        GLOBAL_INT_DECLARATION();

        GLOBAL_INT_DISABLE();
        buffer = txl_buffer_pop(access_category);
        if(NULL == buffer)
        {
            ke_evt_clear(ke_get_ac_payload_bit(access_category));
            
            GLOBAL_INT_RESTORE();
        	break;
        }
        GLOBAL_INT_RESTORE();

        // Retrieve the descriptor from the buffer
        txdesc = buffer->txdesc;

        #if NX_TX_FRAME
        if (!is_int_frame(txdesc))
        #endif
        {
            #if NX_AMSDU_TX
            if (!(buffer->flags & BUF_INT_MSDU))
            #endif
            {
                // Check if we need to format the MAC header
                if (!is_qos_data(txdesc))
                {
                    if (txdesc->host.flags & TXU_CNTRL_MGMT)
                    {
                        struct mac_hdr *p_mac_hdr;
                        
                        p_mac_hdr = (struct mac_hdr *)(buffer->payload + buffer->padding);

                        // Check if we need to start a PM monitoring procedure
                        if (txl_cntrl_start_pm_mon(p_mac_hdr))
                        {
                            // Remember that the PM monitoring has been started for this frame
                            txdesc->host.flags |= TXU_CNTRL_MGMT_PM_MON;
                            // Start the monitoring
                            rxu_cntrl_monitor_pm(&p_mac_hdr->addr1);
                        }
                    }

                    txl_machdr_format(CPU2HW(buffer->payload) + buffer->padding);
                }

                // Format the TX DMA descriptor
                txl_hwdesc_config_post(txdesc, access_category);
            }
        }

        // Increment SW bridge DMA counter to keep track with HW one
        txlist->bridgedmacnt ++;

        // Manage an atomic frame exchange formatting and chaining to previous one in HW
        txl_frame_exchange_manage(txdesc, buffer, access_category);
    }while (0);
}

#if (NX_BW_LEN_ADAPT)
void txl_transmit_bw_drop(void)
{
    uint8_t access_category;
    uint32_t status, tx_status;

    // retrieve the hardware status
    status = nxmac_tx_rx_int_status_get();
    tx_status = status;

    #if NX_AMSDU_TX
    // don't care about tx header or buffer trigger, only access category, so duplicate all in tx
    tx_status |= tx_status >> (NXMAC_AC_0_TX_BUF_TRIGGER_POS - NXMAC_AC_0_TX_TRIGGER_POS);
    #endif

    // process only the tx part
    tx_status &= TX_IRQ_BITS;

    // don't care about tx header or buffer trigger, only access category, so duplicate all in tx
    status &= (NXMAC_AC_0BW_DROP_TRIGGER_BIT | NXMAC_AC_1BW_DROP_TRIGGER_BIT |
               NXMAC_AC_2BW_DROP_TRIGGER_BIT | NXMAC_AC_3BW_DROP_TRIGGER_BIT);

    // compute the access category
    access_category =  31 - co_clz(status) - NXMAC_AC_0BW_DROP_TRIGGER_POS;

    // handle the interrupt
    txl_bw_drop_handle(access_category);

    // acknowledge the interrupt
    nxmac_tx_rx_int_ack_clear(status);
}
#endif

#if RW_MUMIMO_TX_EN
void txl_sec_transmit_trigger(void)
{
    while (nxmac_sec_users_tx_int_event_get() & TX_SEC_IRQ_BITS)
    {
        uint8_t access_category;
        uint8_t user_idx;
        uint8_t highest;
        uint32_t status = nxmac_sec_users_tx_int_event_get() & TX_SEC_IRQ_BITS;

        // sanity check: we cannot be handling a TX interrupt if there isn't one pending
        ASSERT_REC(status != 0);

        // don't care about tx header or buffer trigger, only access category and user index,
        // so merge trigger and buffer status
        status |= status >> (NXMAC_SEC_U_1AC_0_TX_BUF_TRIGGER_POS - NXMAC_SEC_U_1AC_0_TX_TRIGGER_POS);

        // Now proceed only the merged bits
        status &= TX_SEC_IRQ_BITS_MERGED;

        // Get the highest bit set
        highest =  31 - co_clz(status);

        // Compute the corresponding access category and secondary user index
        access_category = highest % 4;
        user_idx = highest / 8;

        // sanity checks
        ASSERT_ERR(access_category < NX_TXQ_CNT);
        ASSERT_ERR(user_idx < RW_USER_MAX);

        // clear the handled interrupts (buffer and TX)
        nxmac_sec_users_tx_int_event_clear(CO_BIT(user_idx*8 + access_category + NXMAC_SEC_U_1AC_0_TX_BUF_TRIGGER_POS) |
                                           CO_BIT(user_idx*8 + access_category + NXMAC_SEC_U_1AC_0_TX_TRIGGER_POS));

        // Go through the list of already transmitted packets to release them
        txl_mumimo_secondary_done(access_category, user_idx + 1);
    }
}
#endif

void txl_transmit_trigger(void)
{
    uint8_t access_category;
    uint32_t status;
     
    // retrieve the hardware status (process only the tx part)
    status = nxmac_tx_rx_int_status_get();

    #if RW_MUMIMO_TX_EN
    // Check if secondary user interrupts are pending
    if (status & NXMAC_SEC_USER_TX_TRIGGER_BIT)
        txl_sec_transmit_trigger();
    #endif

    #if NX_AMSDU_TX
    // don't care about tx header or buffer trigger, only access category, so duplicate all in tx
    status |= status >> (NXMAC_AC_0_TX_BUF_TRIGGER_POS - NXMAC_AC_0_TX_TRIGGER_POS);
    #endif

    // process only the tx part (and duplicated buffer trigger)
    status &= TX_IRQ_BITS;

    // Check if a TX interrupt is pending
    if (status == 0)
        return;

    // compute the access category
    access_category =  31 - co_clz(status) - NXMAC_AC_0_TX_TRIGGER_POS;

    // sanity check
    ASSERT_ERR(access_category < NX_TXQ_CNT);

    // clear the handled interrupts (buffer and TX)
    #if NX_AMSDU_TX
    nxmac_tx_rx_int_ack_clear(CO_BIT(access_category + NXMAC_AC_0_TX_BUF_TRIGGER_POS) |
                              CO_BIT(access_category + NXMAC_AC_0_TX_TRIGGER_POS));
    #else
    nxmac_tx_rx_int_ack_clear(CO_BIT(access_category + NXMAC_AC_0_TX_TRIGGER_POS));
    #endif

    
    // Go through the list of already transmitted packets to release them
    txl_frame_exchange_done(access_category);

	#if CFG_TX_EVM_TEST
	evm_via_mac_continue();
	#endif
}

void txl_current_desc_get(int access_category, struct tx_hd **thd)
{
    #if NX_AMPDU_TX
    if (txl_cntrl_env.txlist[access_category].chk_state == ATHD_CHK_STATE)
    {
        //get the last txdesc queued into cfm list for this ac, its agg_desc and then bar_desc
        struct tx_agg_desc *agg_desc = txl_cntrl_env.txlist[access_category].agg_desc;
        struct tx_hd *bar_thd = &agg_desc->bar_thd;

        // First TX Header Descriptor chained to the HW
        *thd = bar_thd;
    }
    else
    #endif
    {
        // Pick first txdesc in transmitting list
        struct txdesc *txdesc = (struct txdesc *)co_list_pick(&(txl_cntrl_env.txlist[access_category].transmitting[0]));

        // a descriptor is waiting to be confirmed
        if (txdesc != NULL)
        {
            #if NX_AMPDU_TX
            // Check if the packet is the first MPDU of an A-MPDU
            if (is_mpdu_first(txdesc))
            {
                struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;
                struct tx_hd *a_thd = &agg_desc->a_thd;

                // First TX Header Descriptor chained to the HW
                *thd = a_thd;
            }
            else
            #endif
            {
                struct tx_hd *txhd = &txdesc->lmac.hw_desc->thd;

                *thd = txhd;
            }
        }
        else
        {
            *thd = NULL;
        }
    }
}

void txl_reset(void)
{
    int i,j;
    uint16_t seq_num = txl_cntrl_env.seqnbr;

    // Clear the possibly pending TX kernel events
    ke_evt_clear(KE_EVT_TXCFM_MASK);

    #if (NX_TX_FRAME)
    txl_cntrl_env.reset = true;
    #endif //(NX_TX_FRAME)

    for (i = 0; i < NX_TXQ_CNT; i++)
    {
        struct txl_list *txlist = &txl_cntrl_env.txlist[i];
        
    	// Flush all the queues
        txl_cfm_flush(i, &txl_cfm_env.cfmlist[i], DESC_DONE_SW_TX_BIT);
        for (j=0; j<RW_USER_MAX; j++)
        {
            txl_cfm_flush(i, &txlist->transmitting[j], DESC_DONE_SW_TX_BIT);
        }
        
        #if RW_MUMIMO_TX_EN
        for (j=0; j<RW_USER_MAX; j++)
        {
            if (txlist->mumimo.users & CO_BIT(j))
                txl_cfm_flush(i, &txlist->mumimo.tx[j], DESC_DONE_SW_TX_BIT);

            if (txlist->mumimo.txdesc[j] != NULL)
                txl_cfm_flush_desc(i, txlist->mumimo.txdesc[j], DESC_DONE_SW_TX_BIT);
        }
        
        while (1)
        {
            struct tx_agg_desc *agg_desc = (struct tx_agg_desc *)co_list_pop_front(&txlist->aggregates);

            if (agg_desc == NULL)
                break;

            txl_cfm_flush(i, &agg_desc->cfm, DESC_DONE_SW_TX_BIT);
        }
        #endif
    }

    // Reset the required data structures
    txl_hwdesc_reset();
    txl_buffer_reinit();
    txl_cfm_init();

    memset(&txl_cntrl_env, 0, sizeof(txl_cntrl_env));

    // Put back the value of the sequence number
    txl_cntrl_env.seqnbr = seq_num;

    for (i = 0; i < NX_TXQ_CNT; i++)
    {
        for (j=0; j<RW_USER_MAX; j++)
        {
            co_list_init(&(txl_cntrl_env.txlist[i].transmitting[j]));
        }

        #if RW_MUMIMO_TX_EN
        txl_cntrl_env.txlist[i].mumimo.open = MU_USER_MASK;
        ipc_emb_enable_users(i, MU_USER_MASK);
        #endif

        txl_cntrl_env.txlist[i].last_frame_exch = NULL;
        txl_cntrl_env.txlist[i].bridgedmacnt = 0;
        txl_cntrl_env.txlist[i].chk_state = THD_CHK_STATE;

        #if NX_AMPDU_TX
        txl_cntrl_env.txlist[i].ppdu_cnt = 0;
        txl_cntrl_env.txlist[i].agg_desc = NULL;
        txl_cntrl_env.txlist[i].agg_desc_prev = NULL;
        co_list_init(&(txl_cntrl_env.txlist[i].aggregates));
        #endif
    }
}



/// @}  // end of group TX_CNTRL
