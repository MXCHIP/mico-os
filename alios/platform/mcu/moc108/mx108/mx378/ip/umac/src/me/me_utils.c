/**
 ****************************************************************************************
 *
 * @file me_utils.c
 *
 * @brief All utility functions manipulating rates, etc.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h"

#include "mac_defs.h"
#include "me_utils.h"
#include "co_math.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "mac_ie.h"
#include "hal_desc.h"
#include "mac_frame.h"
#include "me.h"
#include "mm.h"

#if RC_ENABLE
#include "rc.h"
#else
#include "rc_basic.h"
#endif

#include "me_mgmtframe.h"
#include "tpc.h"

#if CFG_SUPPORT_CALIBRATION
#include "bk7011_cal_pub.h"
#endif

#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

/// Control channel on which we have to enable the AP
struct scan_chan_tag chan;
/// Center frequency of the first segment
uint32_t center_freq1;
/// Center frequency of the second segment (only in 80+80 configuration)
uint32_t center_freq2;
/// Width of channel
uint8_t ch_width;

/*
 * FUNCTION IMPLEMENTATION
 ****************************************************************************************
 */
static void me_chan_ctxt_update(struct vif_info_tag *vif)
{
    struct mac_bss_info *bss = &vif->bss_info;
    struct mm_chan_ctxt_update_req *req = KE_MSG_ALLOC(MM_CHAN_CTXT_UPDATE_REQ, TASK_MM, TASK_ME,
                                          mm_chan_ctxt_update_req);

    // Sanity check - We shall have a channel context assigned when this procedure is
    // executed
    ASSERT_ERR(vif->chan_ctxt != NULL);

    // Fill-in the channel context addition request
    req->chan_index = vif->chan_ctxt->idx;
    req->band = bss->chan->band;
    req->type = bss->phy_bw;
    req->prim20_freq = bss->chan->freq;
    req->center1_freq = bss->center_freq1;
    req->center2_freq = bss->center_freq2;
    req->tx_power = vif->chan_ctxt->channel.tx_power;

    // Send the message
    ke_msg_send(req);
}

uint8_t me_11ac_mcs_max(uint16_t mcs_map)
{
    uint8_t mcs_max;

    switch (mcs_map & MAC_VHT_MCS_MAP_MSK)
    {
    case MAC_VHT_MCS_MAP_0_7:
        mcs_max = 7;
        break;
    case MAC_VHT_MCS_MAP_0_8:
        mcs_max = 8;
        break;
    case MAC_VHT_MCS_MAP_0_9:
        mcs_max = 9;
        break;
    default:
        mcs_max = 7;
        break;
    }

    return(mcs_max);
}

uint8_t me_11ac_nss_max(uint16_t mcs_map)
{
    uint8_t nss_max;

    // Go through the MCS map to check how many SS are supported
    for (nss_max = 7; nss_max > 0; nss_max--)
    {
        if (((mcs_map >> (2 * nss_max)) & MAC_VHT_MCS_MAP_MSK) != MAC_VHT_MCS_MAP_NONE)
            break;
    }

    return(nss_max);
}

uint8_t me_11n_nss_max(uint8_t *mcs_set)
{
    uint8_t nss_max;

    // Go through the MCS map to check how many SS are supported
    for (nss_max = 3; nss_max > 0; nss_max--)
    {
        if (mcs_set[nss_max] != 0)
            break;
    }

    return(nss_max);
}

uint8_t me_legacy_ridx_min(uint16_t rate_map)
{
    uint8_t i;

    for (i = 0; i < MAC_RATESET_LEN; i++)
    {
        if (rate_map & (1 << i))
        {
            break;
        }
    }

    return i;
}

uint8_t me_legacy_ridx_max(uint16_t rate_map)
{
    uint8_t i;
    uint8_t mcs_max;

    if (rate_map != 0 )
    {
        for (i = 0; i < MAC_RATESET_LEN; i++)
        {
            if (rate_map & (1 << (HW_RATE_54MBPS - i)))
            {
                break;
            }
        }
        mcs_max = HW_RATE_54MBPS - i;
    }
    else
    {
        mcs_max = MAC_RATESET_LEN;
    }

    return mcs_max;
}

static void me_erp_prot_check(uint32_t erp_addr, uint16_t *port_status)
{
    /*
     * Extract received ERP parameters field value
     *      Bit 0 - NonERP Present
     *      Bit 1 - Use Protection
     *      Bit 2 - Barker Preamble Mode
     */
    uint8_t erp_val = co_read8p(erp_addr + MAC_ERP_PARAM_OFT);

    // Clear current ERP Protection status
    *port_status &= ~MAC_PROT_ERP_STATUS_MASK;

    if (erp_val & MAC_ERP_NON_ERP_PRESENT)
    {
        // Non_ERP STA present
        *port_status |= CO_BIT(MAC_PROT_NONERP_PRESENT_OFT);
    }

    if (erp_val & MAC_ERP_USE_PROTECTION)
    {
        // Use protection
        *port_status |= CO_BIT(MAC_PROT_USE_PROTECTION_OFT);
    }

    if (erp_val & MAC_ERP_BARKER_PREAMBLE_MODE)
    {
        // Barker Preamble Mode
        *port_status |= CO_BIT(MAC_PROT_BARKER_PREAMB_OFT);
    }
}

