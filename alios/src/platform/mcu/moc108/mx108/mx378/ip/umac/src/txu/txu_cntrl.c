/**
 ****************************************************************************************
 *
 * @file txu_cntrl.c
 *
 * @brief UMAC TX Path implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/** @addtogroup UMACTX
 * @{
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// for mode
#include "txl_cfm.h"
#include "txu_cntrl.h"
#include "ke_task.h"
#include "mac.h"
#include "mac_frame.h"
#include "sta_mgmt.h"
#include "llc.h"
#include "co_utils.h"
#include "co_endian.h"
#include "vif_mgmt.h"
#include "me_utils.h"
#include "bam.h"
#include "mm.h"
#include "tpc.h"

#if NX_MFP
#include "mfp.h"
#endif

#if (RW_MESH_EN)
#include "mesh.h"
#include "mesh_hwmp.h"
#include "mesh_ps.h"
#endif //(RW_MESH_EN)

#include "include.h"
#include "arm_arch.h"

#include "uart_pub.h"
/*
 * PRIVATE FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/*
 * Accept all packets if the port is OPEN,
 * Accept only EAP packets if port is CONTROLED
 * Discard all packets if the port is CLOSED
 */
static bool txu_cntrl_logic_port_filter(uint8_t sta_idx, uint16_t eth_type)
{
    bool status;
    uint8_t port_state = sta_mgmt_get_port_state(sta_idx);
    uint16_t port_proto = sta_mgmt_get_port_ethertype(sta_idx);

    do
    {
        if(port_state == PORT_CLOSED)
        {
           status = false;
           break;
        }

        if ((port_state == PORT_OPEN) ||
            ((port_state == PORT_CONTROLED) && (eth_type == port_proto)))
        {
            status = true;
            break;
        }

        // If we execute this line, it means that we must discard the frame
        status = false;
    } while(0);

    return (status);
}

static int txu_cntrl_sechdr_len_compute(struct txdesc *txdesc, int *tail_len)
{
    struct hostdesc *host = &txdesc->host;
    struct sta_info_tag *sta = &sta_info_tab[host->staid];
    struct key_info_tag *key = *sta->sta_sec_info.cur_key;
    struct vif_info_tag *vif = &vif_info_tab[host->vif_idx];
    int head_len = 0;

    // By default we consider that there won't be any tail
    *tail_len = 0;

    // Check if we have a valid key for this STA
    if (!key || (vif->flags & CONTROL_PORT_NO_ENC &&
                 co_ntohs(host->ethertype) == sta_mgmt_get_port_ethertype(host->staid)))
        return 0;

    // Check which type of security is used with this STA
    switch(key->cipher)
    {
        case MAC_RSNIE_CIPHER_WEP40:
        case MAC_RSNIE_CIPHER_WEP104:
            head_len = IV_LEN;
            *tail_len = ICV_LEN;
            // Check if we need to get a fresh PN
            if (!(host->flags & TXU_CNTRL_RETRY))
            {
                key->tx_pn++;
                memcpy(host->pn, &key->tx_pn, 2 * sizeof(host->pn[0]));
            }
            break;
        case MAC_RSNIE_CIPHER_TKIP:
            head_len = IV_LEN + EIV_LEN;
            *tail_len = MIC_LEN + ICV_LEN;
            // Check if we need to get a fresh PN
            if (!(host->flags & TXU_CNTRL_RETRY))
            {
                key->tx_pn++;
                memcpy(host->pn, &key->tx_pn, 3 * sizeof(host->pn[0]));
            }
            break;
        case MAC_RSNIE_CIPHER_CCMP:
            head_len = IV_LEN + EIV_LEN;
            *tail_len = MIC_LEN;
            // Check if we need to get a fresh PN
            if (!(host->flags & TXU_CNTRL_RETRY))
            {
                key->tx_pn++;
                memcpy(host->pn, &key->tx_pn, 3 * sizeof(host->pn[0]));
            }
            break;
        #if RW_WAPI_EN
        case MAC_RSNIE_CIPHER_WPI_SMS4:
            head_len = WPI_IV_LEN;
            *tail_len = WPI_MIC_LEN;
            if (!(host->flags & TXU_CNTRL_RETRY))
            {
                if (key->hw_key_idx < MM_SEC_DEFAULT_KEY_COUNT) {
                    // broadcast key
                    key->tx_pn++;
                } else {
                    // unicast key
                    key->tx_pn+=2;
                }
                memcpy(host->pn, &key->tx_pn, 4 * sizeof(host->pn[0]));
            }
            break;
        #endif
    }

    return (head_len);
}

