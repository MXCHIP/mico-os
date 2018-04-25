/**
 ****************************************************************************************
 *
 * @file me_mgmtframe.c
 *
 * @brief All functions required either to build or to extract data from air frames.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */
#include "me_mgmtframe.h"
#include "mac_defs.h"
#include "mac_frame.h"
#include "mac_common.h"
#include "mac_ie.h"
#include "mac.h"
#include "me.h"
#include "sm_task.h"
#include "ps.h"
#include "co_endian.h"
#include "co_utils.h"
#include "me_utils.h"
#include "sta_mgmt.h"


/// Default listen interval (if no listen interval is proposed by the host)
#define ME_DEFAULT_LISTEN_INTERVAL   5

// Functions used in the BAM
static void me_add_baparamset_frame(uint32_t addr, struct bam_env_tag *bam_env)
{
    uint16_t value = 0;

    /* The Block Ack Parameter Set field
     *
     *******************************************************************
     *  B0              *  B1               * (B2 -> B5)*  (B6->B15)   *
     *  A-MSD Supported *  Block Ack Policy * TID       *  Buffer Size *
     *******************************************************************
     *
     */

    // Fill the A-MSDU Supported bit
    // TODO[LT] - Set A-MSDU Supported bit
    //    value = mib_AggregationConfigTable.AllowAMsduWithBA[tid_info_tab[bam_env->tid].access_category];
    value |= 0;
    // Fill the Block Ack Policy bit
    value |= (bam_env->ba_policy << MAC_BA_PARAMSET_POLICY_OFT);
    // Fill the TID bits
    value |= (bam_env->tid << MAC_BA_PARAMSET_TID_OFT);
    // Fill the Buffer Size bits
    value |= (bam_env->buffer_size << MAC_BA_PARAMSET_BUFFER_OFT);

    co_write16p(addr, co_htows(value));
}

static void me_add_ba_ssc_frame(uint32_t addr, struct bam_env_tag *bam_env)
{
    co_write16p(addr, co_htows(bam_env->ssn << MAC_BA_BARINFO_SEQ_STARTSEQ_OFT));
}

static void me_del_baparamset_frame(uint32_t addr, struct bam_env_tag *bam_env)
{
    /* The DELBA Parameter Set field
     *
     ******************************************
     *  B0->B10   *   B11       * (B12 -> B15)*
     *  Reserved  *  Initiator  * TID         *
     ******************************************
     *
     */
    uint16_t value = 0;

    // Set the value of the TID
    value |= bam_env->tid << BAM_DELBA_PARAM_TID_OFFSET;

    if (bam_env->dev_type == BA_ORIGINATOR)
    {
        value |= BAM_DELBA_PARAM_INITIATOR_MASK;
    }

    co_write16p(addr, co_htows(value));
}

/**
 * Information Element Management
 ****************************************************************************************
 */

uint32_t me_add_ie_ssid(uint32_t *frame_addr, uint8_t ssid_len, uint8_t *p_ssid)
{
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + ssid_len;

    // Tag Number - SSID
    co_write8p(*frame_addr + MAC_INFOELT_ID_OFT, MAC_ELTID_SSID);
    // Tag Length
    co_write8p(*frame_addr + MAC_INFOELT_LEN_OFT, ssid_len);

    if (ssid_len != 0)
    {
        // Copy the SSID
        co_pack8p(*frame_addr + MAC_INFOELT_INFO_OFT, (uint8_t *)p_ssid, ssid_len);
    }

    // Update the position in the frame
    *frame_addr += ie_len;

    // And the written length
    return (ie_len);
}

uint32_t me_add_ie_supp_rates(uint32_t *frame_addr, struct mac_rateset *p_rateset)
{
    // Length for Supported Rates - Only up to 8 rates can be added in the supported rate element
    uint32_t supp_len = co_min(p_rateset->length, MAC_SUPPORTED_RATES_LEN);
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + supp_len;

    // Tag Number - Supported Rates
    co_write8p(*frame_addr + MAC_INFOELT_ID_OFT, MAC_ELTID_RATES);
    // Tag Length
    co_write8p(*frame_addr + MAC_INFOELT_LEN_OFT, supp_len);

    // Copy the rates
    co_pack8p(*frame_addr + MAC_INFOELT_INFO_OFT, p_rateset->array, supp_len);

    // Update the frame pointer
    *frame_addr += ie_len;

    // And the written length
    return (ie_len);
}