static void me_ht_prot_check(uint32_t ht_op_addr, uint16_t *port_status)
{
    // HT Information Subset 2 field value
    uint16_t ht_info_2 = co_read16p(ht_op_addr + MAC_HT_OPER_INFO_SUBSET2_OFT);

    // Clear current HT Protection status
    *port_status &= ~MAC_PROT_HT_STATUS_MASK;

    // Keep HT Operation Mode
    *port_status |= ((ht_info_2 & MAC_HT_OPER_OP_MODE_MASK) << MAC_PROT_HT_OPERATION_MODE_OFT);

    if (ht_info_2 & MAC_HT_OPER_OBSS_MASK)
    {
        // OBSS Non-HT STAs present
        *port_status |= CO_BIT(MAC_PROT_OBSS_NONHT_PRESENT_OFT);
    }

    if (ht_info_2 & MAC_HT_OPER_NONGF_MASK)
    {
        // Nongreenfield HT STAs Present
        *port_status |= CO_BIT(MAC_PROT_NON_GF_HT_STA_OFT);
    }
}

static void me_pol_tbl_prot_upd(struct sta_info_tag *sta_info, uint16_t prot_status)
{
    struct sta_pol_tbl_cntl *pol_tbl = &sta_info->pol_tbl;
    struct vif_info_tag *vif;
    bool prot_11b, prot_non_ht, prot_40m;
    uint8_t ht_prot_mode;

    do
    {
        // Reset Protection Configuration in the policy table info structure
        pol_tbl->prot_cfg = 0;

        // Require update of protection configuration field in policy table
        pol_tbl->upd_field |= CO_BIT(STA_MGMT_POL_UPD_PROT);

        // Get required protections
        prot_11b     = prot_status & (CO_BIT(MAC_PROT_USE_PROTECTION_OFT));
        ht_prot_mode = (prot_status >> MAC_PROT_HT_OPERATION_MODE_OFT) & MAC_PROT_HT_OPERATION_MASK;
        // TODO [LT] - OBSS and NONMEMBER to be checked if GF is used
        prot_non_ht  = //(prot_status & CO_BIT(MAC_PROT_OBSS_NONHT_PRESENT_OFT)) ||
            //(ht_prot_mode == MAC_HT_OPER_PROT_NONMEMBER)            ||
            (ht_prot_mode == MAC_HT_OPER_PROP_NON_HT_MIXED);
        prot_40m     = (ht_prot_mode == MAC_HT_OPER_PROT_20MHZ) && prot_11b && prot_non_ht;

        /*
         ***********************************************************************
         * NAV Protection Frame Exchange
         *      If Protection has to be enabled, CTS-to-self mechanism is used
         *      Protection will be enabled in following cases:
         *          - ERP element's Use Protection Bit set to 1 (11b protection)
         *          - HT operation's OBSS Non-HT STAs present Bit set to 1
         *          - HT operation's HT Protection mode not set to No Protection
         ***********************************************************************
         */
        if (!prot_11b && !prot_non_ht && !prot_40m)
        {
            // Do not use protection
            break;
        }

        // Use CTS-to-self
        pol_tbl->prot_cfg |= STA_MGMT_PROT_NAV_CTS_SELF_MASK;

        // Get VIF interface
        vif = &vif_info_tab[sta_info->inst_nbr];

        /*
         ***********************************************************************
         * MCS Index of Protection Frame
         *      If 11b protection required, use highest 11b rate allowed by AP
         *      else use highest 11g rate allowed
         ***********************************************************************
         */
        if (prot_11b)
        {
            pol_tbl->prot_cfg |= ((vif->bss_info.high_11b_rate) << STA_MGMT_PROT_MCS_OFT);
        }
        else
        {
            pol_tbl->prot_cfg |= ((vif->bss_info.high_legacy_rate) << STA_MGMT_PROT_MCS_OFT);
        }

        /*
         ***********************************************************************
         * Bandwidth of Protection Frame for Transmission
         *      If current channel type is 40Mhz and
         *          - No 11b protection
         *          - No NonHT protection
         *          - No 20MHz HT STA associated with AP,
         *      then use 40 MHz bandwidth
         ***********************************************************************
         */
        if (vif->chan_ctxt->channel.type > PHY_CHNL_BW_20)
        {
            if (!prot_40m)
            {
                // Use 40 MHz bandwidth
                pol_tbl->prot_cfg |= (BW_40MHZ << STA_MGMT_PROT_BW_OFT);
            }
        }

        /*
         ***********************************************************************
         * Format and Modulation of Protection frame for Transmission
         *      If non-HT STA detected, use NON-HT modulation.
         *      else use HT-MF
         ***********************************************************************
         */
        if (!prot_11b && !prot_non_ht)
        {
            pol_tbl->prot_cfg |= (FORMATMOD_HT_MF << STA_MGMT_PROT_FMT_MOD_OFT);
        }
    }
    while (0);

    // Reset PPDU TX Configuration in the policy table info structure
    pol_tbl->ppdu_tx_cfg = 0;

#if !RC_ENABLE
    // Require update of PPDU configuration field in policy table
    pol_tbl->upd_field |= CO_BIT(STA_MGMT_POL_UPD_PPDU_TX);
#endif
    /*
     ***********************************************************************
     * Preamble Type of PPDU for Transmission
     ***********************************************************************
     */
    if (!(prot_status & (CO_BIT(MAC_PROT_BARKER_PREAMB_OFT))))
    {
        pol_tbl->ppdu_tx_cfg |= STA_MGMT_PPDU_TX_PREAM_MASK;
    }
#if RC_ENABLE
    // Update preamble type in the RC algorithm
    rc_update_preamble_type(sta_info->staid, (pol_tbl->ppdu_tx_cfg > 0));
#endif
}