static void txu_cntrl_umacdesc_prep(struct txdesc *txdesc)
{
    struct hostdesc *host = &txdesc->host;
    struct umacdesc *umac = &txdesc->umac;
    #if (RW_MESH_EN)
    // Get VIF Information
    struct vif_info_tag *p_vif_entry = &vif_info_tab[host->vif_idx];
    #endif //(RW_MESH_EN)
    struct sta_info_tag *sta = &sta_info_tab[host->staid];
    int head_len, tail_len, hdr_len_802_2 = 0;
    uint16_t payl_len;

    // Check if the STA has QoS enabled
    if (host->tid != 0xFF)
    {
        // Packet shall be of type QoS
        head_len = MAC_SHORT_QOS_MAC_HDR_LEN;
        // Check if we need to get a fresh SN
        if (!(host->flags & TXU_CNTRL_RETRY))
            host->sn = sta->seq_nbr[host->tid]++;

        #if (RW_MESH_EN)
        if (p_vif_entry->type == VIF_MESH_POINT)
        {
            // Check if Mesh Control field is present
            head_len += mesh_tx_data_prepare(p_vif_entry, host, umac);
        }
        #endif //(RW_MESH_EN)
    }
    else
    {
        head_len = MAC_SHORT_MAC_HDR_LEN;
    }

    if (host->flags & TXU_CNTRL_USE_4ADDR)
    {
        head_len += (MAC_LONG_MAC_HDR_LEN - MAC_SHORT_MAC_HDR_LEN);
    }

    // Update the header length
    head_len += txu_cntrl_sechdr_len_compute(txdesc, &tail_len);

    #if (RW_MESH_EN)
    if (!(host->flags & TXU_CNTRL_MESH_FWD))
    #endif //(RW_MESH_EN)
    {
        // Check if the Ethernet frame is of Type II or SNAP
        if (co_ntohs(host->ethertype) > LLC_FRAMELENGTH_MAXVALUE)
        {
            // Update header length - LLC/SNAP will be added after buffer allocation
            head_len += LLC_802_2_HDR_LEN;
            hdr_len_802_2 = LLC_802_2_HDR_LEN;
        }
    }

    // Compute payload length
    #if NX_AMSDU_TX
    if (host->flags & TXU_CNTRL_AMSDU)
    {
        head_len += sizeof_b(struct amsdu_hdr);
    }

    payl_len = 0;
    for (int i = 0; i < host->packet_cnt; i++)
    {
        payl_len += host->packet_len[i];
    }
    #else
    payl_len = host->packet_len;
    #endif

    // Update the different lengths
    umac->payl_len = payl_len;
    umac->head_len = head_len;
    umac->tail_len = tail_len;
    umac->hdr_len_802_2 = hdr_len_802_2;

    #if NX_AMPDU_TX
    umac->phy_flags = 0;
    umac->flags = 0;
    #endif
}