uint32_t me_add_ie_ext_supp_rates(uint32_t *frame_addr, struct mac_rateset *p_rateset)
{
    // Length for Extended Supported Rates
    uint32_t ext_supp_len = p_rateset->length - MAC_SUPPORTED_RATES_LEN;
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + ext_supp_len;

    // Tag Number - Extended Supported Rates
    co_write8p(*frame_addr + MAC_INFOELT_ID_OFT, MAC_ELTID_EXT_RATES);
    // Tag Length
    co_write8p(*frame_addr + MAC_INFOELT_LEN_OFT, ext_supp_len);

    // Copy the rates
    co_pack8p(*frame_addr + MAC_INFOELT_INFO_OFT, &p_rateset->array[MAC_SUPPORTED_RATES_LEN],
              ext_supp_len);

    // Update the frame pointer
    *frame_addr += ie_len;

    // And the written length
    return (ie_len);
}

#if (RW_MESH_EN)
uint32_t me_add_ie_tim(uint32_t *frame_addr, uint8_t dtim_period)
{
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + 4;

    // Tag Number - TIM
    co_write8p(*frame_addr + MAC_INFOELT_ID_OFT, MAC_ELTID_TIM);
    // Tag Length
    co_write8p(*frame_addr + MAC_INFOELT_LEN_OFT, 4);

    // Write DTIM Period
    co_write8p(*frame_addr + MAC_TIM_PERIOD_OFT, dtim_period);
    // Other parameters will be updated in mm_bcn module before beacon transmission

    // Update the position in the frame
    *frame_addr += ie_len;

    // And the written length
    return (ie_len);
}

uint32_t me_add_ie_dsss_param(uint32_t *frame_addr, uint8_t chan)
{
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + 1;

    // Tag Number - DS Parameter Set
    co_write8p(*frame_addr + MAC_INFOELT_ID_OFT, MAC_ELTID_DS);
    // Tag Length
    co_write8p(*frame_addr + MAC_INFOELT_LEN_OFT, 1);

    // Copy the Channel
    co_write8p(*frame_addr + MAC_INFOELT_INFO_OFT, chan);

    // Update the position in the frame
    *frame_addr += ie_len;

    // And return the written length
    return (ie_len);
}
#endif //(RW_MESH_EN)

uint32_t me_add_ie_ht_capa(uint32_t *frame_addr)
{
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + MAC_HT_CAPA_ELMT_LEN;
    // Get Local HT Capabilities
    struct mac_htcapability *p_ht_cap = &me_env.ht_cap;
    uint16_t ht_capa_info = p_ht_cap->ht_capa_info & ~MAC_HTCAPA_SMPS_MSK;

    // Tag Number - HT Capabilities
    co_write8p(*frame_addr + MAC_HT_CAPA_ID_OFT, MAC_ELTID_HT_CAPA);
    // Tag Length
    co_write8p(*frame_addr + MAC_HT_CAPA_LEN_OFT, MAC_HT_CAPA_ELMT_LEN);

    ht_capa_info |= MAC_HTCAPA_SMPS_DISABLE;
    co_write16p(*frame_addr + MAC_HT_CAPA_INFO_OFT, co_htows(ht_capa_info));
    co_write8p(*frame_addr + MAC_HT_CAPA_AMPDU_PARAM_OFT, p_ht_cap->a_mpdu_param);
    co_pack8p(*frame_addr + MAC_HT_CAPA_SUPPORTED_MCS_SET_OFT, p_ht_cap->mcs_rate,
              MAX_MCS_LEN);
    co_write16p(*frame_addr + MAC_HT_CAPA_EXTENDED_CAPA_OFT, co_htows(p_ht_cap->ht_extended_capa));
    co_pack8p(*frame_addr + MAC_HT_CAPA_TX_BEAM_FORMING_CAPA_OFT,
              (uint8_t *)&p_ht_cap->tx_beamforming_capa, 4);
    co_write8p(*frame_addr + MAC_HT_CAPA_ASEL_CAPA_OFT, p_ht_cap->asel_capa);

    *frame_addr += ie_len;

    // And return the written length
    return (ie_len);
}
#if (RW_MESH_EN)
uint32_t me_add_ie_ht_oper(uint32_t *frame_addr, struct vif_info_tag *p_vif_entry)
{
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + MAC_HT_OPER_ELMT_LEN;
    // Get Channel Context used by the VIF
    struct chan_ctxt_tag *p_chan_ctxt = p_vif_entry->chan_ctxt;
    // HT Information Subset (1 of 3)
    uint8_t ht_info1 = 0x00;

    // Tag Number - HT Information
    co_write8p(*frame_addr + MAC_HT_OPER_ID_OFT, MAC_ELTID_HT_OPERATION);
    // Tag Length
    co_write8p(*frame_addr + MAC_HT_OPER_LEN_OFT, MAC_HT_OPER_ELMT_LEN);

    co_write8p(*frame_addr + MAC_HT_OPER_PRIM_CH_OFT,
               (uint8_t)phy_freq_to_channel(p_chan_ctxt->channel.band,
                                            p_chan_ctxt->channel.prim20_freq));

    if (p_chan_ctxt->channel.type > BW_20MHZ)
    {
        // Allow use of any channel width
        ht_info1 |= 0x04;

        if (p_chan_ctxt->channel.prim20_freq > p_chan_ctxt->channel.center1_freq)
        {
            // Secondary channel is below the primary channel
            ht_info1 |= 0x03;
        }
        else
        {
            // Secondary channel is above the primary channel
            ht_info1 |= 0x01;
        }
    }

    co_write8p(*frame_addr + MAC_HT_OPER_INFO_OFT, ht_info1);
    co_write16p(*frame_addr + MAC_HT_OPER_INFO_SUBSET2_OFT, MAC_HT_OPER_OP_MODE_MASK);
    co_write16p(*frame_addr + MAC_HT_OPER_INFO_SUBSET3_OFT, 0);
    co_write8p(*frame_addr + MAC_HT_OPER_BASIC_MSC_SET_OFT, 0xFF);

    *frame_addr += ie_len;

    // And return the written length
    return (ie_len);
}
#endif //(RW_MESH_EN)