static void me_pol_tbl_bw_upd(struct sta_info_tag *sta_info, uint8_t new_bw)
{
    // Policy Table Info
    struct sta_pol_tbl_cntl *pol_tbl = &sta_info->pol_tbl;

    // Check if the new bandwidth of BSS is compliant with the supported one
    if (new_bw != sta_info->info.bw_cur)
    {
        struct mac_sta_info *info = &sta_info->info;
        uint8_t local_supp_nss = 0;
        uint8_t peer_supp_nss = 0;
        uint8_t nss;

#if NX_VHT
        if (info->capa_flags & STA_VHT_CAPA)
        {
            ASSERT_ERR(me_env.vht_supported);

            // Check the peer and local supported NSS (VHT)
            peer_supp_nss = me_11ac_nss_max(info->vht_cap.rx_mcs_map);
            local_supp_nss = me_11ac_nss_max(me_env.vht_cap.tx_mcs_map);

            nss = co_min(local_supp_nss, peer_supp_nss);
        }
        else
#endif
        {
            ASSERT_ERR(me_env.ht_supported);
            // Check the peer and local supported NSS (VHT)
            peer_supp_nss = me_11n_nss_max(info->ht_cap.mcs_rate);
            local_supp_nss = me_11n_nss_max(me_env.ht_cap.mcs_rate);

            nss = co_min(local_supp_nss, peer_supp_nss);
        }
        // Compute new BW
        sta_info->info.bw_cur = co_min(new_bw, sta_info->info.bw_max);

#if RC_ENABLE
        // Update max BW allowed in the RC algorithm
        rc_update_bw_nss_max(sta_info->staid, sta_info->info.bw_cur, nss);
#endif

        (void)nss;

        // Require update of protection configuration field in policy table
        pol_tbl->upd_field |= CO_BIT(STA_MGMT_POL_UPD_BW);
    }
}

uint8_t me_rate_translate(uint8_t rate)
{
    uint8_t hwrate = 0;

    rate &= ~MAC_BASIC_RATE;
    switch(rate)
    {
    case MAC_RATE_1MBPS:
        hwrate = HW_RATE_1MBPS;
        break;
    case MAC_RATE_2MBPS:
        hwrate = HW_RATE_2MBPS;
        break;
    case MAC_RATE_5_5MBPS:
        hwrate = HW_RATE_5_5MBPS;
        break;
    case MAC_RATE_11MBPS:
        hwrate = HW_RATE_11MBPS;
        break;
    case MAC_RATE_48MBPS:
        hwrate = HW_RATE_48MBPS;
        break;
    case MAC_RATE_24MBPS:
        hwrate = HW_RATE_24MBPS;
        break;
    case MAC_RATE_12MBPS:
        hwrate = HW_RATE_12MBPS;
        break;
    case MAC_RATE_6MBPS:
        hwrate = HW_RATE_6MBPS;
        break;
    case MAC_RATE_54MBPS:
        hwrate = HW_RATE_54MBPS;
        break;
    case MAC_RATE_36MBPS:
        hwrate = HW_RATE_36MBPS;
        break;
    case MAC_RATE_18MBPS:
        hwrate = HW_RATE_18MBPS;
        break;
    case MAC_RATE_9MBPS:
        hwrate = HW_RATE_9MBPS;
        break;
    default:
        hwrate = 0xFF;
    }
    return(hwrate);
}

uint32_t me_basic_rate_bitfield_build(struct mac_rateset *rateset)
{
    int i;
    int bit_pos;
    uint32_t brates = 0;

    // Build the legacy rates bitfield
    for (i = 0; i < rateset->length; i++)
    {
        // If the current rate is not basic, then we go to the next one
        if (!(rateset->array[i] & MAC_BASIC_RATE))
            continue;

        // Convert the rate into an index
        bit_pos = me_rate_translate(rateset->array[i]);

        // Check if the rate is consistent
        ASSERT_WARN(bit_pos < MAC_RATESET_LEN);

        // Set the corresponding bit in the bitfield
        if (bit_pos < MAC_RATESET_LEN)
        {
            brates |= CO_BIT(bit_pos);
        }
    }

    return (brates);
}

void me_rate_bitfield_legacy_build(struct mac_rates   *ratefield,
                                   struct mac_rateset *rateset,
                                   bool                basic_only)
{
    int i;
    int bit_pos;

    // First reset the bitfield
    ratefield->legacy = 0;

    // Build the legacy rates bitfield
    for (i = 0; i < rateset->length; i++)
    {
        // If we are looking for basic rates only and the current rate is not basic,
        // then we go to the next rate
        if (basic_only && !(rateset->array[i] & MAC_BASIC_RATE))
        {
            continue;
        }

        // Convert the rate into an index
        bit_pos = me_rate_translate(rateset->array[i]);

        // Check if the rate is consistent
        ASSERT_WARN(bit_pos < MAC_RATESET_LEN);

        // Set the corresponding bit in the bitfield
        if (bit_pos < MAC_RATESET_LEN)
        {
            ratefield->legacy |= CO_BIT(bit_pos);
        }
    }
}

void me_rate_bitfield_ht_build(struct mac_rates   *ratefield,
                               struct mac_mcsset  *mcsset,
                               bool                basic_only)
{
    int m;

    // First reset the bitfield
    memset(ratefield->mcs, 0, sizeof(ratefield->mcs));

    // copy MCS rates from MIB to bitfield encoded structure
    if (basic_only == false)
    {
        for (m = 0 ; m < MAC_MCS_WORD_CNT; m++)
        {
            ratefield->mcs[m] =  ( (mcsset->array[(m * 4) + 3] << 24) |
                                   (mcsset->array[(m * 4) + 2] << 16) |
                                   (mcsset->array[(m * 4) + 1] << 8) |
                                   (mcsset->array[(m * 4) + 0] << 0) );
        }
    }
}