static void txu_cntrl_umacdesc_mgmt_prep(struct txdesc *p_txdesc, struct vif_info_tag *p_vif_entry)
{
    struct hostdesc *p_host = &p_txdesc->host;
    struct umacdesc *p_umac = &p_txdesc->umac;
    int head_len = 0, tail_len = 0;
    uint8_t pwr_idx, i;

    // Select the policy table to be used
    if ((p_host->flags & TXU_CNTRL_MGMT_NO_CCK)
            #if (NX_P2P)
            || p_vif_entry->p2p
            #endif //(NX_P2P)
        )
    {
        p_umac->buf_control = &txl_buffer_control_5G;
    }
    else
    {
        uint8_t band = p_vif_entry->chan_ctxt->channel.band;

        p_umac->buf_control = (band == PHY_BAND_2G4) ? &txl_buffer_control_24G : &txl_buffer_control_5G;
    }

    #if NX_MFP
    if (p_host->flags & TXU_CNTRL_MGMT_ROBUST)
    {
        enum mfp_protection mfp;

        /* still need to check if MFP is enable on this vif, and since
           we don't know which mgmt frame is it, pretend it is DEAUTH */
        mfp = mfp_protect_mgmt_frame(p_txdesc, MAC_FCTRL_DEAUTHENT, 0);

        if (mfp == MFP_UNICAST_PROT)
        {
            head_len = txu_cntrl_sechdr_len_compute(p_txdesc, &tail_len);
            //TODO: Use another policy table, as we will overwrite key index ?
        }
        else if (mfp == MFP_MULTICAST_PROT)
        {
            tail_len = MAC_MGMT_MIC_LEN;
        }
        else
        {
            p_host->flags &= ~TXU_CNTRL_MGMT_ROBUST;
        }
    }
    #endif

    // update TX Power in policy table
    pwr_idx = tpc_get_vif_tx_power(p_vif_entry);
    for (i = 0; i < RATE_CONTROL_STEPS; i++)
    {
        p_umac->buf_control->policy_tbl.powercntrlinfo[i] = pwr_idx << TX_PWR_LEVEL_PT_RCX_OFT;
    }


    // head_len and tail_len are used by LMAC.
    p_umac->head_len = head_len;
    p_umac->tail_len = tail_len;

    #if (NX_AMPDU_TX)
    p_umac->phy_flags = 0;
    p_umac->flags = 0;
    #endif //(NX_AMPDU_TX)
}

