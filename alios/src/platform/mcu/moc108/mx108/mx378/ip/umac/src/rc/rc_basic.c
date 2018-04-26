/**
 ****************************************************************************************
 *
 * @file rc_basic.c
 *
 * @brief The Basic Rate Control module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

#include "me.h"
#include "mm.h"
#include "me_utils.h"

#if !RC_ENABLE
#include "rc_basic.h"
void rc_basic_check(uint8_t sta_idx)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;

    // Check if it is time to compute the PER
    if (rc->tx_cnt > ME_RC_PERIOD)
    {
        uint32_t tx_cnt = rc->tx_cnt;
        uint32_t tx_fail = rc->fail_cnt;
        uint32_t ratio;

        // Check if the failure count is NULL
        if (tx_fail == 0)
            tx_fail = 1;

        // Compute the ratio between attempts and failures
        ratio = tx_cnt/tx_fail;

        // Check if we need to do something
        if ((ratio < ME_UTILS_RC_DEC_RATIO) && (rc->r_idx > rc->r_idx_min))
        {
            rc->r_idx--;

            // Keep in mind we have to update the rate
            rc->upd_field |= CO_BIT(STA_MGMT_POL_UPD_RATE);
        }
        else if ((ratio > ME_UTILS_RC_INC_RATIO) && (rc->r_idx < rc->r_idx_max))
        {
            rc->r_idx++;

            // Keep in mind we have to update the rate
            rc->upd_field |= CO_BIT(STA_MGMT_POL_UPD_RATE);
        }

        // Reset the statistics
        rc->tx_cnt = 0;
        rc->fail_cnt = 0;
    }
}

void rc_basic_init(struct sta_info_tag *sta_entry)
{
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    struct tx_policy_tbl *pol = &rc->buf_ctrl[0]->policy_tbl;
    struct tx_policy_tbl *pol1 = &rc->buf_ctrl[1]->policy_tbl;
    uint8_t hw_key_idx = MM_STA_TO_KEY(sta_entry->staid);
    uint32_t rate_info;
    uint32_t phy_cntrl_info1 = phy_get_ntx() << NX_TX_PT_OFT;
    uint8_t nss_max = 0;
    int i;

    // Check if the peer is a 11n or 11ac device
    if (sta_entry->info.capa_flags & STA_HT_CAPA)
    {
        struct mac_htcapability *htcap = &sta_entry->info.ht_cap;

        #if NX_VHT
        if (sta_entry->info.capa_flags & STA_VHT_CAPA)
        {
            struct mac_vhtcapability *vhtcap = &sta_entry->info.vht_cap;
            struct mac_vhtcapability *vhtcaploc = &me_env.vht_cap;

            nss_max = co_min(me_11ac_nss_max(vhtcap->rx_mcs_map),
                             me_11ac_nss_max(vhtcaploc->tx_mcs_map));

            rate_info = FORMATMOD_VHT << FORMAT_MOD_TX_RCX_OFT;
            rc->r_idx_min = nss_max << VHT_NSS_OFT;
            rc->r_idx_max = co_min(me_11ac_mcs_max(vhtcap->rx_mcs_map),
                                   me_11ac_mcs_max(vhtcaploc->tx_mcs_map))
                         | (nss_max << VHT_NSS_OFT);
            if ((vhtcaploc->vht_capa_info & MAC_VHTCAPA_RXLDPC) &&
                (vhtcap->vht_capa_info & MAC_VHTCAPA_RXLDPC))
            {
                phy_cntrl_info1 |= FEC_CODING_PT_BIT;
            }
        }
        else
        #endif
        {
            struct mac_htcapability *htcaploc = &me_env.ht_cap;

            nss_max = co_min(me_11n_nss_max(htcap->mcs_rate),
                             me_11n_nss_max(htcaploc->mcs_rate));

            rate_info = FORMATMOD_HT_MF << FORMAT_MOD_TX_RCX_OFT;
            rc->r_idx_min = nss_max << HT_NSS_OFT;
            rc->r_idx_max = 7 | (nss_max << HT_NSS_OFT);

            if ((htcaploc->ht_capa_info & MAC_HTCAPA_LDPC) &&
                (htcap->ht_capa_info & MAC_HTCAPA_LDPC))
            {
                phy_cntrl_info1 |= FEC_CODING_PT_BIT;
            }
        }

        rate_info |= sta_entry->info.bw_cur << BW_TX_RCX_OFT;
        switch(sta_entry->info.bw_cur)
        {
            case BW_20MHZ:
                if (htcap->ht_capa_info & MAC_HTCAPA_SHORTGI_20)
                    rate_info |= SHORT_GI_TX_RCX_MASK;
                break;
            case BW_40MHZ:
                if (htcap->ht_capa_info & MAC_HTCAPA_SHORTGI_40)
                    rate_info |= SHORT_GI_TX_RCX_MASK;
                break;
            #if NX_VHT
            case BW_80MHZ:
                if (sta_entry->info.vht_cap.vht_capa_info & MAC_VHTCAPA_SHORT_GI_80)
                    rate_info |= SHORT_GI_TX_RCX_MASK;
                break;
            case BW_160MHZ:
                if (sta_entry->info.vht_cap.vht_capa_info & MAC_VHTCAPA_SHORT_GI_160)
                    rate_info |= SHORT_GI_TX_RCX_MASK;
                break;
            #endif
            default:
                break;
        }
    }
    else
    {
        rate_info = 0;
        if (((sta_entry->info.rate_set.array[0] & ~MAC_BASIC_RATE) < MAC_RATE_6MBPS) ||
            ((sta_entry->info.rate_set.array[0] & ~MAC_BASIC_RATE) == MAC_RATE_11MBPS))
        {
            rc->r_idx_min = 0;
        }
        else
        {
            rc->r_idx_min = HW_RATE_6MBPS;
        }

        if ((sta_entry->info.rate_set.array[sta_entry->info.rate_set.length - 1]
                                                  & ~MAC_BASIC_RATE) > MAC_RATE_11MBPS)
        {
            rc->r_idx_max = HW_RATE_54MBPS;
        }
        else
        {
            rc->r_idx_max = HW_RATE_11MBPS;
        }
    }

    rate_info |= 1 << N_RETRY_RCX_OFT;
    rc->r_idx = HW_RATE_1MBPS;//rc->r_idx_max; yhb changed. set init data rate to 1Mbps, make eapol & dhcp work at 1Mbps.
    rc->fail_cnt = 0;
    rc->tx_cnt = 0;
    rc->buf_ctrl_idx = 0;

    for (i = 0; i < RATE_CONTROL_STEPS; i++)
    {
        pol->ratecntrlinfo[i] = rate_info;
    }
    pol->maccntrlinfo1 = hw_key_idx << KEYSRAM_INDEX_RA_OFT;
    pol1->maccntrlinfo1 = pol->maccntrlinfo1;

    pol->phycntrlinfo1  = phy_cntrl_info1;
    pol1->phycntrlinfo1 = pol->phycntrlinfo1;

    pol->phycntrlinfo2  = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
    pol1->phycntrlinfo2 = pol->phycntrlinfo2;

    rc->upd_field |= CO_BIT(STA_MGMT_POL_UPD_RATE);
}

#endif