uint16_t me_rate_bitfield_vht_build(uint16_t mcs_map_1, uint16_t mcs_map_2)
{
    uint8_t i;
    uint16_t mcs_map = 0xFFFF;

    for (i = 0; i < 8; i++)
    {
        uint8_t mcs_cfg_1 = (mcs_map_1 >> (i << 1)) & MAC_VHT_MCS_MAP_MSK;
        uint8_t mcs_cfg_2 = (mcs_map_2 >> (i << 1)) & MAC_VHT_MCS_MAP_MSK;
        if ((mcs_cfg_1 == MAC_VHT_MCS_MAP_NONE) || (mcs_cfg_2 == MAC_VHT_MCS_MAP_NONE))
        {
            break;
        }
        else
        {
            mcs_map &= ~(MAC_VHT_MCS_MAP_MSK << (i << 1));
            mcs_map |= (co_min(mcs_cfg_1, mcs_cfg_2) & MAC_VHT_MCS_MAP_MSK) << (i << 1);
        }
    }

    return mcs_map;
}

uint16_t me_build_capability(uint8_t vif_idx)
{
    uint16_t capa_info = 0;
    struct vif_info_tag *vif = &vif_info_tab[vif_idx];

    capa_info |= MAC_CAPA_ESS;

    // add privacy
    capa_info &= ~MAC_CAPA_PRIVA;

    if (vif->bss_info.chan->band == PHY_BAND_5G)
        capa_info |= MAC_CAPA_SPECTRUM;

    return capa_info;
}

void me_init_rate(struct sta_info_tag *sta_entry)
{
#if RC_ENABLE
    rc_init(sta_entry); 
#else
    rc_basic_init(sta_entry);
#endif
    me_update_buffer_control(sta_entry, NULL);
}


void me_init_bcmc_rate(struct sta_info_tag *sta_entry)
{
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
#if !RC_ENABLE
    struct tx_policy_tbl *pol = &rc->buf_ctrl[0]->policy_tbl;
#endif
    struct mac_rateset *rates = &sta_entry->info.rate_set;
    uint8_t max_rate = 0;
    int i;

    ASSERT_ERR(rates->length != 0);
    for (i = 0; i < rates->length; i++)
    {
        if ((rates->array[i] & ~MAC_BASIC_RATE) > max_rate)
            max_rate = rates->array[i] & ~MAC_BASIC_RATE;
    }

#if RC_ENABLE
    rc_init_bcmc_rate(sta_entry, me_rate_translate(max_rate));
#else
    rc->r_idx_min = me_rate_translate(max_rate);
    rc->r_idx_max = rc->r_idx_min;

    rc->r_idx = rc->r_idx_max;
    rc->fail_cnt = 0;
    rc->tx_cnt = 0;
    rc->buf_ctrl_idx = 0;

    pol->ratecntrlinfo[0] = rc->r_idx;
#endif

    rc->upd_field = 0;
}


void me_tx_cfm_singleton(struct txdesc *txdesc)
{
    // Check the status
    uint32_t status = txdesc->lmac.hw_desc->cfm.status;
    uint32_t retry_cnt = ((status & NUM_MPDU_RETRIES_MSK) >> NUM_MPDU_RETRIES_OFT);
    uint32_t trials;
    uint32_t failures;

    // Compute the statistics
    trials = retry_cnt + 1;
    failures = retry_cnt + ((status & RETRY_LIMIT_REACHED_BIT) != 0);

    // Update statistics
#if RC_ENABLE
    rc_update_counters(txdesc->host.staid, trials, failures, 0, 0);
#else
    struct sta_info_tag *sta_entry = &sta_info_tab[txdesc->host.staid];
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    rc->fail_cnt += failures;
    rc->tx_cnt += trials;
#endif
}


void me_tx_cfm_ampdu(uint8_t sta_idx, uint32_t txed, uint32_t txok, bool retry_required)
{
    // Update statistics
#if RC_ENABLE
    rc_update_counters(sta_idx, txed, (txed - txok), 1, retry_required);
#else
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    rc->fail_cnt += (txed - txok);
    rc->tx_cnt += txed;
#endif
}

uint8_t me_check_rc(uint8_t sta_idx, bool *tx_ampdu)
{
#if RC_ENABLE
    return rc_check(sta_idx, tx_ampdu);
#else
    rc_basic_check(sta_idx);
    return 0;
#endif
}