static uint32_t txu_cntrl_mac_hdr_append(struct txdesc *txdesc, uint32_t buf)
{
    struct hostdesc *host = &txdesc->host;
    
    #if (RW_MESH_EN)
    struct umacdesc *p_umac_desc = &txdesc->umac;
    #endif //(RW_MESH_EN)
    
    struct vif_info_tag *vif = &vif_info_tab[host->vif_idx];
    struct sta_info_tag *sta = &sta_info_tab[host->staid];
    struct key_info_tag *key = *sta->sta_sec_info.cur_key;
    struct mac_hdr_qos *machdr;
    struct mac_hdr_long_qos *machdr_4a;
    uint8_t *qos;
    uint16_t qos_shadow = 0;

    buf -= MAC_SHORT_MAC_HDR_LEN;
    if (host->tid != 0xFF)
    {
        buf -= MAC_HDR_QOS_CTRL_LEN;

        #if (RW_MESH_EN)
        if ((vif->type == VIF_MESH_POINT) && p_umac_desc->has_mesh_ctrl)
        {
            buf -= (MESH_CTRL_MIN_LEN + (p_umac_desc->nb_ext_addr * MAC_ADDR_LEN));
        }
        #endif //(RW_MESH_EN)
    }

    if (host->flags & TXU_CNTRL_USE_4ADDR)
    {
        buf -= (MAC_LONG_MAC_HDR_LEN - MAC_SHORT_MAC_HDR_LEN);
        machdr = HW2CPU(buf);
        machdr_4a = HW2CPU(buf);
        qos = (uint8_t *)&(machdr_4a->qos);
    }
    else
    {
        machdr = HW2CPU(buf);
        machdr_4a = NULL;
        qos = (uint8_t *)&(machdr->qos);
    }

    // Check if the STA has QoS enabled
    if (host->tid != 0xFF)
    {
        // Packet shall be of type QoS
        machdr->fctl = MAC_QOS_ST_BIT;
        qos_shadow = host->tid << MAC_QOSCTRL_UP_OFT;
        machdr->seq = host->sn << MAC_SEQCTRL_NUM_OFT;
        
        #if NX_AMSDU_TX
        if (host->flags & TXU_CNTRL_AMSDU)
            qos_shadow |= MAC_QOSCTRL_AMSDU_PRESENT;
        #endif
        
        if (host->flags & TXU_CNTRL_EOSP)
        {
            qos_shadow |= MAC_QOSCTRL_EOSP;
            sta->uapsd_sp_ongoing = false;
        }

        #if (RW_MESH_EN)
        if (vif->type == VIF_MESH_POINT)
        {
            if (p_umac_desc->has_mesh_ctrl)
            {
                qos_shadow |= MAC_QOSCTRL_MESH_CTRL_PRESENT;

                // Fill the Mesh Control Field
                qos[0] = qos_shadow & 0xff;
                qos[1] = (qos_shadow >> 8) & 0xff;
                mesh_tx_data_fill_mesh_ctrl(vif, CPU2HW(qos) + MAC_HDR_QOS_CTRL_LEN, host, p_umac_desc);
            }
            else if (host->flags & TXU_CNTRL_MESH_FWD)
            {
                qos_shadow |= MAC_QOSCTRL_MESH_CTRL_PRESENT;
            }
        }
        #else
        (void)qos;
        #endif //(RW_MESH_EN)
    }
    else
    {
        machdr->fctl = 0;
        machdr->seq = 0;
    }

    // Frame Control
    machdr->fctl |= MAC_FCTRL_DATA_T;
    // Check the type of VIF
    if (txdesc->host.flags & TXU_CNTRL_TDLS)
        machdr->fctl &= ~MAC_FCTRL_TODS_FROMDS;
    else if (host->flags & TXU_CNTRL_USE_4ADDR)
        machdr->fctl |= MAC_FCTRL_TODS_FROMDS;
    else if (vif->type == VIF_STA)
        machdr->fctl |= MAC_FCTRL_TODS;
    else if (vif->type == VIF_AP)
        machdr->fctl |= MAC_FCTRL_FROMDS;
    #if (RW_MESH_EN)
    else if (vif->type == VIF_MESH_POINT)
    {
        machdr->fctl |= MAC_FCTRL_FROMDS;
    }
    #endif //(RW_MESH_EN)

    // Check if the more data bit shall be set
    if (host->flags & TXU_CNTRL_MORE_DATA)
        machdr->fctl |= MAC_FCTRL_MOREDATA;

    /*
     * Addresses
     ************************************************
     * IEEE 802.11 Address Field                    *
     * See IEEE 802.11-2012 Table 8.19              *
     *                                              *
     *  ToDS FromDS Addr1 Addr2 Addr3 Addr4         *
     *    0    0    DA    SA    BSSID n/a           *
     *    0    1    DA    BSSID SA    n/a           *
     *    1    0    BSSID SA    DA    n/a           *
     *    1    1    RA    TA    DA    SA            *
     ************************************************
     */
    MAC_ADDR_CPY(&machdr->addr2, &vif->mac_addr);
    #if (RW_MESH_EN)
    if (vif->type == VIF_MESH_POINT)
    {
        if (host->flags & TXU_CNTRL_USE_4ADDR)
        {
            // Get Mesh Path to be used
            struct mesh_hwmp_path_tag *p_mpath_entry = &mesh_hwmp_path_pool[p_umac_desc->path_idx];

            MAC_ADDR_CPY(&machdr_4a->addr3, &p_mpath_entry->tgt_mac_addr);
            MAC_ADDR_CPY(&machdr_4a->addr1, &sta->mac_addr);

            if (host->flags & TXU_CNTRL_MESH_FWD)
            {
                MAC_ADDR_CPY(&machdr_4a->addr4, &host->eth_src_addr);
            }
            else
            {
                MAC_ADDR_CPY(&machdr_4a->addr4, &vif->mac_addr);
            }
        }
        else if ((machdr->fctl & MAC_FCTRL_TODS_FROMDS) == MAC_FCTRL_FROMDS)
        {
            MAC_ADDR_CPY(&machdr->addr1, &host->eth_dest_addr);
            MAC_ADDR_CPY(&machdr->addr3, &vif->mac_addr);
        }
    }
    else
    #endif //(RW_MESH_EN)
    {
        if ((machdr->fctl & MAC_FCTRL_TODS_FROMDS) == MAC_FCTRL_TODS)
        {
            MAC_ADDR_CPY(&machdr->addr1, &sta->mac_addr);
            MAC_ADDR_CPY(&machdr->addr3, &host->eth_dest_addr);
        }
        else if ((machdr->fctl & MAC_FCTRL_TODS_FROMDS) == MAC_FCTRL_FROMDS)
        {
            MAC_ADDR_CPY(&machdr->addr1, &host->eth_dest_addr);
            MAC_ADDR_CPY(&machdr->addr3, &host->eth_src_addr);
        }
        else if ((machdr->fctl & MAC_FCTRL_TODS_FROMDS) == 0)
        {
            MAC_ADDR_CPY(&machdr->addr1, &host->eth_dest_addr);
            MAC_ADDR_CPY(&machdr->addr3, &vif->bss_info.bssid);
        }
        else
        {
            MAC_ADDR_CPY(&machdr_4a->addr1, &sta->mac_addr);
            MAC_ADDR_CPY(&machdr_4a->addr3, &host->eth_dest_addr);
            MAC_ADDR_CPY(&machdr_4a->addr4, &host->eth_src_addr);
        }
    }

    // Check if the frame will be protected
    if (key && !(vif->flags & CONTROL_PORT_NO_ENC &&
                 co_ntohs(host->ethertype) == sta_mgmt_get_port_ethertype(host->staid)))
    {
        machdr->fctl |= MAC_FCTRL_PROTECTEDFRAME;
    }

    return buf;
}