#if (NX_VHT)
uint32_t me_add_ie_vht_capa(uint32_t *frame_addr)
{
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + MAC_VHT_CAPA_ELMT_LEN;
    // Get Local VHT Capabilities
    struct mac_vhtcapability *p_vht_cap = &me_env.vht_cap;

    // Tag Number - VHT Capabilities
    co_write8p(*frame_addr + MAC_VHT_CAPA_ID_OFT, MAC_ELTID_VHT_CAPA);
    // Tag Length
    co_write8p(*frame_addr + MAC_VHT_CAPA_LEN_OFT, MAC_VHT_CAPA_ELMT_LEN);

    co_write32p(*frame_addr + MAC_VHT_CAPA_INFO_OFT, co_htowl(p_vht_cap->vht_capa_info));
    co_write16p(*frame_addr + MAC_VHT_RX_MCS_MAP_OFT, co_htows(p_vht_cap->rx_mcs_map));
    co_write16p(*frame_addr + MAC_VHT_RX_HIGHEST_RATE_OFT, co_htows(p_vht_cap->rx_highest));
    co_write16p(*frame_addr + MAC_VHT_TX_MCS_MAP_OFT, co_htows(p_vht_cap->tx_mcs_map));
    co_write16p(*frame_addr + MAC_VHT_TX_HIGHEST_RATE_OFT, co_htows(p_vht_cap->tx_highest));

    *frame_addr += ie_len;

    // And return the written length
    return (ie_len);
}