void me_update_buffer_control(struct sta_info_tag *sta_info, struct umacdesc *umac_desc)
{
    // Policy Table Info
    struct sta_pol_tbl_cntl *pol_tbl = &sta_info->pol_tbl;

    // Check if a field has to be updated
    if (pol_tbl->upd_field)
    {
        int i;
#if RC_ENABLE
        uint8_t stbc_nss = 0;
        uint8_t sta_stbc_nss = 0;
        int use_stbc = 0;
#endif
        // Next Buffer Control index
        uint8_t next_idx = (pol_tbl->buf_ctrl_idx ^ 1);
        // Current TX Policy Table
        struct tx_policy_tbl *curr_pol = &pol_tbl->buf_ctrl[pol_tbl->buf_ctrl_idx]->policy_tbl;
        // Next TX Policy Table
        struct tx_policy_tbl *next_pol = &pol_tbl->buf_ctrl[next_idx]->policy_tbl;
        // Rate control info
        uint32_t rate_info[RATE_CONTROL_STEPS];
        // Power control info
        uint32_t pwr_info[RATE_CONTROL_STEPS];

        // Get the currently used rate control information
        for (i = 0; i < RATE_CONTROL_STEPS; i++)
        {
            rate_info[i] = curr_pol->ratecntrlinfo[i];
            pwr_info[i] = curr_pol->powercntrlinfo[i];
        }

#if RC_ENABLE
        // Update rate configuration (pre_type, short_gi, bw, nss, mcs)
        if (pol_tbl->upd_field & CO_BIT(STA_MGMT_POL_UPD_RATE))
        {
            struct rc_sta_stats *rc_ss = pol_tbl->sta_stats;
            sta_stbc_nss = sta_info->info.stbc_nss;
            for (i = 0; i < RATE_CONTROL_STEPS; i++)
            {
                uint8_t nss = 0;
                uint16_t idx = rc_ss->retry[(rc_ss->sw_retry_step + i) % RATE_CONTROL_STEPS].idx;
                uint32_t new_rate_info = (RC_MAX_NUM_RETRY << N_RETRY_RCX_OFT) |
                                         (rc_ss->rate_stats[idx].rate_config &
                                          (MCS_INDEX_TX_RCX_MASK | BW_TX_RCX_MASK |
                                           SHORT_GI_TX_RCX_MASK | PRE_TYPE_TX_RCX_MASK |
                                           FORMAT_MOD_TX_RCX_MASK));
                uint32_t format = (new_rate_info & FORMAT_MOD_TX_RCX_MASK) >>
                                  FORMAT_MOD_TX_RCX_OFT;

                // Get the number of SS of this frame
                if (format >= FORMATMOD_HT_MF)
                {
                    uint32_t mcs_idx = (new_rate_info & MCS_INDEX_TX_RCX_MASK)
                                       >> MCS_INDEX_TX_RCX_OFT;
#if NX_VHT
                    if (format == FORMATMOD_VHT)
                    {
                        // Extract NSS
                        nss = (mcs_idx & VHT_NSS_MASK) >> VHT_NSS_OFT;
                    }
                    else
#endif
                    {
                        // Extract NSS
                        nss = (mcs_idx & HT_NSS_MASK) >> HT_NSS_OFT;
                    }

                    // Check if we have to enable or disable STBC
                    if (i == 0)
                    {
                        if (nss < sta_stbc_nss)
                        {
                            stbc_nss = nss;
                            use_stbc = 1;
                        }
                    }
                    else if (use_stbc && (nss != stbc_nss))
                    {
                        use_stbc = 0;
                    }
                }

                // Clear the current configuration
                rate_info[i] &= ~((uint32_t)N_RETRY_RCX_MASK | (uint32_t)MCS_INDEX_TX_RCX_MASK |
                                  (uint32_t)BW_TX_RCX_MASK | (uint32_t)SHORT_GI_TX_RCX_MASK |
                                  (uint32_t)PRE_TYPE_TX_RCX_MASK | (uint32_t)FORMAT_MOD_TX_RCX_MASK);
                // Set new configuration
                rate_info[i] |= new_rate_info;
            }

            next_pol->phycntrlinfo1 &= ~STBC_PT_MASK;
            if (use_stbc)
            {
                next_pol->phycntrlinfo1 |= (stbc_nss + 1) << STBC_PT_OFT;
            }

        }
#else
        // Update rate
        if (pol_tbl->upd_field & CO_BIT(STA_MGMT_POL_UPD_RATE))
        {
            int r_idx = pol_tbl->r_idx;

#if CFG_SUPPORT_CALIBRATION
            rwnx_cal_set_txpwr_by_rate(r_idx);// set tx output power for change rate
#endif

            for (i = 0; i < RATE_CONTROL_STEPS; i++)
            {
                // Clear the current rate index
                rate_info[i] &= ~MCS_INDEX_TX_RCX_MASK;
                // Set the new one
                rate_info[i] |= r_idx;
                // Decrease the rate index for next retry
                r_idx = (r_idx >= (pol_tbl->r_idx_min + 2)) ? r_idx - 2 : pol_tbl->r_idx_min;
            }
        }

        // Update bandwidth
        if (pol_tbl->upd_field & CO_BIT(STA_MGMT_POL_UPD_BW))
        {
            // VIF Interface
            struct vif_info_tag *vif = &vif_info_tab[sta_info->inst_nbr];
            uint32_t gi = 0;

            // Compute the GI to use
            if (sta_info->info.bw_cur == BW_40MHZ)
            {
                if (sta_info->info.ht_cap.ht_capa_info & MAC_HTCAPA_SHORTGI_40)
                    gi = SHORT_GI_TX_RCX_MASK;
            }
            else
            {
                if (sta_info->info.ht_cap.ht_capa_info & MAC_HTCAPA_SHORTGI_20)
                    gi = SHORT_GI_TX_RCX_MASK;
            }

            for (i = 0; i < RATE_CONTROL_STEPS; i++)
            {
                // Clear the current bandwidth and GI type
                rate_info[i] &= ~(BW_TX_RCX_MASK | SHORT_GI_TX_RCX_MASK);
                // Set the new ones
                rate_info[i] |= vif->bss_info.bw | gi;
            }
        }
#endif

        if (pol_tbl->upd_field & CO_BIT(STA_MGMT_POL_UPD_PROT))
        {
            uint32_t prot_cfg = ((uint32_t)(pol_tbl->prot_cfg) << STA_MGMT_PROT_HW_OFT);

            for (i = 0; i < RATE_CONTROL_STEPS; i++)
            {
                // Clear the current used Protection Fields
                rate_info[i] &= ~STA_MGMT_PROT_HW_MASK;

                // Apply computed values
                rate_info[i] |= prot_cfg;
            }
        }

#if !RC_ENABLE
        if (pol_tbl->upd_field & CO_BIT(STA_MGMT_POL_UPD_PPDU_TX))
        {
            for (i = 0; i < RATE_CONTROL_STEPS; i++)
            {
                // Clear the current used Protection Fields
                rate_info[i] &= ~STA_MGMT_PPDU_TX_HW_MASK;

                // Apply computed values
                rate_info[i] |= (uint32_t)(pol_tbl->ppdu_tx_cfg);
            }
        }
#endif

        // update Tx power
        if (pol_tbl->upd_field & CO_BIT(STA_MGMT_POL_UPD_TX_POWER))
        {
            struct vif_info_tag *vif = &vif_info_tab[sta_info->inst_nbr];
            uint8_t idx;

            idx = tpc_get_vif_tx_power(vif);
            for (i = 0; i < RATE_CONTROL_STEPS; i++)
            {
                pwr_info[i] = idx << TX_PWR_LEVEL_PT_RCX_OFT;
            }
        }

        // Update the next policy table
        for (i = 0; i < RATE_CONTROL_STEPS; i++)
        {
            next_pol->ratecntrlinfo[i] = rate_info[i];
            next_pol->powercntrlinfo[i] = pwr_info[i];
        }

        // Get the new policy table
        pol_tbl->buf_ctrl_idx = next_idx;
        // Clear update bit field
        pol_tbl->upd_field = 0;
    }

    if (umac_desc)
    {
#if (RW_BFMER_EN)
        // Indicate if used rates allow this frame to be beamformed if sent as singleton
        bool can_use_bfm;
        // Nc received in the latest beamforming report
        uint8_t nc = bfr_get_last_nc(sta_info->staid);
        // Read number of antennas
        uint8_t ntx = phy_get_ntx();
        // TX Policy Table
        struct tx_policy_tbl *p_pol_tbl = &pol_tbl->buf_ctrl[pol_tbl->buf_ctrl_idx]->policy_tbl;

        can_use_bfm = (nc != BFR_INVALID_NC);
#endif //(RW_BFMER_EN)

        // Store policy table to be used in the UMAC descriptor
        umac_desc->buf_control = pol_tbl->buf_ctrl[pol_tbl->buf_ctrl_idx];

#if (RW_BFMER_EN)
        for (int i = 0; (i < RATE_CONTROL_STEPS) && can_use_bfm; i++)
        {
            uint32_t rate_info = p_pol_tbl->ratecntrlinfo[i];
            // Extract the number of spatial streams (for VHT)
            uint8_t nss = ((((rate_info & MCS_INDEX_TX_RCX_MASK) >> MCS_INDEX_TX_RCX_OFT)
                            & VHT_NSS_MASK) >> VHT_NSS_OFT);

            if ((((rate_info & FORMAT_MOD_TX_RCX_MASK) >> FORMAT_MOD_TX_RCX_OFT) != FORMATMOD_VHT)
                    || (nss > nc) || (nss == ntx))
            {
                can_use_bfm = false;
            }
        }

        if (can_use_bfm)
        {
            umac_desc->tx_flags |= TX_SWDESC_UMAC_BEAMFORM_MASK;
        }
        else
        {
            umac_desc->tx_flags &= ~TX_SWDESC_UMAC_BEAMFORM_MASK;
        }
#endif //(RW_BFMER_EN)
    }
}