static uint32_t txu_cntrl_sec_hdr_append(struct txdesc *txdesc, uint32_t buf)
{
    struct hostdesc *host = &txdesc->host;
    struct umacdesc *umac = &txdesc->umac;
    struct sta_info_tag *sta = &sta_info_tab[host->staid];
    struct key_info_tag *key = *sta->sta_sec_info.cur_key;
    struct vif_info_tag *vif = &vif_info_tab[host->vif_idx];
    struct tx_policy_tbl *pol;
    uint32_t mac_control_info1;
    uint16_t *iv;

    // Check if we have a valid key for this STA
    if (!key || (vif->flags & CONTROL_PORT_NO_ENC &&
                 co_ntohs(host->ethertype) == sta_mgmt_get_port_ethertype(host->staid)))
        return buf;

    // Check which type of security is used with this STA
    // Check which encryption type has to be used
    switch(key->cipher)
    {
        case MAC_RSNIE_CIPHER_WEP40:
        case MAC_RSNIE_CIPHER_WEP104:
            // Build IV
            buf -= IV_LEN;
            iv = HW2CPU(buf);
            iv[0] = host->pn[0];
            iv[1] = host->pn[1] | (key->key_idx << 14);
            break;
        case MAC_RSNIE_CIPHER_TKIP:
            // Build IV/EIV
            buf -= IV_LEN + EIV_LEN;
            iv = HW2CPU(buf);
            iv[0] = (host->pn[0] >> 8) | ((host->pn[0] | 0x2000) & 0x7F00);
            iv[1] = (host->pn[0] & 0xFF) | (key->key_idx << 14) | EIV_PRESENT;
            iv[2] = host->pn[1];
            iv[3] = host->pn[2];
            break;
        case MAC_RSNIE_CIPHER_CCMP:
        {
            #if (RW_MESH_EN)
            uint32_t offset = 0;

            if ((vif->type = VIF_MESH_POINT) && umac->has_mesh_ctrl)
            {
                // Get Mesh VIF Information
                struct mesh_vif_info_tag *p_mvif_entry = &mesh_vif_info_tab[vif->mvif_idx];

                if (p_mvif_entry->is_auth)
                {
                    offset += (MESH_CTRL_MIN_LEN + (umac->nb_ext_addr * MAC_ADDR_LEN));
                }
            }
            #endif //(RW_MESH_EN)

            // Build IV/EIV
            buf -= IV_LEN + EIV_LEN;
            #if (RW_MESH_EN)
            iv = HW2CPU(buf - offset);
            #else
            iv = HW2CPU(buf);
            #endif //(RW_MESH_EN)
            iv[0] = host->pn[0];
            iv[1] = EIV_PRESENT | (key->key_idx << 14);
            iv[2] = host->pn[1];
            iv[3] = host->pn[2];
        } break;
        #if RW_WAPI_EN
        case MAC_RSNIE_CIPHER_WPI_SMS4:
            buf -= WPI_IV_LEN;
            iv = HW2CPU(buf);
            iv[0] = key->key_idx;
            iv[1] = host->pn[0];
            iv[2] = host->pn[1];
            iv[3] = host->pn[2];
            iv[4] = host->pn[3];
            iv[5] = 0x5c36;
            iv[6] = 0x5c36;
            iv[7] = 0x5c36;
            iv[8] = 0x5c36;
            break;
        #endif
    }

    // Update the policy table with the right key index
    pol = &umac->buf_control->policy_tbl;
    mac_control_info1 = pol->maccntrlinfo1 & KEYSRAM_INDEX_RA_MASK;
    pol->maccntrlinfo1 = mac_control_info1 | key->hw_key_idx;

    return (buf);
}