#if (RW_MESH_EN)
uint32_t me_add_ie_vht_oper(uint32_t *frame_addr, struct vif_info_tag *p_vif_entry)
{
    // Element length
    uint32_t ie_len = MAC_INFOELT_INFO_OFT + MAC_VHT_OPER_ELMT_LEN;
    // Get Channel Context used by the VIF
    struct chan_ctxt_tag *p_chan_ctxt = p_vif_entry->chan_ctxt;

    // Tag Number - VHT Operation
    co_write8p(*frame_addr + MAC_VHT_OPER_ID_OFT, MAC_ELTID_VHT_OPERATION);
    // Tag Length
    co_write8p(*frame_addr + MAC_VHT_OPER_LEN_OFT, MAC_VHT_OPER_ELMT_LEN);

    if (p_chan_ctxt->channel.type == BW_80MHZ)
    {
        co_write8p(*frame_addr + MAC_VHT_CHAN_WIDTH_OFT, 1);
        co_write8p(*frame_addr + MAC_VHT_CENTER_FREQ0_OFT,
                   (uint8_t)phy_freq_to_channel(p_chan_ctxt->channel.band,
                                                p_chan_ctxt->channel.center1_freq));
    }
    else
    {
        co_write8p(*frame_addr + MAC_VHT_CHAN_WIDTH_OFT, 0);
        co_write8p(*frame_addr + MAC_VHT_CENTER_FREQ0_OFT, 0);
    }

    co_write8p(*frame_addr + MAC_VHT_CENTER_FREQ1_OFT, 0);
    co_write16p(*frame_addr + MAC_VHT_BASIC_MCS_OFT, 0);

    *frame_addr += ie_len;

    // And return the written length
    return (ie_len);
}
#endif //(RW_MESH_EN)
#endif //(NX_VHT)

uint16_t me_build_authenticate(uint32_t frame,
                               uint16_t algo_type,
                               uint16_t seq_nbr,
                               uint16_t status_code,
                               uint32_t *challenge_array_ptr)
{

    uint16_t mac_frame_len;

    co_write16p(frame + MAC_AUTH_ALGONBR_OFT, co_htows(algo_type));
    co_write16p(frame + MAC_AUTH_SEQNBR_OFT, co_htows(seq_nbr));
    co_write16p(frame + MAC_AUTH_STATUS_OFT, co_htows(status_code));
    mac_frame_len = MAC_AUTH_CHALLENGE_OFT;

    if (challenge_array_ptr != NULL)
    {
        frame += MAC_AUTH_CHALLENGE_OFT;
        co_write8p(frame + MAC_CHALLENGE_ID_OFT, MAC_ELTID_CHALLENGE);
        co_write8p(frame + MAC_CHALLENGE_LEN_OFT, MAC_AUTH_CHALLENGE_LEN);
        co_copy8p(frame + MAC_CHALLENGE_TEXT_OFT, CPU2HW(challenge_array_ptr),
                  MAC_AUTH_CHALLENGE_LEN);
        mac_frame_len += CHALLENGE_TEXT_SIZE;
    }
    return mac_frame_len;
}

uint16_t me_build_deauthenticate(uint32_t frame, uint16_t reason_code)
{
    uint16_t mac_frame_len;

    co_write16p(frame + MAC_DISASSOC_REASON_OFT, co_htows(reason_code));
    mac_frame_len = MAC_DISASSOC_REASON_LEN;

    return(mac_frame_len);
}

// Functions used in the SM
uint16_t me_build_associate_req(uint32_t frame,
                                struct mac_bss_info *bss,
                                struct mac_addr *old_ap_addr_ptr,
                                uint8_t vif_idx,
                                uint32_t *ie_addr,
                                uint16_t *ie_len,
                                struct sm_connect_req const *con_par)
{
    uint16_t mac_frame_len;
    uint16_t capainfo = 0;
    uint32_t add_ie_addr = CPU2HW(con_par->ie_buf);
    uint16_t add_ie_len = con_par->ie_len;
    uint16_t listen;
#if NX_UAPSD
    uint8_t uapsd_info = con_par->uapsd_queues;
#endif

    // Set the listen interval based on the parameter from the host
    listen = con_par->listen_interval ? con_par->listen_interval : ME_DEFAULT_LISTEN_INTERVAL;

    // build the capability information
    capainfo = me_build_capability(vif_idx);

    // build the frame body
    co_write16p(frame + MAC_ASSO_REQ_CAPA_OFT, co_htows(capainfo));
    co_write16p(frame + MAC_ASSO_REQ_LISTEN_OFT, co_htows(listen));

    if (old_ap_addr_ptr)
    {
        // Add current AP if this is a re-association request
        MAC_ADDR_CPY(HW2CPU(frame + MAC_REASSO_REQ_AP_ADDR_OFT), old_ap_addr_ptr);

        // update frame length
        mac_frame_len = MAC_REASSO_REQ_SSID_OFT;
    }
    else
    {
        // update frame length
        mac_frame_len = MAC_ASSO_REQ_SSID_OFT;
    }

    frame += mac_frame_len;

    *ie_addr = frame;