void me_bw_check(uint32_t ht_op_addr, uint32_t vht_op_addr, struct mac_bss_info *bss)
{
    // By default we consider that the BW will be 20MHz
    bss->bw = BW_20MHZ;
    bss->phy_bw = PHY_CHNL_BW_20;
    bss->center_freq1 = bss->chan->freq;
    bss->center_freq2 = 0;

    // Check if there is a HT operation element
    if (ht_op_addr != 0)
    {
        uint8_t sec_ch_oft = co_read8p(ht_op_addr + MAC_HT_OPER_PRIM_CH_OFT + 1) & 3;
        if (sec_ch_oft != 0)
        {
            // Compute the secondary channel frequency offset
            int8_t freq_offset = (sec_ch_oft == 1) ? 10 : -10;
            bss->center_freq1 = bss->chan->freq + freq_offset;
            bss->bw = BW_40MHZ;
            bss->phy_bw = PHY_CHNL_BW_40;
        }
    }

    // Check if there is a VHT operation element
#if NX_VHT
    if (vht_op_addr != 0)
    {
        uint8_t chan_width = co_read8p(vht_op_addr + MAC_VHT_CHAN_WIDTH_OFT) & 3;

        if (chan_width)
        {
            uint8_t center_freq0 = co_read8p(vht_op_addr + MAC_VHT_CENTER_FREQ0_OFT);
            uint8_t center_freq1 = co_read8p(vht_op_addr + MAC_VHT_CENTER_FREQ1_OFT);

            bss->center_freq1 = phy_channel_to_freq(bss->chan->band, center_freq0);

            if (center_freq1 && (chan_width == 3))
            {
                bss->center_freq2 = phy_channel_to_freq(bss->chan->band, center_freq1);
            }
            switch (chan_width)
            {
            case 1:
                bss->bw = BW_80MHZ;
                bss->phy_bw = PHY_CHNL_BW_80;
                break;
            case 2:
                bss->bw = BW_160MHZ;
                bss->phy_bw = PHY_CHNL_BW_160;
                break;
            case 3:
                bss->bw = BW_160MHZ;
                bss->phy_bw = PHY_CHNL_BW_80P80;
                break;
            default:
                break;
            }
        }
    }
#endif
}