static uint32_t  txu_cntrl_llc_hdr_append(struct txdesc *txdesc, uint32_t buf)
{
    struct hostdesc *host = &txdesc->host;
    struct llc_snap *llc_snap_ptr;

    // Check if the Ethernet frame is of Type II or SNAP
    if (co_ntohs(host->ethertype) < LLC_FRAMELENGTH_MAXVALUE)
        return buf;

    // Compute address of LLC snap
    buf -= sizeof_b(struct llc_snap);
    llc_snap_ptr = HW2CPU(buf);

    /*
     ***********************************************************************
     *              TYPE II: Build up the 802_2 Header (LLC)               *
     ***********************************************************************
     * Received from the adapter (Before the LLC translation):
     *    xxxxxxxxxxx   |    Eth Hdr    | Data
     *   [data_offset]        (14)
     *
     * After the LLC translation:
     *    xxxxxxxxxxxxxxx   | 802.2 Hdr | Data
     *     [data_offset]         (8)
     ***********************************************************************
     */
    // Fill the LLC SNAP header the LLC+OUI to the packet
    llc_snap_ptr->dsap_ssap = (LLC_DSAP | LLC_SSAP << 8);
    llc_snap_ptr->control_oui0 = LLC_CTRL;
    llc_snap_ptr->oui1_2 = 0;
    llc_snap_ptr->proto_id = host->ethertype;

    return buf;
}

#if NX_AMSDU_TX
static uint32_t  txu_cntrl_amsdu_hdr_append(struct txdesc *txdesc, uint32_t buf)
{
    struct hostdesc *host = &txdesc->host;
    struct umacdesc *umac = &txdesc->umac;
    struct amsdu_hdr *amsdu_hdr;
    uint16_t length;

    if (!(host->flags & TXU_CNTRL_AMSDU))
        return buf;

    buf -= sizeof_b(struct amsdu_hdr);
    amsdu_hdr = HW2CPU(buf);

    MAC_ADDR_CPY(&amsdu_hdr->da, &host->eth_dest_addr);
    MAC_ADDR_CPY(&amsdu_hdr->sa, &host->eth_src_addr);
    length = host->packet_len[0] + umac->hdr_len_802_2;
    amsdu_hdr->len= co_htons(length);

    return buf;
}
#endif

static void txu_cntrl_check_rate(struct txdesc *txdesc)
{
    // Check if rate must be changed
    struct hostdesc *host = &txdesc->host;
    #if NX_AMPDU_TX
    bool tx_ampdu = txdesc->umac.flags & AMPDU_BIT ? 1 : 0;
    uint8_t step = me_check_rc(txdesc->host.staid, &tx_ampdu);
    #if RC_ENABLE
    if (tx_ampdu == 0)
    {
        txdesc->umac.flags &= ~AMPDU_BIT;
    }
    #endif
    #else
    me_check_rc(txdesc->host.staid, 0);
    #endif

    // Update Buffer control
    me_update_buffer_control(&sta_info_tab[host->staid], &txdesc->umac);

    #if NX_AMPDU_TX
    // Update the PHY flags accordingly
    txdesc->umac.phy_flags = txdesc->umac.buf_control->policy_tbl.ratecntrlinfo[step];
    #endif
}