    // Update the SSID
    mac_frame_len += me_add_ie_ssid(&frame, bss->ssid.length, (uint8_t *)&bss->ssid.array);
    // Add supported rates
    mac_frame_len += me_add_ie_supp_rates(&frame, &bss->rate_set);
    mac_frame_len += me_add_ie_ext_supp_rates(&frame, &bss->rate_set);
    
    if (capainfo & MAC_CAPA_SPECTRUM)
    {
        int8_t min, max;

        // Add Power capability
        co_write8p(frame++, MAC_ELTID_POWER_CAPABILITY);
        co_write8p(frame++, MAC_POWER_CAPABILITY_IE_LEN);
        phy_get_rf_gain_capab(&min, &max);
        if (max > bss->chan->tx_power)
        {
            max = bss->chan->tx_power;
        }
        co_write8p(frame++, (uint8_t)min);
        co_write8p(frame++, (uint8_t)max);
        mac_frame_len += MAC_POWER_CAPABILITY_IE_LEN + 2;
    }

    if (capainfo & MAC_CAPA_SPECTRUM)
    {
        uint8_t start, prev, i, nb, ch_incr, ch_idx, len, cnt;
        struct scan_chan_tag *chan;
        uint32_t tmp;

        // Add Supported Channels
        co_write8p(frame++, MAC_ELTID_SUPPORTED_CHANNELS);
        tmp = frame++; // skip len
        if (bss->chan->band == PHY_BAND_2G4)
        {
            ch_incr = 1;
            chan = me_env.chan.chan2G4;
            cnt = me_env.chan.chan2G4_cnt;
        }
        else
        {
            ch_incr = 4;
            chan = me_env.chan.chan5G;
            cnt =  me_env.chan.chan5G_cnt;
        }

        nb = len = start = prev = 0;
        for (i = 0; i < cnt; i++)
        {
            if (chan->flags & SCAN_DISABLED_BIT)
                continue;

            ch_idx = phy_freq_to_channel(chan->band, chan->freq);
            if (nb && (ch_idx - prev) != ch_incr)
            {
                co_write8p(frame++, start);
                co_write8p(frame++, nb);
                nb++;
                len += 2;
            }
            else if (!nb)
            {
                start = ch_idx;
            }

            prev = ch_idx;
            nb++;
            chan++;
        }
        co_write8p(frame++, start);
        co_write8p(frame++, nb);
        len += 2;
        co_write8p(tmp, len);
        mac_frame_len += len + 2;
    }

    // Copy the additional information elements
    co_copy8p(frame, add_ie_addr, add_ie_len);
    mac_frame_len += add_ie_len;
    frame += add_ie_len;

    if (bss->valid_flags & BSS_QOS_VALID)
    {
        struct mac_raw_rsn_ie raw_wme_def_info = {MAC_RAW_WME_INFO_ELMT_DEFAULT};

#if NX_UAPSD
        // adjust the U-APSD flags as requested by the QSTA
        if (ps_uapsd_enabled() &&
                (bss->edca_param.qos_info & MAC_QOS_INFO_AP_UAPSD_ENABLED))
        {
            raw_wme_def_info.data[8] = uapsd_info;
        }
        else
#endif
        {
            raw_wme_def_info.data[8] = 0;
        }
        co_pack8p(frame, (uint8_t *)&raw_wme_def_info,
                  raw_wme_def_info.data[MAC_INFOELT_LEN_OFT] + MAC_INFOELT_INFO_OFT);

        // update the frame pointer
        mac_frame_len += raw_wme_def_info.data[MAC_INFOELT_LEN_OFT] + MAC_INFOELT_INFO_OFT;
        frame += raw_wme_def_info.data[MAC_INFOELT_LEN_OFT] + MAC_INFOELT_INFO_OFT;
    }

    // build the HT capabilities field
    if ((bss->valid_flags & BSS_HT_VALID) && me_env.ht_supported)
    {
        mac_frame_len += me_add_ie_ht_capa(&frame);
    }