void me_beacon_check(uint8_t vif_idx, uint16_t length, uint32_t bcn_addr)
{
    uint32_t addr, ht_op_addr = 0, vht_op_addr = 0;
    struct vif_info_tag *vif = &vif_info_tab[vif_idx];
    struct sta_info_tag *sta = (struct sta_info_tag *)co_list_pick(&vif->sta_list);
    uint16_t cur_prot_status      = vif->bss_info.prot_status;
    uint8_t cur_bw                = vif->bss_info.bw;
    uint8_t cur_pwr_const         = vif->bss_info.power_constraint;
    uint8_t csa_count, csa_mode;

    /**
     *================================================================
     * Check if ERP field is present in the received beacon
     *================================================================
     */
    addr = mac_ie_find(bcn_addr + MAC_BEACON_VARIABLE_PART_OFT,
                       length - MAC_BEACON_VARIABLE_PART_OFT,
                       MAC_ELTID_ERP);

    if (addr)
    {
        me_erp_prot_check(addr, &vif->bss_info.prot_status);
    }

    /**
     *================================================================
     * Check if HT and VHT Operation Element are present in
     * the received beacon
     *================================================================
     */
    if (me_env.ht_supported)
    {
        ht_op_addr = mac_ie_find(bcn_addr + MAC_BEACON_VARIABLE_PART_OFT,
                                 length - MAC_BEACON_VARIABLE_PART_OFT,
                                 MAC_ELTID_HT_OPERATION);

        if (ht_op_addr)
        {
            me_ht_prot_check(addr, &vif->bss_info.prot_status);
        }

#if NX_VHT
        /**
         *================================================================
         * Check if VHT Operation Element field is present in
         * the received beacon
         *================================================================
         */
        if (me_env.vht_supported)
        {
            vht_op_addr = mac_ie_find(bcn_addr + MAC_BEACON_VARIABLE_PART_OFT,
                                      length - MAC_BEACON_VARIABLE_PART_OFT,
                                      MAC_ELTID_VHT_OPERATION);
        }
#endif
    }
    me_bw_check(ht_op_addr, vht_op_addr, &vif->bss_info);

    // Check for channel switch
    csa_count = me_extract_csa(bcn_addr + MAC_BEACON_VARIABLE_PART_OFT,
                               length - MAC_BEACON_VARIABLE_PART_OFT,
                               &csa_mode, &vif->csa_channel);
    if (csa_count)
    {
        if (vif->type == VIF_STA)
        {
            if ((!vif->u.sta.csa_count) &&
                    (csa_mode == MAC_SWITCH_MODE_TX_TRAFFIC_STOP))
            {
                mm_send_csa_traffic_ind(vif->index, false);
            }
            vif->u.sta.csa_count = csa_count;
        }
#if NX_BCN_AUTONOMOUS_TX
        else if (vif->type == VIF_AP) // if not defined this function is not called for AP itf
        {
            vif->u.ap.csa_count = csa_count;
        }
#endif
    }

    // Check if power constraint changed
    me_extract_power_constraint(bcn_addr + MAC_BEACON_VARIABLE_PART_OFT,
                                length - MAC_BEACON_VARIABLE_PART_OFT,
                                &vif->bss_info);

    if (cur_pwr_const != vif->bss_info.power_constraint)
    {
        int8_t pwr;
        uint8_t idx;
        pwr = vif->bss_info.chan->tx_power - vif->bss_info.power_constraint;
        tpc_update_vif_tx_power(vif, &pwr, &idx);
    }

    // Check if we need to change the channel context
    if (cur_bw < vif->bss_info.bw)
    {
        me_chan_ctxt_update(vif);
    }

    // Update the protection and BW of STA linked to this VIF
    while (sta != NULL)
    {
        if (cur_prot_status != vif->bss_info.prot_status)
        {
            // Update Policy Table for Protection Frames
            me_pol_tbl_prot_upd(sta, vif->bss_info.prot_status);
        }

        if (cur_bw != vif->bss_info.bw)
        {
            me_pol_tbl_bw_upd(sta, vif->bss_info.bw);
        }

        sta = (struct sta_info_tag *)co_list_next(&sta->list_hdr);
    }
}