static void txu_cntrl_discard(struct txdesc *txdesc, uint8_t access_category)
{
    GLOBAL_INT_DECLARATION();

    // Increment the packet counter to keep it in track with the confirmation
    txl_cntrl_inc_pck_cnt();

    // Send the confirmation to the host
    GLOBAL_INT_DISABLE();
    txl_cfm_push(txdesc, DESC_DONE_SW_TX_BIT | DESC_DONE_TX_BIT, access_category);
    GLOBAL_INT_RESTORE();
}


/*
 * PUBLIC FUNCTION DEFINITIONS
 ****************************************************************************************
 */

void txu_cntrl_init(void)
{
}

void txu_cntrl_frame_build(struct txdesc *txdesc, uint32_t buf)
{
    #if (RW_MESH_EN)
    if (!(txdesc->host.flags & TXU_CNTRL_MESH_FWD))
    #endif //(RW_MESH_EN)
    {
        // Check if we need to build the LLC/SNAP
        buf = txu_cntrl_llc_hdr_append(txdesc, buf);
    }

    #if NX_AMSDU_TX
    buf = txu_cntrl_amsdu_hdr_append(txdesc, buf);
    #endif

    // Build the SEC header and trailers
    buf = txu_cntrl_sec_hdr_append(txdesc, buf);

    // Build the MAC header
    buf = txu_cntrl_mac_hdr_append(txdesc, buf);
}

bool txu_cntrl_push(struct txdesc *txdesc, uint8_t access_category)
{
    struct vif_info_tag *p_vif_entry;
    bool queue_stop = false;
	
    do
    {
        p_vif_entry = &vif_info_tab[txdesc->host.vif_idx];
        if (!p_vif_entry)
        {
        	warning_prf("---------------------port_filtering:%d:0x%x\r\n", txdesc->host.staid,  co_ntohs(txdesc->host.ethertype));
            break;
        }

        // Check if packet can be sent now (current channel could not be the good one)
        if (!txl_cntrl_tx_check(p_vif_entry))
        {
			warning_prf("bad_channel\r\n");
            break;
        }

        // Check if the frame is a management frame
        if ((txdesc->host.flags & TXU_CNTRL_MGMT))
        {
            // Prepare the UMAC part of the TX descriptor
            txu_cntrl_umacdesc_mgmt_prep(txdesc, p_vif_entry);
        }
        else
        {
            // Perform logical port filtering
            if (!txu_cntrl_logic_port_filter(txdesc->host.staid, co_ntohs(txdesc->host.ethertype)))
            {
				warning_prf("pfilter:0x%x\r\n",txdesc->host.staid);
                break;
            }

            #if (RW_MESH_EN)
            // If Mesh Point VIF, check if destination can be reached
            if (p_vif_entry->type == VIF_MESH_POINT)
            {
                if (txdesc->host.staid < NX_REMOTE_STA_MAX)
                {
                    // Check if frame can be sent to the peer mesh STA
                    if (!mesh_ps_tx_data_handle(p_vif_entry, txdesc->host.staid, txdesc))
                    {
                        break;
                    }
                }

                if (!mesh_hwmp_path_check(p_vif_entry, &txdesc->host.eth_dest_addr, &txdesc->umac))
                {
                    break;
                }
            }
            #endif //(RW_MESH_EN)

            // Prepare the UMAC part of the TX descriptor
            txu_cntrl_umacdesc_prep(txdesc);

            #if (NX_AMPDU_TX)
            // Check if the packet can be aggregated
            bam_check_ba_agg(txdesc);
            #endif

            // Check RC stats
            txu_cntrl_check_rate(txdesc);
        }

        // Push the descriptor to LMAC
        queue_stop = txl_cntrl_push(txdesc, access_category);

        // Handling is done for this frame
        return (queue_stop);
    } while(0);

    // Packet discarded
    txu_cntrl_discard(txdesc, access_category);

    return false;
}