    // add Mobility Domain field
    if (sm_env.ft_over_ds == 0)
    {
        struct mobility_domain *mde = &bss->mde;
        if (mde->mdid != 0)
        {
            co_write8p(frame + MAC_INFOELT_MDE_ID_OFT, MAC_ELTID_MDE);
            co_write8p(frame + MAC_INFOELT_MDE_LEN_OFT, MAC_INFOELT_MDE_ELMT_LEN);

            co_write16p(frame + MAC_INFOELT_MDE_MDID_OFT, co_htows(mde->mdid));
            co_write8p(frame + MAC_INFOELT_MDE_FT_CAPA_POL_OFT, mde->ft_capability_policy);

            mac_frame_len += MAC_INFOELT_MDE_LEN;
            frame += MAC_INFOELT_MDE_LEN;
        }
    }

#if NX_VHT
    // build the VHT capabilities field
    if ((bss->valid_flags & BSS_VHT_VALID) && me_env.vht_supported)
    {
        mac_frame_len += me_add_ie_vht_capa(&frame);
    }
#endif //(NX_VHT)

#ifdef COEXISTENCE_ELEMENT
    // 20-40 Coexistence element
    if (mib_dot11OperationTable[vlan_idx].dot112040BSSCoexistenceManagementSupport)
    {
        /* B0               B1              B2           B3         B4
         * Information      Forty MHz       20 MHz       OBSS       OBSS
         * Request          Intolerant      BSS Width    Scanning   Scanning
         *                                  Request      Request    Grant
         */
        frame->array[mac_frame_len + MAC_20_40_COEXISTENCE_ID_OFT]  = MAC_ELTID_20_40_COEXISTENCE;
        frame->array[mac_frame_len + MAC_20_40_COEXISTENCE_LEN_OFT] = MAC_20_40_COEXISTENCE_LEN;
        frame->array[mac_frame_len + MAC_20_40_COEXISTENCE_INFO_OFT] |=
            (mib_dot11OperationTable[vlan_idx].dot11FortyMHzIntolerant) ? MAC_20_40_COEXISTENCE_40_INTOLERANT_MASK : 0;

        mac_frame_len += MAC_20_40_COEXISTENCE_LEN;
        frame += MAC_20_40_COEXISTENCE_LEN;
    }
#endif

    *ie_len = frame - *ie_addr;

    return mac_frame_len;
}

uint16_t me_build_add_ba_req(uint32_t frame, struct bam_env_tag *bam_env)
{
    co_write8p(frame, MAC_BA_ACTION_CATEGORY);
    co_write8p(frame + MAC_ACTION_ACTION_OFT, MAC_BA_ACTION_ADDBA_REQ);
    co_write8p(frame + MAC_ACTION_TOKEN_OFT, bam_env->dialog_token);

    // Fill the BA Parameter field
    me_add_baparamset_frame(frame + BAM_ADDBAREQ_BA_PARAM_OFFSET, bam_env);

    co_write16p(frame + BAM_ADDBAREQ_BA_TIMEOUT_OFFSET, co_htows(bam_env->ba_timeout));

    // Add Block Ack Starting Sequence Control
    me_add_ba_ssc_frame(frame + BAM_ADDBAREQ_BA_SSC_OFFSET, bam_env);

    return (BAM_ADDBAREQ_LENGTH);
}

uint16_t me_build_add_ba_rsp(uint32_t frame, struct bam_env_tag *bam_env,
                             uint16_t param,
                             uint8_t dialog_token,
                             uint16_t status_code)
{
    co_write8p(frame, MAC_BA_ACTION_CATEGORY);
    co_write8p(frame + MAC_ACTION_ACTION_OFT, MAC_BA_ACTION_ADDBA_RSP);
    co_write8p(frame + MAC_ACTION_TOKEN_OFT, dialog_token);
    co_write16p(frame + BAM_ADDBARSP_STATUS_OFFSET, co_htows(status_code));

    // Fill the BA Parameter field
    co_write16p(frame + BAM_ADDBARSP_BA_PARAM_OFFSET, co_htows(param));

    if (status_code != MAC_BA_ST_SUCCESS)
        co_write16p(frame + BAM_ADDBARSP_BA_TIMEOUT_OFFSET, co_htows(0));
    else
        co_write16p(frame + BAM_ADDBARSP_BA_TIMEOUT_OFFSET, co_htows(bam_env->ba_timeout));

    return (BAM_ADDBARSP_LENGTH);
}

uint16_t me_build_del_ba(uint32_t frame, struct bam_env_tag *bam_env, uint16_t reason_code)
{
    co_write8p(frame, MAC_BA_ACTION_CATEGORY);
    co_write8p(frame + MAC_ACTION_ACTION_OFT, MAC_BA_ACTION_DELBA);

    // Add the Status
    me_del_baparamset_frame(frame + BAM_DELBA_PARAM_OFFSET, bam_env);

    // Add Reason Code value
    co_write16p(frame + BAM_DELBA_REASON_OFFSET, co_htows(reason_code));

    return (BAM_DELBA_LENGTH);
}