void me_sta_bw_nss_max_upd(uint8_t sta_idx, uint8_t bw, uint8_t nss)
{
    struct sta_info_tag *sta_info = &sta_info_tab[sta_idx];
    struct sta_pol_tbl_cntl *pol_tbl = &sta_info->pol_tbl;
    struct mac_sta_info *info = &sta_info->info;
    struct vif_info_tag *vif = &vif_info_tab[sta_info->inst_nbr];
    uint8_t local_supp_bw = BW_20MHZ;
    uint8_t peer_supp_bw = BW_20MHZ;
    uint8_t local_supp_nss = 0;
    uint8_t peer_supp_nss = 0;

#if NX_VHT
    if (info->capa_flags & STA_VHT_CAPA)
    {
        ASSERT_ERR(me_env.vht_supported);
        // Check the peer and local supported BW (VHT)
        if (info->vht_cap.vht_capa_info & MAC_VHTCAPA_SUPP_CHAN_WIDTH_MSK)
            peer_supp_bw = BW_160MHZ;
        else
            peer_supp_bw = BW_80MHZ;

        if (me_env.vht_cap.vht_capa_info & MAC_VHTCAPA_SUPP_CHAN_WIDTH_MSK)
            local_supp_bw = BW_160MHZ;
        else
            local_supp_bw = BW_80MHZ;

        // Check the peer and local supported NSS (VHT)
        peer_supp_nss = me_11ac_nss_max(info->vht_cap.rx_mcs_map);
        local_supp_nss = me_11ac_nss_max(me_env.vht_cap.tx_mcs_map);

        nss = co_min(nss, co_min(local_supp_nss, peer_supp_nss));
    }
    else
#endif
    {
        ASSERT_ERR(me_env.ht_supported);
        // Check the peer supported BW (HT)
        if (info->ht_cap.ht_capa_info & MAC_HTCAPA_40_MHZ)
            peer_supp_bw = BW_40MHZ;

        // Check our local supported BW
        if (me_env.ht_cap.ht_capa_info & MAC_HTCAPA_40_MHZ)
            local_supp_bw = BW_40MHZ;

        // Check the peer and local supported NSS (VHT)
        peer_supp_nss = me_11n_nss_max(info->ht_cap.mcs_rate);
        local_supp_nss = me_11n_nss_max(me_env.ht_cap.mcs_rate);

        nss = co_min(nss, co_min(local_supp_nss, peer_supp_nss));
    }
    bw = co_min(bw, peer_supp_bw);

    info->bw_max = co_min(local_supp_bw, bw);
    info->bw_cur = co_min(info->bw_max, vif->bss_info.bw);

    // Check if the bandwidth needs to be changed
#if RC_ENABLE
    // Update max BW allowed in the RC algorithm
    rc_update_bw_nss_max(sta_info->staid, info->bw_cur, nss);
#endif

    // Require update of protection configuration field in policy table
    pol_tbl->upd_field |= CO_BIT(STA_MGMT_POL_UPD_BW);
}

#if RC_ENABLE
uint16_t me_tx_cfm_amsdu(struct txdesc *txdesc)
{
    struct hostdesc *host = &txdesc->host;
    struct sta_info_tag *sta_entry = &sta_info_tab[host->staid];

    if (host->tid == 0xff)
        return 0;

    if (sta_entry->inst_nbr == 0xFF)
        return 0;

#if NX_AMPDU_TX
    // AMSDU allowed only under BA (because of retry)
    if (sta_entry->ba_info[host->tid].bam_idx_tx == BAM_INVALID_TASK_IDX)
        return 0;

    if (!bam_env[sta_entry->ba_info[host->tid].bam_idx_tx].amsdu)
        return 0;
#endif

    // Get the max aggregation size
    return rc_get_max_amsdu_len(sta_entry);
}
#else
uint16_t me_tx_cfm_amsdu(struct txdesc *txdesc)
{
    struct hostdesc *host = &txdesc->host;
    struct sta_info_tag *sta_entry = &sta_info_tab[host->staid];
    struct mac_sta_info *info = &sta_entry->info;
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;

    if (host->tid == 0xff)
        return 0;

    // Only allow A-MSDU for high rate
    if (rc->r_idx < rc->r_idx_max)
        return 0;

#if NX_AMPDU_TX
    // AMSDU allowed only under BA (because of retry)
    if (sta_entry->ba_info[host->tid].bam_idx_tx == BAM_INVALID_TASK_IDX)
        return 0;

    if (!bam_env[sta_entry->ba_info[host->tid].bam_idx_tx].amsdu)
        return 0;
#endif

    if (info->capa_flags & STA_VHT_CAPA)
    {
        int max_mpdu_head_tail = MAC_LONG_QOS_HTC_MAC_HDR_LEN + MAC_FCS_LEN;
#if RW_WAPI_EN
        max_mpdu_head_tail += WPI_IV_LEN + WPI_MIC_LEN;
#else
        max_mpdu_head_tail += IV_LEN + EIV_LEN + MIC_LEN + ICV_LEN;
#endif

        switch (info->vht_cap.vht_capa_info & MAC_VHTCAPA_MAX_MPDU_LENGTH_MSK)
        {
        case MAC_VHTCAPA_MAX_MPDU_LENGTH_3895:
            return 3895 - max_mpdu_head_tail;
        case MAC_VHTCAPA_MAX_MPDU_LENGTH_7991:
            return 7991 - max_mpdu_head_tail;
        case MAC_VHTCAPA_MAX_MPDU_LENGTH_11454:
            return 11454 - max_mpdu_head_tail;
        }
    }
    else if (info->capa_flags & STA_HT_CAPA)
    {
        if (info->ht_cap.ht_capa_info & MAC_HTCAPA_AMSDU)
#if NX_AMPDU_TX
            return 4095;
#else
            return 7935;
#endif
        else
            return 3839;
    }

    return 0;
}
#endif

uint8_t me_add_chan_ctx(uint8_t *p_chan_idx, struct scan_chan_tag *p_chan,
                        uint32_t center_freq1, uint32_t center_freq2, uint8_t ch_width)
{
    struct mm_chan_ctxt_add_req req;

    // Fill-in the channel context addition request
    req.band         = p_chan->band;
    req.type         = ch_width;
    req.prim20_freq  = p_chan->freq;
    req.center1_freq = center_freq1;
    req.center2_freq = center_freq2;
    req.tx_power     = p_chan->tx_power;

    // Add the channel
    return (chan_ctxt_add(&req, p_chan_idx));
}
// eof