void txu_cntrl_tkip_mic_append(struct txdesc *txdesc, uint8_t ac)
{
    struct hostdesc *host = &txdesc->host;
    struct umacdesc *umac = &txdesc->umac;
    struct sta_info_tag *sta = &sta_info_tab[host->staid];
    struct key_info_tag *key = *sta->sta_sec_info.cur_key;
    struct txl_buffer_tag *buffer = txl_buffer_get(txdesc);
    uint32_t buf = CPU2HW(buffer->payload) + buffer->padding 
                            + umac->head_len 
                            - umac->hdr_len_802_2;

    // Check if we have a valid key for this STA
    if (!key)
        return;

    // Check which type of security is used with this STA
    // Check which encryption type has to be used
    switch(key->cipher)
    {
        case MAC_RSNIE_CIPHER_TKIP:
            txl_buffer_mic_compute(txdesc, key->u.mic.tx_key, buf,
                                   umac->hdr_len_802_2 + umac->payl_len, ac);
            break;
				
        default:
            break;
    }
		
		return;
}

void txu_cntrl_cfm(struct txdesc *txdesc)
{
    int8_t credits = 1;
    #if NX_AMPDU_TX
    struct hostdesc *host = &txdesc->host;
    #endif
    struct tx_cfm_tag *cfm = &txdesc->lmac.hw_desc->cfm;
    // Indicate if transmission is successful
    bool success = ((cfm->status & FRAME_SUCCESSFUL_TX_BIT) != 0);
    // Indicate if retransmission is required by FW
    bool sw_retry = ((cfm->status & (DESC_DONE_SW_TX_BIT | DESC_DONE_TX_BIT)) ==
                                    (DESC_DONE_SW_TX_BIT | DESC_DONE_TX_BIT));

    // Reset the status field
    cfm->status = 0;

    // Specific handling for management frames transmitted by the host
    if (txdesc->host.flags & TXU_CNTRL_MGMT)
    {
        // Check if we need to stop the monitoring or not
        if ((txdesc->host.flags & TXU_CNTRL_MGMT_PM_MON) && !success)
        {
            // Check if we need to stop the monitoring or not
            rxu_cntrl_get_pm();
        }

        // Check if we need to send a EOSP QoS NULL frame
        if (txdesc->host.flags & TXU_CNTRL_EOSP)
        {
            uint16_t qos = MAC_QOSCTRL_EOSP | (0x07 << MAC_QOSCTRL_UP_OFT);

            // Sanity check - buffered UAPSD frame shall belong to a known STA
            ASSERT_ERR(txdesc->host.staid != INVALID_STA_IDX);

            // Send the NULL frame
            txl_frame_send_qosnull_frame(txdesc->host.staid, qos, NULL, NULL);
            sta_info_tab[txdesc->host.staid].uapsd_sp_ongoing = false;
        }
    }

    if (sw_retry)
    {
        cfm->status |= TX_STATUS_SW_RETRY_REQUIRED;
    }
    else
    {
        #if (NX_AMPDU_TX)
        do
        {
            // Check if packet is a QoS one
            if (host->tid == 0xFF)
                break;

            // Check if there is a BA agreement for this packet
            credits = bam_tx_cfm(txdesc, success);
        } while (0);
        #endif
    }

    // Update the confirmation descriptor
    cfm->status |= TX_STATUS_DONE;
    cfm->credits = credits;
    #if NX_AMSDU_TX
    cfm->amsdu_size = me_tx_cfm_amsdu(txdesc);
    #endif
}

void txu_cntrl_protect_mgmt_frame(struct txdesc *txdesc, uint32_t frame,
                                  uint16_t hdr_len)
{
    struct mac_hdr *mac_hdr = (struct mac_hdr *)frame;
    int tail_len, head_len;

    if (txdesc->umac.head_len == 0) {
        head_len = txu_cntrl_sechdr_len_compute(txdesc, &tail_len);
        txdesc->umac.head_len = head_len;
        txdesc->umac.tail_len = tail_len;
    } else {
        head_len = txdesc->umac.head_len;
        tail_len = txdesc->umac.tail_len;
    }

    mac_hdr->fctl |= MAC_FCTRL_PROTECTEDFRAME;
    txu_cntrl_sec_hdr_append(txdesc, CPU2HW(frame) + hdr_len + head_len);
}
/// @}