void me_extract_rate_set(uint32_t buffer,
                         uint16_t buflen,
                         struct mac_rateset *mac_rate_set_ptr)

{
    uint32_t elmt_ptr;
    uint8_t elmt_length;

    // By default we consider that no rates are found, set the length of the array to zero
    mac_rate_set_ptr->length = 0;

    do
    {
        // search the rate set IE in the frame
        elmt_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_RATES);
        if (elmt_ptr == 0)
            break;

        // the rate set IE is found, extract its length
        elmt_length = co_read8p(elmt_ptr + MAC_INFOELT_LEN_OFT);

        // Check if element length is valid
        if (elmt_length > MAC_RATESET_LEN)
            break;

        // copy the rate set from the frame
        co_unpack8p(mac_rate_set_ptr->array,
                    elmt_ptr + MAC_RATES_RATES_OFT,
                    elmt_length);

        mac_rate_set_ptr->length = elmt_length;

        // search for extended rates if any
        elmt_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_EXT_RATES);
        if(elmt_ptr == 0)
            break;

        // extended rates are found, update the length
        elmt_length = co_read8p(elmt_ptr + MAC_INFOELT_LEN_OFT);
        ASSERT_ERR (mac_rate_set_ptr->length + elmt_length <= MAC_RATESET_LEN);

        // Check if the extended rate set is not too long
        if (mac_rate_set_ptr->length + elmt_length > MAC_RATESET_LEN)
            break;

        // add the extended rates to the rate array
        co_unpack8p(&mac_rate_set_ptr->array[mac_rate_set_ptr->length],
                    elmt_ptr + MAC_RATES_RATES_OFT,
                    elmt_length);

        mac_rate_set_ptr->length += (uint8_t)elmt_length;
    }
    while(0);
}


void me_extract_power_constraint(uint32_t buffer, uint16_t buflen,
                                 struct mac_bss_info *bss)
{
    uint32_t elmt_ptr;

    elmt_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_POWER_CONSTRAINT);

    if (elmt_ptr == 0)
    {
        bss->power_constraint = 0;
        return;
    }

    bss->power_constraint = co_read8p(elmt_ptr + MAC_INFOELT_POWER_CONSTRAINT_OFT);
}

void me_extract_country_reg(uint32_t buffer, uint16_t buflen,
                            struct mac_bss_info *bss)
{
    uint32_t elmt_ptr;
    uint8_t bss_ch_idx, ch_incr, ch_idx, ch_nb, i;
    uint8_t elmt_length, oft;

    elmt_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_COUNTRY);

    if (elmt_ptr == 0)
    {
        return;
    }

    /* Only update tx power of the bss channel (required to set power
       capability in (re)assoc resp). Other channels will be updated by driver */
    if (bss->chan->band == PHY_BAND_2G4)
    {
        ch_incr = 1;
    }
    else
    {
        ch_incr = 4;
    }

    bss_ch_idx = phy_freq_to_channel(bss->chan->band, bss->chan->freq);
    elmt_length = co_read8p(elmt_ptr + MAC_COUNTRY_LEN_OFT);
    oft = MAC_COUNTRY_STRING_OFT + MAC_COUNTRY_STRING_LEN;
    while (oft < (elmt_length + 2))
    {
        ch_idx = co_read8p(elmt_ptr + oft + MAC_COUNTRY_FIRST_CHAN_OFT);
        ch_nb  = co_read8p(elmt_ptr + oft + MAC_COUNTRY_NB_CHAN_OFT);

        for (i = 0 ; i < ch_nb; i++)
        {
            if (bss_ch_idx == ch_idx)
            {
                bss->chan->tx_power = (int8_t)co_read8p(elmt_ptr + oft + MAC_COUNTRY_PWR_LEVEL_OFT);
                return;
            }
            ch_idx += ch_incr;
        }

        oft += MAC_COUNTRY_TRIPLET_LEN;
    }
}

void me_extract_mobility_domain(uint32_t buffer, uint16_t buflen,
                                struct mac_bss_info *bss)
{
    uint32_t elmt_ptr;

