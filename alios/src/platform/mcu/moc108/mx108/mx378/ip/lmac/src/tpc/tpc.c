/**
 ******************************************************************************
 *
 * @file tpc.c
 *
 * @brief TPC (Transmit Power Control) module.
 *
 * Copyright (C) RivieraWaves 2016
 *
 ******************************************************************************
 */

#include "tpc.h"

void tpc_update_tx_power(int8_t pwr)
{
    uint8_t idx = 0;

    phy_get_rf_gain_idx(&pwr, &idx);

    nxmac_max_power_level_set((uint32_t)idx << NXMAC_DSSS_MAX_PWR_LEVEL_LSB 
								|(uint32_t)idx << NXMAC_OFDM_MAX_PWR_LEVEL_LSB);
}

uint8_t tpc_get_vif_tx_power(struct vif_info_tag *vif)
{
    uint8_t idx;
    int8_t pwr;

    if (vif->tx_power != VIF_UNDEF_POWER)
    {
        if (vif->user_tx_power < vif->tx_power)
            pwr = vif->user_tx_power;
        else
            pwr = vif->tx_power;
        phy_get_rf_gain_idx(&pwr, &idx);
        return idx;
    }

    return nxmac_ofdm_max_pwr_level_getf();
}

void tpc_update_vif_tx_power(struct vif_info_tag *vif, int8_t *pwr, uint8_t *idx)
{
    int8_t prev_pwr = vif->tx_power;

    if (*pwr == VIF_UNDEF_POWER) {
        return;
    }

    phy_get_rf_gain_idx(pwr, idx);
    vif->tx_power = *pwr;

    if (vif->user_tx_power < *pwr)
    {
        *pwr = vif->user_tx_power;
        phy_get_rf_gain_idx(pwr, idx);
    }

    if (prev_pwr != *pwr)
    {
        /* Mark all sta associated to the vif, in order to update policy table
           with new tx power on next transmit */
        struct co_list_hdr *sta_hdr;
        sta_hdr = co_list_pick(&vif->sta_list);
        while (sta_hdr != NULL)
        {
            struct sta_info_tag *sta;
            struct sta_pol_tbl_cntl *rc;

            sta = (struct sta_info_tag *)sta_hdr;
            rc = &sta->pol_tbl;
            rc->upd_field |= CO_BIT(STA_MGMT_POL_UPD_TX_POWER);
            sta_hdr = co_list_next(sta_hdr);
        }

        #if NX_CHNL_CTXT
        if (vif->chan_ctxt)
        {
            chan_update_tx_power(vif->chan_ctxt);
            if (chan_is_on_channel(vif))
            {
                tpc_update_tx_power(vif->chan_ctxt->channel.tx_power);
            }
        }
        #else
        /* Without channel context always update power */
        int i;

        for (i = 0; i < NX_VIRT_DEV_MAX; i++) {
            if (vif_info_tab[i].active && vif_info_tab[i].tx_power < *pwr)
                *pwr = vif_info_tab[i].tx_power;
        }

        tpc_update_tx_power(*pwr);
        #endif /* NX_CHNL_CTXT */
    }
}

#if NX_TX_FRAME
void tpc_update_frame_tx_power(struct vif_info_tag *vif, struct txl_frame_desc_tag *frame)
{
    struct tx_policy_tbl *pol;
    uint8_t idx;

    idx = tpc_get_vif_tx_power(vif);
    pol = HW2CPU(frame->txdesc.lmac.hw_desc->thd.policyentryaddr);
    pol->powercntrlinfo[0] = idx << TX_PWR_LEVEL_PT_RCX_OFT;
}
#endif /* NX_TX_FRAME */