    elmt_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_MDE);

    if (elmt_ptr == 0)
    {
        bss->mde.mdid = 0;
        bss->mde.ft_capability_policy = 0;
        return;
    }

    bss->mde.mdid = co_read16p(elmt_ptr + MAC_INFOELT_MDE_MDID_OFT);
    bss->mde.ft_capability_policy = co_read8p(elmt_ptr + MAC_INFOELT_MDE_FT_CAPA_POL_OFT);
}

int me_extract_csa(uint32_t buffer, uint16_t buflen, uint8_t *mode,
                   struct mm_chan_ctxt_add_req *chan_desc)
{
    uint32_t csa_ptr, ecsa_ptr, sec_chan_ptr, cs_wrp_ptr, wide_bw_cs;
    uint8_t count, bw, band;
    uint16_t prim20 = 0, center1 = 0, center2 = 0;

    csa_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_CHANNEL_SWITCH);
    ecsa_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_EXT_CHANNEL_SWITCH);

    if (csa_ptr == 0 && ecsa_ptr == 0)
    {
        return 0;
    }

    sec_chan_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_SEC_CH_OFFSET);
    cs_wrp_ptr = mac_ie_find(buffer, buflen, MAC_ELTID_CHAN_SWITCH_WRP);
    if (cs_wrp_ptr != 0)
    {
        wide_bw_cs = mac_ie_find(cs_wrp_ptr + MAC_INFOELT_INFO_OFT,
                                 co_read8p(cs_wrp_ptr + MAC_INFOELT_LEN_OFT),
                                 MAC_ELTID_WIDE_BANDWIDTH_CHAN_SWITCH);
    }
    else
    {
        wide_bw_cs = 0;
    }

    if (csa_ptr)
    {
        count = co_read8p(csa_ptr + MAC_INFOELT_SWITCH_COUNT_OFT);
        *mode  = co_read8p(csa_ptr + MAC_INFOELT_SWITCH_MODE_OFT);
        prim20 = co_read8p(csa_ptr + MAC_INFOELT_SWITCH_NEW_CHAN_OFT);
    }
    else
    {
        count = co_read8p(ecsa_ptr + MAC_INFOELT_EXT_SWITCH_COUNT_OFT);
        *mode  = co_read8p(ecsa_ptr + MAC_INFOELT_EXT_SWITCH_MODE_OFT);
        prim20 = co_read8p(ecsa_ptr + MAC_INFOELT_EXT_SWITCH_NEW_CHAN_OFT);
    }

    if (count == 0)
        count = 2;

    band = (prim20 > 14) ? PHY_BAND_5G : PHY_BAND_2G4;
    prim20 = phy_channel_to_freq(band, prim20);

    if (wide_bw_cs)
    {
        bw = co_read8p(wide_bw_cs + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CW_OFT);
        center1 = co_read8p(wide_bw_cs + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CENTER1_OFT);
        center2 = co_read8p(wide_bw_cs + MAC_INFOELT_WIDE_BW_CHAN_SWITCH_NEW_CENTER2_OFT);
        switch (bw)
        {
        case 1:
            bw = PHY_CHNL_BW_80;
            break;
        case 2:
            bw = PHY_CHNL_BW_160;
            break;
        case 3:
            bw = PHY_CHNL_BW_80P80;
            break;
        default :
            bw =  PHY_CHNL_BW_40;
            break;
        }
        center1 = phy_channel_to_freq(band, center1);
        if (center2)
            center2 = phy_channel_to_freq(band, center2);
    }
    else if (sec_chan_ptr)
    {
        center1 = co_read8p(sec_chan_ptr + MAC_INFOELT_SEC_CH_OFFSET_OFT);
        if (center1 == 1)
        {
            center1 = prim20 + 10;
            bw =  PHY_CHNL_BW_40;
        }
        else if (center1 == 3)
        {
            center1 = prim20 - 10;
            bw =  PHY_CHNL_BW_40;
        }
        else
        {
            center1 = prim20;
            bw =  PHY_CHNL_BW_20;
        }
    }
    else
    {
        center1 = prim20;
        bw =  PHY_CHNL_BW_20;
    }

    chan_desc->band = band;
    chan_desc->type = bw;
    chan_desc->prim20_freq = prim20;
    chan_desc->center1_freq = center1;
    chan_desc->center2_freq = center2;

    return count;
}
