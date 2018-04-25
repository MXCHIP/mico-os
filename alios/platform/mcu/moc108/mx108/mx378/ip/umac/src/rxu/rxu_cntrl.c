/**
 ****************************************************************************************
 *
 * @file rxu_cntrl.c
 *
 * @brief The UMAC's Rx module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/** @addtogroup UMACRX
 * @{
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_int.h"
#include "co_bool.h"
#include <string.h>
#include "compiler.h"
#include "rxu_cntrl.h"
#include "co_ring.h"
#include "ke_event.h"
#include "ke_task.h"
#include "me_task.h"
#include "mac_frame.h"
#include "rx_swdesc.h"
#include "rxl_cntrl.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "mm.h"
#include "rxl_hwdesc.h"
#include "ps.h"

#include "co_utils.h"
#include "hal_desc.h"

#include "llc.h"
#include "bam.h"
#include "mac_ie.h"
#include "me_utils.h"
#include "rxu_task.h"
#include "scanu_task.h"
#include "wlan_ui_pub.h"
#include "param_config.h"

#if NX_MFP
#include "mfp.h"
#endif

#if (RW_MESH_EN)
#include "mesh_hwmp.h"
#endif

#include "include.h"
#include "arm_arch.h"
#include "mem_pub.h"
#include "uart_pub.h"

#if CFG_WIFI_AP_MODE
#include "sk_intf.h"
#endif

#include "defs.h"
#include "ieee802_11_defs.h"
#include "wpa_common.h"

/*
 * DEFINES
 ****************************************************************************************
 */

/// Ethernet Protocol IDs
/// IPX over DIX
#define RX_ETH_PROT_ID_IPX          (0x8137)
/// Appletalk AARP
#define RX_ETH_PROT_ID_AARP         (0x80F3)

/// Ethernet Header Length (2 MAC Addresses (6 bytes each) + Len/Type Field (2 bytes))
#define RX_CNTRL_ETH_HDR_LEN        (14)
/// LLC/SNAP length
#define RX_CNTRL_ETH_LLC_SNAP_LEN   (8)

/*
 * MACROS
 ****************************************************************************************
 */

/// Indicate if the received packet is a Data packet by reading the Frame Control Field
#define RXL_CNTRL_IS_DATA(frame_cntrl)                                                   \
           ((frame_cntrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_DATA_T)
/// Indicate if the received packet is a QOS Data packet by reading the Frame Control Field
#define RXL_CNTRL_IS_QOS_DATA(frame_cntrl)                                               \
           ((frame_cntrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_QOS_DATA)
/// Indicate if the received packet is a Management packet by reading the Frame Control Field
#define RXL_CNTRL_IS_MGT(frame_cntrl)                                                    \
           ((frame_cntrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_MGT_T)
/// Indicate if the received packet is a Control packet by reading the Frame Control Field
#define RXL_CNTRL_IS_CTRL(frame_cntrl)                                                   \
           ((frame_cntrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_CTRL_T)

/// Indicate if the received frame was encrypted (Protected Bit set to 1)
#define RXL_CNTRL_IS_PROTECTED(frame_cntrl)                                               \
           ((frame_cntrl & MAC_FCTRL_PROTECTEDFRAME) == MAC_FCTRL_PROTECTEDFRAME)

///**
// ****************************************************************************************
// * @brief Determines whether or not the received frame is an EAPOL.
// *
// * @param[in] rxdesc Pointer to MSDU descriptor associated with the payload
// *
// * @return True if the frame is an EAPOL frame, false otherwise.
// ****************************************************************************************
// */
//__INLINE bool rxu_is_frame_eapol(struct rxdesc* rxdesc)
//{
//    return (rxdesc->eth_type2 == LLC_ETHERTYPE_EAP_T);
//}

/*
 * GLOBAL VARIABLE DECLARATIONS
 ****************************************************************************************
 */

/// The RX module's environment
struct rxu_cntrl_env_tag rxu_cntrl_env;


#if (NX_REORD)
/// Pool of reordering structures
struct rxu_cntrl_reord rxu_cntrl_reord_pool[RX_CNTRL_REORD_POOL_SIZE];
#endif //(NX_REORD)

/// Pool of reordering structures
struct rxu_cntrl_defrag rxu_cntrl_defrag_pool[RX_CNTRL_DEFRAG_POOL_SIZE];

// RFC1042 LLC/SNAP Header
static const struct llc_snap_short rxu_cntrl_rfc1042_hdr = {
                                                             0xAAAA, // DSAP LSAP
                                                             0x0003, // Control/Prot0
                                                             0x0000, // Prot1 and 2
                                                           };

// Bridge-Tunnel LLC/SNAP Header
static const struct llc_snap_short rxu_cntrl_bridge_tunnel_hdr = {
                                                                    0xAAAA, // DSAP LSAP
                                                                    0x0003, // Control/Prot0
                                                                    0xF800, // Prot1 and 2
                                                                 };

extern void hal_update_secret_key(uint64_t, uint8_t);

/*
 * PRIVATE FUNCTION DEFINITIONS
 ****************************************************************************************
 */
static bool rxu_cntrl_desc_prepare(struct rx_swdesc *p_rx_swdesc, uint8_t rx_status, uint32_t host_id)
{
    return (0);
}

static void rxu_cntrl_desc_transfer(void)
{
}

__INLINE uint8_t rxu_cntrl_machdr_len_get(uint16_t frame_cntl)
{
    // MAC Header length
    uint8_t mac_hdr_len = MAC_SHORT_MAC_HDR_LEN;

    // Check if Address 4 field is present (FDS and TDS set to 1)
    if ((frame_cntl & (MAC_FCTRL_TODS | MAC_FCTRL_FROMDS))
                                    == (MAC_FCTRL_TODS | MAC_FCTRL_FROMDS))
    {
        mac_hdr_len += (MAC_LONG_MAC_HDR_LEN - MAC_SHORT_MAC_HDR_LEN);
    }

    // Check if QoS Control Field is present
    if (RXL_CNTRL_IS_QOS_DATA(frame_cntl))
    {
        mac_hdr_len += (MAC_LONG_QOS_MAC_HDR_LEN - MAC_LONG_MAC_HDR_LEN);
    }

    // Check if HT Control Field is present (Order bit set to 1)
    if (frame_cntl & MAC_FCTRL_ORDER)
    {
        mac_hdr_len += (MAC_LONG_QOS_HTC_MAC_HDR_LEN - MAC_LONG_QOS_MAC_HDR_LEN);
    }

    return (mac_hdr_len);
}
/**
 ****************************************************************************************
 * @brief This function processes the received encrypted frames
 *
 * @param[in] swdesc SW header descriptor of the frame
 *
 ****************************************************************************************
 */
static bool rxu_cntrl_protected_handle(uint8_t *frame, uint32_t statinfo)
{
    uint32_t frame_addr = CPU2HW(frame);
    uint16_t *iv = HW2CPU(frame_addr + rxu_cntrl_env.rx_status.machdr_len);
    bool ga = (statinfo & RX_HD_GA_FRAME) != 0;
    struct key_info_tag *key;
    bool upload = true;

    switch (statinfo & RX_HD_DECRSTATUS)
    {
        case RX_HD_DECR_CCMPSUCCESS:
            if (ga)
            {
                struct vif_info_tag *vif = &vif_info_tab[rxu_cntrl_env.rx_status.vif_idx];
                key = &vif->key_info[iv[1] >> 14];
            }
            else
            {
                struct sta_info_tag *sta = &sta_info_tab[rxu_cntrl_env.rx_status.sta_idx];
                key = &sta->sta_sec_info.key_info;
            }
            rxu_cntrl_env.rx_status.key = key;
            #if RW_WAPI_EN
            if (key->cipher == MAC_RSNIE_CIPHER_WPI_SMS4) {
                rxu_cntrl_env.rx_status.machdr_len += WPI_IV_LEN;
                rxu_cntrl_env.rx_status.pn = (((uint64_t)iv[4]) << 48) |
                    (((uint64_t)iv[3]) << 32) | (((uint64_t)iv[2]) << 16) | iv[1];
                rxu_cntrl_env.rx_status.frame_info |= RXU_CNTRL_PN_CHECK_NEEDED;
            } else
            #endif
            {
                rxu_cntrl_env.rx_status.machdr_len += IV_LEN + EIV_LEN;
                rxu_cntrl_env.rx_status.pn = (((uint64_t)iv[3]) << 32) |
                    (((uint64_t)iv[2]) << 16) | iv[0];
                rxu_cntrl_env.rx_status.frame_info |= RXU_CNTRL_PN_CHECK_NEEDED;
            }
            break;
			
        case RX_HD_DECR_WEPSUCCESS:
            rxu_cntrl_env.rx_status.machdr_len += IV_LEN;
            break;
			
        case RX_HD_DECR_TKIPSUCCESS:
            rxu_cntrl_env.rx_status.machdr_len += IV_LEN + EIV_LEN;
            rxu_cntrl_env.rx_status.pn = (((uint64_t)iv[3]) << 32) |
                                         (((uint64_t)iv[2]) << 16) |
                                          (iv[0] << 8) | (iv[1] & 0xFF);
            rxu_cntrl_env.rx_status.frame_info |= RXU_CNTRL_PN_CHECK_NEEDED |
                                                  RXU_CNTRL_MIC_CHECK_NEEDED;
            if (ga)
            {
                struct vif_info_tag *vif = &vif_info_tab[rxu_cntrl_env.rx_status.vif_idx];
                key = &vif->key_info[iv[1] >> 14];
            }
            else
            {
                struct sta_info_tag *sta = &sta_info_tab[rxu_cntrl_env.rx_status.sta_idx];
                key = &sta->sta_sec_info.key_info;
            }
            rxu_cntrl_env.rx_status.key = key;
            break;
			
        default:
            upload = false;
            break;
    }

    return upload;
}

/**
 * Check if the received packet is the same than the previous received one
 * in the case where the STA is registered.
 */
static bool rxu_cntrl_check_pn(uint64_t *pn, struct key_info_tag *key, uint8_t tid)
{
    if (*pn > key->rx_pn[tid])
    {
        key->rx_pn[tid] = *pn;
        return true;
    }

    return false;
}

/**
 * Remove security header (if any) in management frame
 * - When using MFP
 * - When using SHARED-KEY authentication
 */
static void rxu_cntrl_remove_sec_hdr_mgmt_frame(struct rx_swdesc *rx_swdesc,
                                                struct rx_cntrl_rx_status *rx_status)
{
    struct rx_payloaddesc *payl_d = HW2CPU(rx_swdesc->dma_hdrdesc->hd.first_pbd_ptr);
    struct mac_hdr *machdr = (struct mac_hdr *)payl_d->buffer;
    uint8_t machdr_len = rxu_cntrl_machdr_len_get(machdr->fctl);
    uint8_t payl_offset = rx_status->machdr_len - machdr_len;
    uint16_t *src, *dst, *start;

    if (payl_offset == 0)
        return;

    ASSERT_WARN((payl_offset & 0x1) == 0);

    start = src = dst = (uint16_t *)payl_d->buffer;
    src  = (uint16_t *)((uint32_t)src + (uint32_t)(machdr_len/2 - 1));
    dst  = (uint16_t *)((uint32_t)dst + (uint32_t)(rx_status->machdr_len/2 - 1));

    while (src >= start)
    {
        *dst-- = *src--;
    }

    rx_swdesc->dma_hdrdesc->hd.frmlen -= payl_offset;
    rx_status->payl_offset = payl_offset;
    rx_status->machdr_len -= payl_offset;
}
/**
 ****************************************************************************************
 * @brief Convert the 802.11 MAC Header into a 802.3 Ethernet Header
 ****************************************************************************************
 */
static void rxu_cntrl_mac2eth_update(struct rx_swdesc *rx_swdesc_ptr)
{
    #if (RW_MESH_EN)
    // Get VIF Information
    struct vif_info_tag *p_vif_entry = &vif_info_tab[rxu_cntrl_env.rx_status.vif_idx];
    #endif //(RW_MESH_EN)
    struct rx_dmadesc *dma_hdrdesc = rx_swdesc_ptr->dma_hdrdesc;
    // First payload descriptor -> Buffer will contain the MAC Header (and IV, EIV if present)
    struct rx_payloaddesc *p_first_pbd
            = (struct rx_payloaddesc *)HW2CPU(dma_hdrdesc->hd.first_pbd_ptr);
    // Map a MAC Header structure on the frame
    struct mac_hdr *machdr = (struct mac_hdr *)HW2CPU(p_first_pbd->pbd.datastartptr);
    // LLC/SNAP part of the PDU
    struct llc_snap *llc_snap;
    // Ethernet Header
    struct ethernet_hdr *eth_hdr;
    // Compute MAC Header Length (will IV length + EIV length if present)
    uint16_t machdr_len = rxu_cntrl_env.rx_status.machdr_len;
    uint8_t payl_offset = machdr_len - sizeof_b(struct ethernet_hdr);

    do
    {
        // Check if the frame contains an A-MSDU
        if (RXL_CNTRL_IS_QOS_DATA(machdr->fctl))
        {
            uint16_t qos;

            if ((machdr->fctl & MAC_FCTRL_TODS_FROMDS) ==
                MAC_FCTRL_TODS_FROMDS)
            {
                struct mac_hdr_long_qos *qos_hdr;
                
                qos_hdr = (struct mac_hdr_long_qos *)machdr;
                qos = qos_hdr->qos;
            }
            else
            {
                struct mac_hdr_qos *qos_hdr;

                qos_hdr = (struct mac_hdr_qos *)machdr;
                qos = qos_hdr->qos;
            }

            if (qos & MAC_QOSCTRL_AMSDU_PRESENT)
            {
                dma_hdrdesc->flags |= RX_FLAGS_IS_AMSDU_BIT;
                payl_offset += sizeof_b(struct ethernet_hdr);
                break;
            }
        }

        #if (RW_MESH_EN)
        if ((p_vif_entry->type == VIF_MESH_POINT)
                && (rxu_cntrl_env.rx_status.dst_idx != INVALID_STA_IDX))
        {
            // -> Packet becomes
            /********************************************************************
             *  DA  |  SA  |  ETHERTYPE  |  MESH_CONTROL  |  LLC/SNAP  |  DATA  |
             ********************************************************************/
            // Skip MAC Header (Mesh Control not included)
            eth_hdr = (struct ethernet_hdr *)((uint16_t *)machdr + (machdr_len >> 1) - 7);
            // Pointer to the payload - Skip MAC Header and Mesh Control
            llc_snap = (struct llc_snap *)((uint16_t *)machdr + (machdr_len >> 1)
                                           + (rxu_cntrl_env.rx_status.mesh_ctrl_len >> 1));

            // Set Ethertype
            eth_hdr->len = llc_snap->proto_id;
        }
        else
        #endif //(RW_MESH_EN)
        {
            // Pointer to the payload - Skip MAC Header
            llc_snap = (struct llc_snap *)((uint16_t *)machdr + (machdr_len >> 1));

            /*
             * Ethernet encapsulated packet structure
             ******************************************
             *  MAC HEADER  |  LLC  |  SNAP  |  DATA  |
             ******************************************
             * or
             *************************
             *  MAC HEADER  |  DATA  |
             *************************
             * The type of structure depends on the LLC fields.
             */
            if ((!memcmp(llc_snap, &rxu_cntrl_rfc1042_hdr, sizeof(rxu_cntrl_rfc1042_hdr))
                 //&& (llc_snap->ether_type != RX_ETH_PROT_ID_AARP) - Appletalk depracated ?
                 && (llc_snap->proto_id != RX_ETH_PROT_ID_IPX))
                || (!memcmp(llc_snap, &rxu_cntrl_bridge_tunnel_hdr, sizeof(rxu_cntrl_bridge_tunnel_hdr))))
            {
                // Case 1 -> Packet becomes
                /********************************************
                 *  DA  |  SA  |  SNAP->ETHERTYPE  |  DATA  |
                 ********************************************/
                /*
                 * Ethernet Header will start 6 half-words (MAC Address length is 6 bytes) before Ethertag
                 * type
                 */
                eth_hdr = (struct ethernet_hdr *)((uint16_t *)&(llc_snap->proto_id) - 6);

                // Remove length of LLC/SNAP
                payl_offset += 8;

                rxu_cntrl_env.rx_status.eth_len_present = false;
            }
            else
            {
                // Case 2 -> Packet becomes
                /********************************
                 *  DA  |  SA  |  LEN  |  DATA  |
                 ********************************/
                /*
                 * Ethernet Header will start 7 half-words (MAC Address length is 6 bytes and Length
                 * field is 2 bytes) before LLC Snap
                 */
                eth_hdr = (struct ethernet_hdr *)((uint16_t *)llc_snap - 7);

                // Set length (Initial length - MAC Header Length)
                eth_hdr->len = rx_swdesc_ptr->dma_hdrdesc->hd.frmlen - machdr_len;

                rxu_cntrl_env.rx_status.eth_len_present = true;
            }
        }

        // Set DA and SA in the Ethernet Header
        MAC_ADDR_CPY(&eth_hdr->da, &rxu_cntrl_env.rx_status.da);
        MAC_ADDR_CPY(&eth_hdr->sa, &rxu_cntrl_env.rx_status.sa);

    } while(0);

    // Update Frame Length
    rx_swdesc_ptr->dma_hdrdesc->hd.frmlen -= payl_offset;
    rxu_cntrl_env.rx_status.payl_offset = payl_offset;
}

/**
 * @return True if a free descriptor has been used, else False
 */
static void rxu_msdu_upload_and_indicate(struct rx_swdesc *p_rx_swdesc,
                                         uint8_t rx_status)
{    
    struct rx_dmadesc *dma_hdrdesc = p_rx_swdesc->dma_hdrdesc;
    struct rx_cntrl_rx_status *rx_stat = &rxu_cntrl_env.rx_status;
    GLOBAL_INT_DECLARATION();

    // Put the VIF and STA information to the RX flags
    dma_hdrdesc->flags |= (rx_stat->sta_idx << RX_FLAGS_STA_INDEX_OFT) |
                          (rx_stat->vif_idx << RX_FLAGS_VIF_INDEX_OFT) |
                          (rx_stat->dst_idx << RX_FLAGS_DST_INDEX_OFT);

#ifdef CONFIG_AOS_MESH
    if (rxu_mesh_monitor(p_rx_swdesc) == false)
#endif
    // Translate the MAC frame to an Ethernet frame
    {
    if(!bk_wlan_is_monitor_mode())
	{
	    rxu_cntrl_mac2eth_update(p_rx_swdesc);
	}
    }
    // Program the DMA transfer of the MPDU
    rxl_mpdu_transfer(p_rx_swdesc);
}

/**
 * @return True if a free descriptor has been used, else False
 */
static void rxu_mpdu_upload_and_indicate(struct rx_swdesc *p_rx_swdesc, uint8_t rx_status)
{    
    GLOBAL_INT_DECLARATION();
    struct rx_dmadesc *dma_hdrdesc = p_rx_swdesc->dma_hdrdesc;
    struct rx_cntrl_rx_status *rx_stat = &rxu_cntrl_env.rx_status;

    // Put the VIF and STA information to the RX flags
    dma_hdrdesc->flags |= (rx_stat->sta_idx << RX_FLAGS_STA_INDEX_OFT) |
                          (rx_stat->vif_idx << RX_FLAGS_VIF_INDEX_OFT) |
                          RX_FLAGS_IS_MPDU_BIT;

    #if (RW_MESH_EN)
    if (rx_stat->frame_info & RXU_CNTRL_NEW_MESH_PEER)
    {
        dma_hdrdesc->flags |= RX_FLAGS_NEW_MESH_PEER_BIT;
    }
    #endif //(RW_MESH_EN)

    // MPDU is transfered as-is, so no offset is applied
    rxu_cntrl_env.rx_status.payl_offset = 0;

	if(!bk_wlan_is_monitor_mode())
	{
    	rxu_cntrl_remove_sec_hdr_mgmt_frame(p_rx_swdesc, rx_stat);
	}

    rxl_mpdu_transfer(p_rx_swdesc);
}

static void rxu_cntrl_get_da_sa(struct mac_hdr_long *machdr_ptr)
{
    /*
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
    // Get DA
    if (machdr_ptr->fctl & MAC_FCTRL_TODS)
    {
        MAC_ADDR_CPY(&rxu_cntrl_env.rx_status.da, &machdr_ptr->addr3);
    }
    else
    {
        MAC_ADDR_CPY(&rxu_cntrl_env.rx_status.da, &machdr_ptr->addr1);
    }

    // Get SA
    if (machdr_ptr->fctl & MAC_FCTRL_FROMDS)
    {
        if (machdr_ptr->fctl & MAC_FCTRL_TODS)
        {
            MAC_ADDR_CPY(&rxu_cntrl_env.rx_status.sa, &machdr_ptr->addr4);
        }
        else
        {
            MAC_ADDR_CPY(&rxu_cntrl_env.rx_status.sa, &machdr_ptr->addr3);
        }
    }
    else
    {
        MAC_ADDR_CPY(&rxu_cntrl_env.rx_status.sa, &machdr_ptr->addr2);
    }
}

#if 1
uint32_t rxu_mtr_is_mulicast_brdcast(struct mac_hdr *machdr)
{
	uint8_t *addr = 0;
	uint32_t ds_status;
	uint32_t cast_flag = 0;
	uint64_t mac_address = 0;

	ds_status = (machdr->fctl >> 8) & 0x03; 
	switch(ds_status)
	{
		case 0:/*ToDS FromDS:0 0 ibss*/
			break;
			
		case 1:/*ToDS FromDS:1 0 infra*/
			addr = (uint8_t *)&machdr->addr3;
			break;
			
		case 2:/*ToDS FromDS:0 1 infra*/
			addr = (uint8_t *)&machdr->addr1;
			break;
			
		case 3:/*ToDS FromDS:1 1 wds*/
		default:
			break;
	}

	if(addr)
	{
		cast_flag = (MAC_ADDR_IS_BSCT(addr) || MAC_ADDR_IS_MULTI(addr));
	}
	
	return cast_flag;
}

uint32_t rxu_mtr_is_mulicast(struct mac_hdr *machdr)
{
	uint8_t *addr = 0;
	uint32_t ds_status;
	uint32_t multi_flag = 0;
	uint64_t mac_address = 0;

	ds_status = (machdr->fctl >> 8) & 0x03;	
	switch(ds_status)
	{
		case 0:/*ToDS FromDS:0 0 ibss*/
			break;
			
		case 1:/*ToDS FromDS:1 0 infra*/
			addr = (uint8_t *)&machdr->addr3;
			break;
			
		case 2:/*ToDS FromDS:0 1 infra*/
			addr = (uint8_t *)&machdr->addr1;
			break;
			
		case 3:/*ToDS FromDS:1 1 wds*/
		default:
			break;
	}

	if(addr)
	{
		multi_flag = MAC_ADDR_IS_MULTI(addr);
	}
	
	return multi_flag;
}

uint32_t rxu_mtr_is_data_or_qosdata(struct mac_hdr *machdr)
{
	uint16_t frame_cntrl = machdr->fctl;

	if((MAC_FCTRL_DATA_T == (frame_cntrl & MAC_FCTRL_TYPE_MASK))
		&& ((0x0000 == ((frame_cntrl & MAC_FCTRL_SUBT_MASK) >> 4))
			|| (frame_cntrl & 0x80)))
	{
		return 1;
	}

	return 0;
}

uint32_t rxu_mtr_record_count(struct mac_hdr *machdr)
{
	return hal_monitor_record_count(machdr);
}

uint32_t rxu_cntrl_get_group_cipher_type(uint32_t cipher_type)
{
	uint32_t type = CTYPERAM_NULL_KEY;

	switch(cipher_type)
	{
		case CIPHER_WEP:
			type = CTYPERAM_WEP;
			break;
			
		case CIPHER_TKIP:			
		case CIPHER_TKIP_COMPATIBILITY:
			type = CTYPERAM_TKIP;
			break;
			
		case CIPHER_CCMP:
			type = CTYPERAM_CCMP;
			break;
			
		case CIPHER_OPEN_SYSTEM:
		default:
			break;
	}
	
	return type;
}

uint32_t rxu_cntrl_be32(const uint8_t *a)
{
	return ((uint32_t) a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3];
}

const u8 * rxu_cntrl_get_ie(uint8_t *p_frame,
	uint32_t len, uint8_t ie)
{
	const u8 *end, *pos;

	pos = (const u8 *) p_frame;
	end = pos + len;

	while (pos + 1 < end) 
	{
		if (pos + 2 + pos[1] > end)
			break;
		
		if (pos[0] == ie)
			return pos;
		
		pos += 2 + pos[1];
	}

	return NULL;
}

const uint8_t * rxu_cntrl_get_vendor_ie(uint8_t *p_frame,
	uint32_t len, uint32_t vendor_type)
{
	const uint8_t *end, *pos;

	pos = (const uint8_t *) p_frame;
	end = pos + len;

	while (pos + 1 < end) 
	{
		if (pos + 2 + pos[1] > end)
			break;
		
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC 
			&& pos[1] >= 4 
			&& vendor_type == rxu_cntrl_be32(&pos[2]))
		{
			return pos;
		}
		
		pos += 2 + pos[1];
	}

	return NULL;
}


uint32_t rxu_cntrl_get_cipher_type_using_beacon(uint8_t *p_frame,
	uint32_t len)
{	
	int wpa;
	uint8_t *addr;	
	struct wpa_ie_data ie = {0};
	const u8 *rsn_ie, *wpa_ie;
	uint32_t support_privacy_flag = 0;
	uint16_t cap_info, cap_offset;
	uint32_t wpa_ie_len, rsn_ie_len;
	uint32_t cipher_type = CIPHER_OPEN_SYSTEM;
	uint32_t group_cipher_type;

	wpa_ie = (uint8_t *)rxu_cntrl_get_vendor_ie((uint8_t *)(p_frame + MAC_BEACON_VARIABLE_PART_OFT), len, WPA_IE_VENDOR_TYPE);
	wpa_ie_len = wpa_ie ? wpa_ie[1] : 0;

	rsn_ie = (uint8_t *)rxu_cntrl_get_ie((uint8_t *)(p_frame + MAC_BEACON_VARIABLE_PART_OFT), len, WLAN_EID_RSN);
	rsn_ie_len = rsn_ie ? rsn_ie[1] : 0;

	wpa = (wpa_ie_len > 0) || (rsn_ie_len > 0);
	cap_offset = MAC_BEACON_CAPA_OFT;
	cap_info = p_frame[cap_offset] + (p_frame[cap_offset + 1] << 8);
	support_privacy_flag = cap_info & BEACON_CAP_PRIVACY;

	if(wpa)
	{
		if(rsn_ie)
		{
			wpa_parse_wpa_ie(rsn_ie, 2 + rsn_ie[1], &ie);
			if(ie.pairwise_cipher == (WPA_CIPHER_CCMP|WPA_CIPHER_TKIP))
			{
				cipher_type = CIPHER_TKIP_COMPATIBILITY;
			}
			else if(ie.pairwise_cipher == WPA_CIPHER_CCMP)
			{
				cipher_type = CIPHER_CCMP;
			}
		}
		
		if(wpa_ie)
		{
			cipher_type = CIPHER_TKIP;
		}
	}
	else if(support_privacy_flag)
	{
		cipher_type = CIPHER_WEP;
	}

	group_cipher_type = rxu_cntrl_get_group_cipher_type(cipher_type);
		
	return group_cipher_type;
}

uint32_t rxu_cntrl_monitor_patch_using_beacon(uint8_t *p_frame,
	uint32_t len)
{
	uint8_t *addr;
	uint16_t frame_cntl;
	uint32_t cipherType;		
	uint64_t macAddress = 0;
    struct mac_hdr *machdr_ptr = (struct mac_hdr *)p_frame;

    frame_cntl = co_read16(p_frame);	
	if(MAC_FRAME_CTRL_BEACON != frame_cntl)
	{
		return 0;
	}
	
	addr = (uint8_t *)&machdr_ptr->addr3;
	os_memcpy(&macAddress,addr,sizeof(machdr_ptr->addr3));	
	if(hal_monitor_is_including_mac_address(macAddress))
	{
		return 0;
	}
	
	cipherType = rxu_cntrl_get_cipher_type_using_beacon(p_frame, len);
	hal_update_secret_key(macAddress, cipherType);
	
	return 0;
}
#endif

/**
 * Extract needed information from the MAC Header
 *
 * @param[in] frame
 */
static void rxu_cntrl_machdr_read(uint8_t *p_frame)
{
    // Map a MAC Header structure on the frame
    struct mac_hdr *machdr = (struct mac_hdr *)p_frame;

    rxu_cntrl_env.rx_status.frame_cntl = machdr->fctl;
    rxu_cntrl_env.rx_status.seq_cntl = machdr->seq;
    rxu_cntrl_env.rx_status.frame_info = 0;

    rxu_cntrl_env.rx_status.sn = machdr->seq >> MAC_SEQCTRL_NUM_OFT;
    rxu_cntrl_env.rx_status.fn = machdr->seq & MAC_SEQCTRL_FRAG_MSK;

    if ((machdr->fctl & MAC_FCTRL_QOS_DATA) == MAC_FCTRL_QOS_DATA)
    {
        uint16_t *qos;

        // Check if Address4 field is present
        if ((machdr->fctl & MAC_FCTRL_TODS_FROMDS)
                                    == MAC_FCTRL_TODS_FROMDS)
        {
            qos = (uint16_t *)&((struct mac_hdr_long_qos *)machdr)->qos;
        }
        else
        {
            qos = (uint16_t *)&((struct mac_hdr_qos *)machdr)->qos;
        }

        // Extract TID from QoS Control Field
        rxu_cntrl_env.rx_status.tid = (*qos & MAC_QOSCTRL_UP_MSK);

        #if (RW_MESH_EN)
        // Keep address of QoS Control field for later Mesh Control handling
        rxu_cntrl_env.rx_status.a_qos_ctrl = CPU2HW(qos);
        #endif //(RW_MESH_EN)
    }
    else
    {
        rxu_cntrl_env.rx_status.tid = 0;
    }

    rxu_cntrl_env.rx_status.machdr_len = rxu_cntrl_machdr_len_get(machdr->fctl);

    // Extract Destination Address and Source Address
    rxu_cntrl_get_da_sa((struct mac_hdr_long *)machdr);
}

/**
 ****************************************************************************************
 * @brief Concatenate two parts of MIC stored at different addresses and write the result
 *        in a buffer
 *
 * @param[in] mic_buffer
 * @param[in] mic_p1_len
 * @param[in] mic_p1_addr
 * @param[in] mic_p2_addr
 ****************************************************************************************
 */
static void rxu_cntrl_mic_rd_concat(uint32_t mic_buffer, uint8_t mic_p1_len,
                                    uint32_t mic_p1_addr, uint32_t mic_p2_addr)
{
    // Copy first part of MIC byte per byte
    co_copy8p(mic_buffer, mic_p1_addr, mic_p1_len);
    mic_buffer += mic_p1_len;

    // Copy second part of MIC byte per byte
    co_copy8p(mic_buffer, mic_p2_addr, MIC_LEN - mic_p1_len);
}

/**
 ****************************************************************************************
 * @brief Compare two MIC values
 *
 * @param[in] mic_value1
 * @param[in] mic_value2
 ****************************************************************************************
 */
static bool rxu_cntrl_mic_compare(uint32_t *mic_value1, uint32_t *mic_value2)
{
    return ((*mic_value1 == *mic_value2) &&
            (*(mic_value1 + 1) == *(mic_value2 + 1)));
}

/**
 ****************************************************************************************
 * @brief Send a TKIP MIC failure indication to the host based on the parameter of the
 * current packet.
 ****************************************************************************************
 */
static void rxu_cntrl_mic_failure(void)
{
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    struct me_tkip_mic_failure_ind *ind = KE_MSG_ALLOC(ME_TKIP_MIC_FAILURE_IND, TASK_API,
                                                       TASK_ME, me_tkip_mic_failure_ind);

    // Fill in parameters
    ind->addr = sta_info_tab[rx_status->sta_idx].mac_addr;
    ind->tsc = rx_status->pn;
    ind->ga = (rx_status->statinfo & RX_HD_GA_FRAME) != 0;
    ind->vif_idx = rx_status->vif_idx;
    ind->keyid = rx_status->key->key_idx;

    // Send the message
    ke_msg_send(ind);
}

/**
 ****************************************************************************************
 * @brief the MIC verification functionality provided for the LMAC in partitioned arch
 *
 * This function implements the MIC verification functionality used internally
 * by the UMAC's RX module to verify the integrity of the packets components.
 *
 * @param[in]  rx_swdesc_ptr
 * @param[in]  mic_calc_ptr
 * @param[in]  sta_idx
 * @param[in]  first_frag
 * @param[in]  last_frag
 *
 * @return      CO_OK     If all parameters of the msdu_desc are OK, MIC
 *                        verification passed successfully.
 *              CO_FAIL   If MIC verification failed
 *
 ****************************************************************************************
 */
static bool rxu_cntrl_mic_check(struct rx_swdesc *rx_swdesc_ptr, struct rxu_mic_calc *mic,
                                bool first_frag, bool last_frag)
{
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    struct mic_calc *mic_calc_ptr = &mic->mic_calc;
    /// Received MIC
    uint32_t rx_mic[2];
    uint32_t rx_mic_addr = CPU2HW(rx_mic);
    bool upload = true;

    /*
     * If first fragment, retrieve the MIC key and initialize the mic_calc structure
     * associated with the provided STAID tuple.
     */
    if (first_frag)
    {
        struct key_info_tag *key = rx_status->key;

        // Initializing mic_calc structure
        me_mic_init(mic_calc_ptr, key->u.mic.rx_key, &rx_status->da,
                        &rx_status->sa, rx_status->tid);
    }

    do
    {
        // Payload Descriptor
        struct rx_payloaddesc *pd = HW2CPU(rx_swdesc_ptr->dma_hdrdesc->hd.first_pbd_ptr);
        /*
         * Total payload Length to proceed (excluding MAC Header, IV, EIV)
         */
        uint16_t data_tot_len = rx_swdesc_ptr->dma_hdrdesc->hd.frmlen
                                                    - rx_status->machdr_len;
        /*
         * Length on which the MIC is computed
         */
        uint16_t remaining_len;
        uint16_t comp_len;
        uint16_t payl_offset;

        /*
         * If either first_frag or last_frag is False, the packet is part of a fragmented
         * MPDU.
         */
        if (!first_frag)
        {
            uint32_t last_bytes_addr = CPU2HW(mic->last_bytes);
            /*
             * If not first fragment, we should have stored 8 last bytes from the previous
             * received fragment in case these blocks contain a part of the MIC value
             */
            if (last_frag && (data_tot_len < MIC_LEN))
            {
                uint32_t data_ptr = pd->pbd.datastartptr + rx_status->machdr_len;
                uint8_t data_len = MIC_LEN - data_tot_len;

                // Continue MIC Calculation with stored bytes not part of MIC
                me_mic_calc(mic_calc_ptr, last_bytes_addr, data_tot_len);

                // Extract the MIC from the stored data and the received data
                rxu_cntrl_mic_rd_concat(CPU2HW(&rx_mic[0]), data_len,
                                        last_bytes_addr + data_tot_len,
                                        data_ptr);

                // Go to End MIC Calculation + MIC verification step
                break;
            }

            // Continue the MIC Calculation with the 8 stored bytes
            me_mic_calc(mic_calc_ptr, last_bytes_addr, MIC_LEN);
        }

        // Loop as long as the MPDU still has data on which we need to compute the MIC
        remaining_len = data_tot_len - MIC_LEN;
        payl_offset = rx_status->machdr_len;
        while (1)
        {
            uint32_t comp_addr = pd->pbd.datastartptr + payl_offset;

            // Check if we have reached the last payload buffer containing the MPDU
            if ((remaining_len + payl_offset) <= NX_RX_PAYLOAD_LEN)
            {
                // DMA only the remaining bytes of the payload
                comp_len = remaining_len;
            }
            else
            {
                // The complete payload buffer has to be DMA'ed
                comp_len = NX_RX_PAYLOAD_LEN - payl_offset;
            }

            // Compute remaining length to be computed
            remaining_len -= comp_len;

            // Compute the MIC on this payload buffer
            me_mic_calc(mic_calc_ptr, comp_addr, comp_len);

            // Check if we have finished to compute the MIC
            if (remaining_len == 0)
                break;

            // Reset the offset
            payl_offset = 0;

            // Move to the next RBD
            pd = HW2CPU(pd->pbd.next);

            // Sanity check - There shall be a payload descriptor available
            ASSERT_REC_VAL(pd != NULL, false);
        }

        // Now read the last 8 bytes
        remaining_len = MIC_LEN;
        payl_offset += comp_len;
        while (1)
        {
            uint32_t mic_addr = pd->pbd.datastartptr + payl_offset;
            uint16_t mic_len;

            // Check if we have reached the last payload buffer containing the MPDU
            if ((remaining_len + payl_offset) <= NX_RX_PAYLOAD_LEN)
            {
                // Copy only the remaining bytes of the MIC
                mic_len = remaining_len;
            }
            else
            {
                // The complete payload buffer has to copied
                mic_len = NX_RX_PAYLOAD_LEN - payl_offset;
            }

            // Copy the MIC chunk
            co_copy8p(rx_mic_addr, mic_addr, mic_len);

            // Compute the remaining length to be DMA'ed to host
            remaining_len -= mic_len;
            rx_mic_addr += mic_len;

            // Check if we have finished to compute the MIC
            if (remaining_len == 0)
                break;

            // Reset the offset
            payl_offset = 0;

            // Move to the next RBD
            pd = HW2CPU(pd->pbd.next);

            // Sanity check - There shall be a payload descriptor available
            ASSERT_REC_VAL(pd != NULL, false);
        }
    } while (0);

    if (last_frag)
    {
        // Finalizing the MIC calculation; to obtain the final MIC key
        me_mic_end(mic_calc_ptr);

        // Finally check the computed MIC value
        upload = rxu_cntrl_mic_compare(&rx_mic[0], (uint32_t *)mic_calc_ptr);
        if (!upload)
            rxu_cntrl_mic_failure();
    }
    else
    {
        // Copy the 8 bytes known as MIC in the structure
        mic->last_bytes[0] = rx_mic[0];
        mic->last_bytes[1] = rx_mic[1];
    }

    return upload;
}

/**
 *
 */
static void rxu_cntrl_defrag_mpdu_transfer(struct rx_swdesc *p_swdesc,
                                           struct rxu_cntrl_defrag *p_defrag)
{
    // DMA Addresses at which we have to copy the received data
    uint32_t hostbuf;
    // Length of data we have to copy
    uint16_t dma_len, mpdu_len;

    // DMA Descriptors
    struct dma_desc *dma_desc;
    struct dma_desc *first_dma_desc;

    // Payload Descriptors
    struct rx_payloaddesc *pd;
    struct rx_payloaddesc *prev_pd = NULL;
    struct rx_dmadesc *dma_hdrdesc = p_swdesc->dma_hdrdesc;

    // Cache the pointers for faster access
    pd = (struct rx_payloaddesc *)HW2CPU(dma_hdrdesc->hd.first_pbd_ptr);

    // Get the IPC DMA descriptor of the MAC header
    dma_desc = &pd->dma_desc;
    // Save the pointer to the first desc, as it will be passed to the DMA driver later
    first_dma_desc = dma_desc;

    // Get the MPDU body length
    mpdu_len = p_defrag->cpy_len;

    // Copy at the beginning of the host buffer + PHY VECT + 2 offset
    hostbuf = p_defrag->dma_addr;

    // Update stored DMA offset
    p_defrag->dma_addr += mpdu_len;

    // Move source data pointer in order to only copy MAC Frame without MAC Header
    dma_desc->src = pd->pbd.datastartptr + p_defrag->mac_hdr_len;

    // By default no DMA IRQ on this intermediate transfer
    dma_desc->ctrl = 0;

    // Loop as long as the MPDU still has data to copy
    while (1)
    {
        // Fill the destination address of the DMA descriptor
        dma_desc->dest = hostbuf;

        // Check if we have reached the last payload buffer containing the MPDU
        if (mpdu_len <= NX_RX_PAYLOAD_LEN)
        {
            // DMA only the remaining bytes of the payload
            dma_len = mpdu_len;
        }
        else
        {
            // The complete payload buffer has to be DMA'ed
            dma_len = NX_RX_PAYLOAD_LEN;

            // Move to the next RBD
            prev_pd = pd;
            pd = (struct rx_payloaddesc *)HW2CPU(pd->pbd.next);

            // Sanity check - There shall be a payload descriptor available
            ASSERT_REC(pd != NULL);
        }

        // Fill the DMA length in the IPC DMA descriptor
        dma_desc->length = dma_len;

        // Move the pointer in the host buffer
        hostbuf  += dma_len;
        // Compute remaining length to be DMA'ed to host
        mpdu_len -= dma_len;

        // Check if we have finished to program the payload transfer
        if (mpdu_len == 0)
        {
            break;
        }

        // Link the new descriptor with the previous one
        dma_desc->next = CPU2HW(&pd->dma_desc);

        // Retrieve the new DMA descriptor from the payload descriptor
        dma_desc = &pd->dma_desc;

        // By default no DMA IRQ on this intermediate transfer
        dma_desc->ctrl = 0;
    }

    // Setup a DMA IRQ on the last transfer
    //dma_desc->ctrl = RX_LLICTRL(1);

    // Push the DMA descriptor in the DMA engine
    dma_push(first_dma_desc, dma_desc, IPC_DMA_CHANNEL_DATA_RX);

    // Now go through the additional RBD if any (e.g. for FCS, ICV)
    while (1)
    {
        uint32_t statinfo = pd->pbd.bufstatinfo;

        // If this is the last RBD, exit the loop
        if (statinfo & RX_PD_LASTBUF)
            break;

        // Otherwise move to the next one
        prev_pd = pd;
        pd = (struct rx_payloaddesc *)HW2CPU(pd->pbd.next);

        // Sanity check - There shall be a payload descriptor available
        ASSERT_REC(pd != NULL);
    };

    // Store the pointer to the last RBD consumed by the HW. It will be used when freeing the frame
    p_swdesc->last_pbd  = &prev_pd->pbd;
    p_swdesc->spare_pbd = &pd->pbd;

    // Store the pointer to the next available RBD (for debug and error logs only)
    rx_hwdesc_env.first = HW2CPU(p_swdesc->spare_pbd->next);
}

/**
 * Go through the list of used reassembly structures in order to find a structure whose station
 * index and tid are the provided values
 *
 * @return NULL if no matching element has been found, else a pointer to the structure
 */
static struct rxu_cntrl_defrag *rxu_cntrl_defrag_get(uint8_t sta_idx, uint16_t sn, uint8_t tid)
{
    // Get the first element of the list of used Reassembly structures
    struct rxu_cntrl_defrag *p_defrag = (struct rxu_cntrl_defrag *)co_list_pick(&rxu_cntrl_env.rxu_defrag_used);

    while (p_defrag)
    {
        // Compare Station Id and TID
        if ((p_defrag->sta_idx == sta_idx)
                && (p_defrag->tid == tid)
                && (p_defrag->sn == sn))
        {
            // We found a matching structure, escape from the loop
            break;
        }

        p_defrag = (struct rxu_cntrl_defrag *)p_defrag->list_hdr.next;
    }

    // Return found element
    return (p_defrag);
}

/**
 * Allocate a defragmentation structure from the free pool. If no structure is available
 * it gets the oldest one from the used pool.
 *
 * @return The pointer to the allocated structure
 */
static struct rxu_cntrl_defrag *rxu_cntrl_defrag_alloc(void)
{
    // Get the first element of the list of used Reassembly structures
    struct rxu_cntrl_defrag *p_defrag =
           (struct rxu_cntrl_defrag *)co_list_pop_front(&rxu_cntrl_env.rxu_defrag_free);

    if (!p_defrag)
    {
        // Get the first element of the list of used Reassembly structures
        p_defrag = (struct rxu_cntrl_defrag *)co_list_pop_front(&rxu_cntrl_env.rxu_defrag_used);

        // Sanity check - There shall be an available structure
        ASSERT_ERR(p_defrag != NULL);

        // Inform the host that the allocated data buffer can be freed
        rxu_cntrl_desc_prepare(NULL, RX_STAT_DELETE, p_defrag->host_id);
    }

    // Return the allocated element
    return (p_defrag);
}

/**
 *
 */
static void rxu_cntrl_defrag_len_update(struct rx_swdesc *swdesc,
                                        struct rxu_cntrl_defrag *p_defrag)
{
    // Structure containing the RX descriptor information
    struct hal_host_rxdesc *p_rx_desc = &hal_host_rxdesc_pool[rxu_cntrl_env.rxdesc_idx];
    
    GLOBAL_INT_DECLARATION();

    // Set SW Desc address to NULL
    p_rx_desc->p_rx_swdesc = swdesc;

    // Fullfil the descriptor value structure
    if (p_defrag->eth_len_present)
    {
        p_rx_desc->rxdesc_val.status = RX_STAT_LEN_UPDATE | RX_STAT_ETH_LEN_UPDATE;
    }
    else
    {
        p_rx_desc->rxdesc_val.status = RX_STAT_LEN_UPDATE;
    }

    p_rx_desc->rxdesc_val.host_id = p_defrag->host_id;
    p_rx_desc->rxdesc_val.frame_len = p_defrag->frame_len;

    GLOBAL_INT_DISABLE();

    // Insert the element in the ready list
    co_list_push_back(&rxu_cntrl_env.rxdesc_ready, &p_rx_desc->list_hdr);

    GLOBAL_INT_RESTORE();

    // Increment the RX Desc index
    rxu_cntrl_env.rxdesc_idx = (rxu_cntrl_env.rxdesc_idx + 1) % HAL_RXDESC_CNT;
}

/**
 *
 */
static void rxu_cntrl_defrag_timeout_cb(void *env)
{
    struct rxu_cntrl_defrag *p_defrag = env;

    // Inform the host that the allocated data buffer can be freed
    rxu_cntrl_desc_prepare(NULL, RX_STAT_DELETE, p_defrag->host_id);

    // Remove the structure from the list
    co_list_extract(&rxu_cntrl_env.rxu_defrag_used, &p_defrag->list_hdr);
    co_list_push_back(&rxu_cntrl_env.rxu_defrag_free, &p_defrag->list_hdr);

    // Start the descriptor transfer
    rxu_cntrl_desc_transfer();
}

static bool rxu_cntrl_defrag_check(struct rx_swdesc *swdesc,
                                      uint8_t sta_idx, bool qos,
                                      uint16_t pdu_len)
{
    bool upload = false;
    uint8_t tid = (qos) ? rxu_cntrl_env.rx_status.tid : 0;
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    uint16_t mf = rx_status->frame_cntl & MAC_FCTRL_MOREFRAG;
    struct rxu_cntrl_defrag *p_defrag;

    do
    {
        if (!mf && !rx_status->fn)
        {
            struct rxu_mic_calc mic;

            // Perform the MIC check if required
            if ((rx_status->frame_info & RXU_CNTRL_MIC_CHECK_NEEDED) &&
                !rxu_cntrl_mic_check(swdesc, &mic, true, true))
                 break;

            // Data is not fragmented, can be forwarded
            rxu_msdu_upload_and_indicate(swdesc, RX_STAT_FORWARD | RX_STAT_ALLOC);
            upload = true;
            
            break;
        }

        // Check if a reassembly procedure is in progress
        p_defrag = rxu_cntrl_defrag_get(sta_idx, rxu_cntrl_env.rx_status.sn, tid);

        if (!p_defrag)
        {
            // If not first fragment, we can reject the packet
            if (rx_status->fn)
                break;

            // Allocate a Reassembly structure
            p_defrag = rxu_cntrl_defrag_alloc();

            // Fullfil the Reassembly structure
            p_defrag->sta_idx     = sta_idx;
            p_defrag->tid         = tid;
            p_defrag->sn          = rx_status->sn;
            p_defrag->all_rcv     = false;
            p_defrag->next_fn     = 1;
            // Reset buffer DMA Address or offset
            p_defrag->dma_addr    = 0;
            // Get MAC Header Length
            p_defrag->mac_hdr_len = rx_status->machdr_len;
            // Get Fragment Length (without MAC Header)
            p_defrag->cpy_len     = pdu_len;
            // Reset total received length
            p_defrag->frame_len   = pdu_len;
            p_defrag->timer.cb = rxu_cntrl_defrag_timeout_cb;
            p_defrag->timer.env = p_defrag;
            mm_timer_set(&p_defrag->timer, hal_machw_time() + RX_CNTRL_DEFRAG_MAX_WAIT);

            // Perform the MIC check if required
            if (rx_status->frame_info & RXU_CNTRL_MIC_CHECK_NEEDED)
                rxu_cntrl_mic_check(swdesc, &p_defrag->mic, true, false);

            // Transfer the frame to the upper layers
            rxu_msdu_upload_and_indicate(swdesc, RX_STAT_ALLOC);
            p_defrag->dma_addr = rx_status->host_buf_addr;
            p_defrag->host_id = rxu_cntrl_env.rx_ipcdesc_stat.host_id;
            p_defrag->eth_len_present = rx_status->eth_len_present;

            // Push the reassembly structure at the end of the list of used structures
            co_list_push_back(&rxu_cntrl_env.rxu_defrag_used, &p_defrag->list_hdr);

            // Data is uploaded
            upload = true;
        }
        else
        {
            uint8_t status;

            // Check the fragment is the one we are waiting for
            if (p_defrag->next_fn != rx_status->fn)
                // Packet has already been received
                break;

            // Get payload length of last fragment (can be smaller than previous ones)
            p_defrag->cpy_len = pdu_len - (uint16_t)p_defrag->mac_hdr_len;
            p_defrag->frame_len += p_defrag->cpy_len;

            // Update number of received fragment
            p_defrag->next_fn++;

            // Perform the MIC check if required
            if ((rx_status->frame_info & RXU_CNTRL_MIC_CHECK_NEEDED) &&
                !rxu_cntrl_mic_check(swdesc, &p_defrag->mic, false, (mf == 0)))
                status = RX_STAT_DELETE;
            else
                status = RX_STAT_FORWARD;

            // Data can be copied
            rxu_cntrl_defrag_mpdu_transfer(swdesc, p_defrag);

            if (mf)
            {
                // Nothing to do on this descriptor
                rxu_cntrl_desc_prepare(swdesc, RX_STAT_COPY, p_defrag->host_id);
            }
            else
            {
                // Request for a frame length update before forwarding the packet
                rxu_cntrl_defrag_len_update(swdesc, p_defrag);

                // Indicate that the packet can now be transmitted
                rxu_cntrl_desc_prepare(NULL, status, p_defrag->host_id);

                // Clear the reassembly timer
                mm_timer_clear(&p_defrag->timer);

                // Free the Reassembly structure
                co_list_extract(&rxu_cntrl_env.rxu_defrag_used, &p_defrag->list_hdr);
                co_list_push_back(&rxu_cntrl_env.rxu_defrag_free, &p_defrag->list_hdr);
            }

            upload = true;
        }

        // Store the current time
        p_defrag->time = hal_machw_time();
    } while (0);

    return (upload);
}

#if (NX_REORD)
static void rxu_cntrl_reord_update(struct rxu_cntrl_reord *p_rx_reord)
{
    // Move the windows
    p_rx_reord->elt[p_rx_reord->rx_status_pos].host_id = 0;
    p_rx_reord->win_start     = (p_rx_reord->win_start + 1) & MAC_SEQCTRL_NUM_MAX;
    p_rx_reord->rx_status_pos = (p_rx_reord->rx_status_pos + 1) % RX_CNTRL_REORD_WIN_SIZE;
}

/**
 *
 */
static void rxu_cntrl_reord_flush(struct rxu_cntrl_reord *p_rx_reord, uint16_t sn_skipped)
{
    uint16_t i;
    
    // Forward all packets that have already been received
    for (i = 0; (i < sn_skipped) && p_rx_reord->ooo_pkt_cnt; i++)
    {
        uint8_t index = (p_rx_reord->rx_status_pos + i) % RX_CNTRL_REORD_WIN_SIZE;
        struct rxu_cntrl_reord_elt *elt = &p_rx_reord->elt[index];
        uint8_t status = RX_STAT_FORWARD;

        if (elt->host_id)
        {
            if (elt->pn_check)
            {
                ASSERT_ERR(p_rx_reord->key);

                if (!rxu_cntrl_check_pn(&elt->pn, p_rx_reord->key, p_rx_reord->tid))
                    status = RX_STAT_DELETE;
            }

            // Data has already been copied in host memory and can now be forwarded
            rxu_cntrl_desc_prepare(NULL, status, elt->host_id);

            elt->host_id = 0;

            p_rx_reord->ooo_pkt_cnt--;
        }
    }

    p_rx_reord->win_start     = (p_rx_reord->win_start + sn_skipped) & MAC_SEQCTRL_NUM_MAX;
    p_rx_reord->rx_status_pos = (p_rx_reord->rx_status_pos + sn_skipped) % RX_CNTRL_REORD_WIN_SIZE;
}

/**
 * Forward packets ready for forwarding after reception of the next waited SN
 */
static void rxu_cntrl_reord_fwd(struct rxu_cntrl_reord *p_rx_reord)
{
    while (p_rx_reord->elt[p_rx_reord->rx_status_pos].host_id)
    {
        struct rxu_cntrl_reord_elt *elt = &p_rx_reord->elt[p_rx_reord->rx_status_pos];
        uint8_t status = RX_STAT_FORWARD;

        ASSERT_ERR(p_rx_reord->ooo_pkt_cnt);

        // Perform PN replay check if required
        if (elt->pn_check)
        {
            ASSERT_ERR(p_rx_reord->key);

            if (!rxu_cntrl_check_pn(&elt->pn, p_rx_reord->key, p_rx_reord->tid))
                status = RX_STAT_DELETE;
        }

        // Data has already been copied in host memory and can now be forwarded
        if (!rxu_cntrl_desc_prepare(NULL, status, elt->host_id))
        {
            break;
        }

        // Update the SN status
        rxu_cntrl_reord_update(p_rx_reord);

        p_rx_reord->ooo_pkt_cnt--;
    }
}

/*
 * Update the reordering information accordingly with the provided BlockAck Request PDU
 */
static void rxu_cntrl_reord_bar_check(uint8_t sta_idx, uint8_t *frame)
{
    // Map the BAR structure on the received frame
    struct bar_frame *bar = (struct bar_frame *)frame;
    // TID for which BA is required
    uint8_t tid = (bar->bar_cntrl & BAR_CNTL_TID_MSK) >> BAR_CNTL_TID_OFT;
    // Get the associated BA Agreement
    struct rxu_cntrl_reord *p_reord = sta_info_tab[sta_idx].ba_agmts_rx[tid];
    // Extract the Start Sequence Number
    uint16_t ssn = bar->bar_information >> MAC_SEQCTRL_NUM_OFT;

    do
    {
        if (!p_reord)
        {
            break;
        }

        // See IEEE 802.11-2012 Section 9.21.7.3 c)
        if ((ssn == p_reord->win_start) ||
            (((ssn - p_reord->win_start) & MAC_SEQCTRL_NUM_MAX) > (MAC_SEQCTRL_NUM_MAX >> 1)))
        {
            // Make no change to the record
            break;
        }

        /*
         * Flush all needed packet so that:
         *      - WinStart = SSN
         *      - WinEnd   = WinStart + WinSize - 1
         */
        rxu_cntrl_reord_flush(p_reord, (ssn - p_reord->win_start) & MAC_SEQCTRL_NUM_MAX);

        // Transfer the descriptors pushed in the ready list
        rxu_cntrl_desc_transfer();
    } while (0);
}

/*
 * Check if received frame was expected for the given virtual interface.
 *
 * @return false if the frame is a duplicate one, true else
 */
static bool rxu_cntrl_reord_check(struct rx_swdesc *swdesc, uint8_t sta_idx)
{
    // Returned status
    bool upload = true;
    // SN position in the sn status bit field
    uint16_t sn_pos;
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;

    do
    {
        struct rxu_cntrl_reord *p_reord
                    = sta_info_tab[sta_idx].ba_agmts_rx[rx_status->tid];

        // Check if the received packet had the lowest expected SN
        if (rx_status->sn == p_reord->win_start)
        {
            // Perform the PN check if required
            if (rx_status->frame_info & RXU_CNTRL_PN_CHECK_NEEDED)
                upload = rxu_cntrl_check_pn(&rx_status->pn, rx_status->key, rx_status->tid);

            // Check if we need to upload the frame
            if (upload)
            {
                rxu_msdu_upload_and_indicate(swdesc, RX_STAT_FORWARD | RX_STAT_ALLOC);
            }

            // Store current time
            p_reord->sn_rx_time = hal_machw_time();

            // Update the RX Window
            rxu_cntrl_reord_update(p_reord);

            // And forward any ready frames
            rxu_cntrl_reord_fwd(p_reord);
        }
        else
        {
            sn_pos = (rx_status->sn - p_reord->win_start) & MAC_SEQCTRL_NUM_MAX;

            if (sn_pos >= RX_CNTRL_REORD_WIN_SIZE)
            {
                if (sn_pos < (MAC_SEQCTRL_NUM_MAX >> 1))
                {
                    // Move the window
                    rxu_cntrl_reord_flush(p_reord, sn_pos - RX_CNTRL_REORD_WIN_SIZE + 1);

                    // Recompute the SN position
                    sn_pos = (rx_status->sn - p_reord->win_start) & MAC_SEQCTRL_NUM_MAX;
                }
                else
                {
                    // Discard the MPDU
                    upload = false;
                    break;
                }
            }

            sn_pos = (sn_pos + p_reord->rx_status_pos) % RX_CNTRL_REORD_WIN_SIZE;

            // Check if the packet has already been received
            if (p_reord->elt[sn_pos].host_id)
            {
                // Discard the MPDU
                upload = false;
                break;
            }

            // Store the PN and keys if required
            if (rx_status->frame_info & RXU_CNTRL_PN_CHECK_NEEDED)
            {
                p_reord->key = rx_status->key;
                p_reord->elt[sn_pos].pn = rx_status->pn;
                p_reord->elt[sn_pos].pn_check = true;
            }
            else
            {
                p_reord->elt[sn_pos].pn_check = false;
            }

            // Data has been received out of order
            rxu_msdu_upload_and_indicate(swdesc, RX_STAT_ALLOC);

            // Store the Host ID in the reordering element
            p_reord->elt[sn_pos].host_id = rxu_cntrl_env.rx_ipcdesc_stat.host_id;

            p_reord->ooo_pkt_cnt++;
        }

        // Indicate the activity on this RX BlockAck agreement
        bam_rx_active(sta_idx, rx_status->tid);
    } while (0);

    return (upload);
}

static void rxu_cntrl_reord_timeout_cb(void *env)
{
    struct rxu_cntrl_reord *p_reord = env;

    // Restart reordering timer
    mm_timer_set(&p_reord->timer, p_reord->timer.time + RX_CNTRL_REORD_MAX_WAIT);

    do
    {
        // Check if there is at least a packet waiting
        if (!p_reord->ooo_pkt_cnt)
            break;

        // Check if we spent too much time waiting for an SN
        if (!hal_machw_time_past(p_reord->sn_rx_time + RX_CNTRL_REORD_MAX_WAIT))
            break;

        if (!p_reord->elt[p_reord->rx_status_pos].host_id)
        {
            // Consider the next waited packet as received
            rxu_cntrl_reord_update(p_reord);
        }
        // Send the data
        rxu_cntrl_reord_fwd(p_reord);

        // If list of descriptor ready for upload was empty and now contains some elements, start transfer
        if (co_list_pick(&rxu_cntrl_env.rxdesc_ready))
        {
            rxu_cntrl_desc_transfer();
        }
    } while (0);
}
#endif //(RX_REORD)

/**
 * Check if the received packet is the same than the previous received one
 * in the case where the STA is not yet registered.
 */
static bool rxu_cntrl_duplicate_nsta_check(uint8_t *p_frame)
{
    // Returned status
    bool upload = false;

    do
    {
        // Map a MAC Header structure on the frame
        struct mac_hdr *machdr = (struct mac_hdr *)p_frame;

        // Check retry bit, source address and received sequence control value
        if ((machdr->fctl & MAC_FCTRL_RETRY) &&
            (rxu_cntrl_env.rxu_dupli.last_seq_cntl == machdr->seq) &&
            (!memcmp(&machdr->addr2, &rxu_cntrl_env.rxu_dupli.last_src_addr, sizeof(struct mac_addr))))
        {
            // Reject the packet
            break;
        }

        rxu_cntrl_env.rxu_dupli.last_seq_cntl = machdr->seq;
        rxu_cntrl_env.rxu_dupli.last_src_addr = machdr->addr2;

        upload = true;
    } while (0);

    return (upload);
}

/**
 * Check if the received packet is the same than the previous received one
 * in the case where the STA is registered.
 */
static bool rxu_cntrl_duplicate_check(uint16_t frame_cntrl, uint8_t sta_idx, uint8_t qos)
{
    // Returned status
    bool upload = false;

    do
    {
        uint16_t *last_seq_cntl;

        if (qos)
        {
            last_seq_cntl = &(sta_info_tab[sta_idx].rx_qos_last_seqcntl[rxu_cntrl_env.rx_status.tid]);
        }
        else
        {
            last_seq_cntl = &(sta_info_tab[sta_idx].rx_nqos_last_seqcntl);
        }

        // Check retry bit and received sequence control value
        if ((frame_cntrl & MAC_FCTRL_RETRY) &&
            (*last_seq_cntl == rxu_cntrl_env.rx_status.seq_cntl))
        {
            // Reject the packet
            break;
        }

        *last_seq_cntl = rxu_cntrl_env.rx_status.seq_cntl;

        upload = true;
    } while (0);

    return (upload);
}

static void rxu_cntrl_pm_mon_check(uint8_t *p_frame, uint32_t statinfo)
{
    // Map a MAC Header structure on the frame
    struct mac_hdr *hdr = (struct mac_hdr *)p_frame;

    do
    {
        // Check if the PM monitoring is enabled
        if (!rxu_cntrl_env.pm_mon.mon)
            break;

        // Check if the packet is for the local device
        if (statinfo & RX_HD_ADDRMIS)
            break;

        // Compare the MAC address with the expected one
        if (!MAC_ADDR_CMP(&rxu_cntrl_env.pm_mon.addr, &hdr->addr2))
            break;

        // Check if the peer device is going to sleep or not
        if ((hdr->fctl & (MAC_FCTRL_PWRMGT | MAC_FCTRL_MOREFRAG)) == MAC_FCTRL_PWRMGT)
            // Device is going to sleep
            rxu_cntrl_env.pm_mon.pm_state = PS_MODE_ON;
        else
            // Device is going to be active
            rxu_cntrl_env.pm_mon.pm_state = PS_MODE_OFF;
    } while(0);
}

/**
 ****************************************************************************************
 * @brief function routes the action frame based on its type
 *
 * @param[in] param Pointer to the parameters of the message.
 *
 * @return A boolean indicating if the frame is handled internally, or needs to be forwarded
 * to the host.
 ****************************************************************************************
 */
static bool rxu_mgt_route_action(uint32_t *payload, uint16_t length, uint8_t sta_idx, uint8_t *vif_idx,
                                 ke_task_id_t *task_id, uint16_t machdr_length, bool *need_machdr)
{
    // Extract the category and action type from the frame
    uint32_t addr           = CPU2HW(payload) + machdr_length;
    uint8_t action_category = co_read8p(addr + MAC_ACTION_CATEGORY_OFT);
    uint16_t task_idx       = 0xFF;
    bool upload = false;

    // Handle action frames for known stations only
    if (sta_idx == INVALID_STA_IDX)
    {
        #if (RW_MESH_EN)
        // If Action Category is Self-Protected, authorize frames from unknown STA
        if (action_category != MAC_SELF_PROT_ACTION_CATEGORY)
        #endif //(RW_MESH_EN)
        {
            return true;
        }
    }

    switch (action_category)
    {
        case MAC_BA_ACTION_CATEGORY:
        {
            *task_id = TASK_BAM;
            task_idx = 0;
        } break;

        #if NX_VHT
        case MAC_VHT_ACTION_CATEGORY:
        {
            uint8_t vht_action = co_read8p(addr + MAC_ACTION_ACTION_OFT);
            switch (vht_action)
            {
                case MAC_OP_MODE_NOTIF_VHT_ACTION:
                {
                    uint8_t opmode = co_read8p(addr + MAC_OP_MODE_NOTIF_OPMODE_OFT);
                    struct sta_info_tag *sta = &sta_info_tab[sta_idx];
                    struct mac_sta_info *info = &sta->info;

                    // This frame is valid if peer device is VHT capable
                    if ((info->capa_flags & STA_VHT_CAPA) &&
                        !(opmode & MAC_OPMODE_RXNSS_TYPE_BIT))
                    {
                        uint8_t bw = (opmode & MAC_OPMODE_BW_MSK) >> MAC_OPMODE_BW_OFT;
                        uint8_t nss = (opmode & MAC_OPMODE_RXNSS_MSK) >> MAC_OPMODE_RXNSS_OFT;

                        // Update maximum supported bandwidth
                        me_sta_bw_nss_max_upd(sta_idx, bw, nss);
                    }
                } break;

                #if RW_MUMIMO_RX_EN
                case MAC_GROUP_ID_MGMT_VHT_ACTION:
                {
                    // Set the Membership Status and User Position arrays to the PHY
                    phy_set_group_id_info(addr + MAC_GROUP_ID_MGT_MEMBERSHIP_OFT,
                                          addr + MAC_GROUP_ID_MGT_USER_POS_OFT);
                } break;
                #endif

                default:
                {
                    // Other VHT action categories are not supported
                    upload = true;
                } break;
            }
        } break;
        #endif

        case MAC_HT_ACTION_CATEGORY:
        {
            uint8_t ht_action = co_read8p(addr + MAC_ACTION_ACTION_OFT);
            switch (ht_action)
            {
                case MAC_CHAN_WIDTH_HT_ACTION:
                {
                    uint8_t chwidth = co_read8p(addr + MAC_CHAN_WIDTH_WIDTH_OFT);
                    struct sta_info_tag *sta = &sta_info_tab[sta_idx];
                    struct mac_sta_info *info = &sta->info;

                    // This frame is valid if peer device is HT capable
                    if ((info->capa_flags & STA_HT_CAPA) && (chwidth <= BW_40MHZ))
                        // Update maximum supported bandwidth
                        me_sta_bw_nss_max_upd(sta_idx, chwidth, 0xFF);
                } break;

                case MAC_SMPS_HT_ACTION:
                {
                    struct sta_info_tag *sta = &sta_info_tab[sta_idx];
                    struct mac_sta_info *info = &sta->info;

                    // This frame is valid if peer device is HT capable
                    if (info->capa_flags & STA_HT_CAPA)
                    {
                        uint8_t sm_pwr_ctrl = co_read8p(addr + MAC_SM_PRW_CTRL_OFT);
                        if (sm_pwr_ctrl & MAC_SMPS_ENABLE_BIT)
                            // Limit NSS for this station to 1
                            me_sta_bw_nss_max_upd(sta_idx, 0xFF, 0);
                        else
                            // No limit for the NSS to be used
                            me_sta_bw_nss_max_upd(sta_idx, 0xFF, 0xFF);
                    }
                } break;

                default:
                {
                    // Other HT action categories are not supported
                    upload = true;
                } break;
            }
        } break;

        #if (RW_MESH_EN)
        case (MAC_SELF_PROT_ACTION_CATEGORY):
        {
            // Get Mesh VIF on which action frame has been received
            struct mesh_vif_info_tag *p_mvif_entry;

            if (*vif_idx == INVALID_VIF_IDX)
            {
                // Check if the received frame contains Mesh ID IE
                uint32_t ie_addr = mac_ie_find(CPU2HW(payload) + MAC_SELF_PROT_ACTION_CAPA_LEN,
                                               length - MAC_SELF_PROT_ACTION_CAPA_LEN,
                                               MAC_ELTID_MESH_ID);

               if (!ie_addr)
               {
                   break;
               }

               // Get addressed Mesh VIF based on received Mesh ID
               p_mvif_entry = mesh_check_mesh_id(ie_addr);

               if (!p_mvif_entry)
               {
                   break;
               }

               // Update the VIF information
               *vif_idx = p_mvif_entry->vif_idx;
            }
            else
            {
                struct vif_info_tag *p_vif_entry = &vif_info_tab[*vif_idx];

                p_mvif_entry = &mesh_vif_info_tab[p_vif_entry->mvif_idx];
            }

            if (p_mvif_entry->user_mpm)
            {
                // These frames are used for MPM, upload them
                upload = true;
                break;
            }
        } // no break

        case (MAC_MESH_ACTION_CATEGORY):
        {
            *task_id = TASK_MESH;
            task_idx = 0;
            *need_machdr = true;
        } break;
        #endif //(RW_MESH_EN)

        default:
        {
            // Other action categories are not supported
            upload = true;
        } break;
    }

    // format the TaskId taking in to account the instance index
    if (*task_id != TASK_NONE)
    {
        // Sanity check
        ASSERT_ERR(task_idx != 0xFF);
        *task_id = KE_BUILD_ID(*task_id, task_idx);
    }

    return upload;
}

static bool rxu_mgt_route(uint16_t framectrl,
                          uint16_t length,
                          uint16_t machdr_length,
                          uint8_t sta_idx,
                          uint8_t *vif_idx,
                          uint32_t *payload,
                          ke_task_id_t *task_id,
                          bool *need_machdr)
{    
    bool upload;
    struct vif_info_tag *vif_entry = NULL;
    uint8_t mode = VIF_UNKNOWN;

	if(g_wlan_general_param->role == CONFIG_ROLE_AP)
	{
    upload = false;
	}

	if(g_wlan_general_param->role == CONFIG_ROLE_STA)
	{
    upload = true;
	}

    // Get the VIF entry
    if (*vif_idx != INVALID_VIF_IDX)
    {
        vif_entry = &vif_info_tab[*vif_idx];
        mode = vif_entry->type;
    }

    // Route the message depending on its type and subtype
    switch (framectrl & MAC_FCTRL_TYPESUBTYPE_MASK)
    {                  
        // Message arrive to STA in ESS only
        case MAC_FCTRL_ASSOCRSP:
        case MAC_FCTRL_REASSOCRSP:
			if(g_wlan_general_param->role == CONFIG_ROLE_STA)
			{
            if (mode == VIF_STA)
            {
                *task_id = TASK_SM;
            }
			}
            break;

        case MAC_FCTRL_PROBERSP:
            // Check if the ProbeRsp was for us
            if (vif_entry)
            {
                *need_machdr = true;
                *task_id = TASK_SCANU;
            }
            break;

        case MAC_FCTRL_BEACON:
        {
            // Only forward beacons to SCANU task if we are in scanning state
            if (ke_state_get(TASK_SCANU) == SCANU_SCANNING)
            {
                *need_machdr = true;
                *task_id = TASK_SCANU;
            }

            #if (RW_MESH_EN)
            do
            {
                struct mesh_vif_info_tag *p_mvif_entry;

                if (*task_id != TASK_NONE)
                {
                    break;
                }

                if (*vif_idx == INVALID_VIF_IDX)
                {
                    // Check if the received beacon is a mesh beacon (contains Mesh ID IE)
                    uint32_t ie_addr = mac_ie_find(CPU2HW(payload) + MAC_BEACON_VARIABLE_PART_OFT,
                                                   length - MAC_BEACON_VARIABLE_PART_OFT,
                                                   MAC_ELTID_MESH_ID);

                   if (!ie_addr)
                   {
                       break;
                   }

                   // Get addressed Mesh VIF based on received Mesh ID
                   p_mvif_entry = mesh_check_mesh_id(ie_addr);

                   if (!p_mvif_entry)
                   {
                       break;
                   }

                   // Update the VIF information
                   *vif_idx = p_mvif_entry->vif_idx;
                   vif_entry = &vif_info_tab[*vif_idx];
                }
                else
                {
                    p_mvif_entry = &mesh_vif_info_tab[vif_entry->mvif_idx];
                }

                if (!p_mvif_entry->user_mpm)
                {
                    // Forward the frame to the mesh task
                    *need_machdr = true;
                    *task_id = TASK_MESH;
                }
                else
                {
                    if (sta_idx == INVALID_STA_IDX)
                    {
                        // Map a MAC Header on the frame
                        struct mac_hdr *p_mac_hdr = (struct mac_hdr *)payload;

                        if (mesh_accept_new_peer(&p_mac_hdr->addr2, *vif_idx,
                                                 CPU2HW(payload) + MAC_BEACON_VARIABLE_PART_OFT,
                                                 length - MAC_BEACON_VARIABLE_PART_OFT))
                        {
                            rxu_cntrl_env.rx_status.frame_info |= RXU_CNTRL_NEW_MESH_PEER;
                        }
                        else
                        {
                            upload = false;
                        }
                    }
                }
            } while (0);
            #endif //(RW_MESH_EN)

            // Handle a received beacon if sent by a peer AP
            if (sta_idx != INVALID_STA_IDX)
            {
                if (vif_entry && vif_entry->active)
                {
                    me_beacon_check(*vif_idx, length, CPU2HW(payload));
                }
            }
            // else keep TASK_NONE as destination task
        } break;

        case MAC_FCTRL_DISASSOC:
        case MAC_FCTRL_AUTHENT:
        case MAC_FCTRL_DEAUTHENT:
            // routed to SM task (case STA in ESS network)
            if (mode == VIF_STA)
            {
                *task_id = TASK_SM;
            }
            break;

        case MAC_FCTRL_ACTION:
            // Route the action frame based on its category and action type
            // in addition to the sta_mode (AP, STA)
            upload = rxu_mgt_route_action(payload, length, sta_idx, vif_idx, task_id, machdr_length, need_machdr);
            break;
    }

    return upload;
}

int rxu_mgt_monitor(uint16_t framectrl,
                              uint32_t *payload,
                              uint16_t length)
{
	monitor_data_cb_t fn;
	int upload = 0;
	
	if(MAC_FCTRL_DATA_T != (framectrl & MAC_FCTRL_TYPE_MASK))
	{
		if(length < 36)
		{
			goto mgt_exit;
		}
		
		rxu_cntrl_monitor_patch_using_beacon((uint8_t *)payload, length);
		
		fn = bk_wlan_get_monitor_cb();		
		(*fn)((uint8_t *)payload, length, NULL);
	}
	else
	{
		upload = 1;
	}

mgt_exit:
	return upload;
}

static bool rxu_mgt_frame_ind(uint16_t framectrl,
                              uint16_t length,
                              uint8_t sta_idx,
                              uint8_t *vif_idx,
                              int8_t rssi,
                              uint32_t *payload,
                              uint16_t machdr_length)
{
    ke_task_id_t dest_id = TASK_NONE;
    bool copy_mac_hdr = false;
    bool upload;
	monitor_data_cb_t fn;
	
    // Route the message to the correct task and/or handle it immediately
    upload = rxu_mgt_route(framectrl, 
							length, 
							machdr_length, 
							sta_idx, 
							vif_idx, 
							payload,
							&dest_id, 
							&copy_mac_hdr);

	fn = bk_wlan_get_mgnt_monitor_cb();
	if (fn) {
		fn(payload, length, NULL);
	}
    // Check if the message has to be forwarded or not
    if (((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_BEACON) 
            && (sta_idx != INVALID_STA_IDX))
    {        
        #if CFG_WIFI_STATION_MODE
		#if CFG_MODE_SWITCH
		if(g_wlan_general_param->role == CONFIG_ROLE_STA)
		#endif
        upload = false;
        #endif
    }

    #if CFG_WIFI_STATION_MODE
	#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_STA)
	#endif
	{
	    if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_PROBEREQ)
		{
			RXU_PRT("-------MAC_FCTRL_PROBEREQ:0x%x\r\n", length);
	        upload = false;
		}
	    if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_PROBERSP)
		{
			RXU_PRT("-------MAC_FCTRL_PROBERSP:0x%x\r\n", length);
		}
	}
    #endif

	#if CFG_WIFI_AP_MODE
	#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP){
	#endif
	if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_ASSOCREQ)
	{
		RXU_PRT("-------MAC_FCTRL_ASSOCREQ:0x%x\r\n", length);
		dest_id = TASK_API;
        upload = true;
	}
	else if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_AUTHENT)
	{
		RXU_PRT("-------MAC_FCTRL_AUTHENT:0x%x\r\n", length);
		dest_id = TASK_API;
        upload = true;
	}
	#if 0
	else if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_PROBEREQ)
	{
		RXU_PRT("-------MAC_FCTRL_PROBEREQ:0x%x\r\n", length);
		dest_id = TASK_API;
	}
	else if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_REASSOCREQ)
	{
		RXU_PRT("-------MAC_FCTRL_REASSOCREQ:0x%x\r\n", length);
		dest_id = TASK_API;
	}
	else if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_ACTION)
	{
		RXU_PRT("-------MAC_FCTRL_ACTION:0x%x\r\n", length);
		dest_id = TASK_API;
	}
	#endif
	else if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_DISASSOC)
	{
		RXU_PRT("-------MAC_FCTRL_DISASSOC:0x%x\r\n", length);
		dest_id = TASK_API;
	}
	else if((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_DEAUTHENT)
	{
		RXU_PRT("-------MAC_FCTRL_DEAUTHENT:0x%x\r\n", length);
		dest_id = TASK_API;
	}
	else
	{
		if((0x40 != (framectrl & MAC_FCTRL_TYPESUBTYPE_MASK))
			&& (0x80 != (framectrl & MAC_FCTRL_TYPESUBTYPE_MASK)))
		{
			RXU_PRT("-------mgmt:0x%x:0x%x\r\n", (framectrl & MAC_FCTRL_TYPESUBTYPE_MASK), length);
		}
	}

	#if 1 // discard wep iv headr
	if(0x40b0 == framectrl)
	{
		#define WEP_IV_LENGTH                   4
		#define FRM_CTRL_HDR_LENGTH             24
		
		char *tmp = (char *)payload;
		
		os_memcpy(tmp + FRM_CTRL_HDR_LENGTH, 
						tmp + FRM_CTRL_HDR_LENGTH + WEP_IV_LENGTH, 
						length - FRM_CTRL_HDR_LENGTH - WEP_IV_LENGTH);
		
		length -= WEP_IV_LENGTH;
	}
	#endif
	#if CFG_MODE_SWITCH
	}
	#endif
	#endif // CFG_WIFI_AP_MODE
	
	if(bk_wlan_is_monitor_mode())
	{
		return rxu_mgt_monitor(framectrl, payload, length);
	}
	
    // Check if the message has to be forwarded or not
    if (dest_id != TASK_NONE)
    {
    	#if CFG_WIFI_AP_MODE
		#if CFG_MODE_SWITCH
		if(g_wlan_general_param->role == CONFIG_ROLE_AP)
		#endif
		{
			ke_mgmt_packet_tx((unsigned char *)payload, length, 0);
		}
		#endif

		#if CFG_WIFI_STATION_MODE
		#if CFG_MODE_SWITCH
		if(g_wlan_general_param->role == CONFIG_ROLE_STA)
		{
		#endif		
	        // Allocate the message
	        struct rxu_mgt_ind *rx;
	        struct phy_channel_info info;

			rx = KE_MSG_ALLOC_VAR(RXU_MGT_IND, dest_id, TASK_RXU, rxu_mgt_ind, length);
			
	        // Get the information on the current channel from the PHY driver
	        phy_get_channel(&info, PHY_PRIM);

	        // Fill in the parameter structure fields that can be filled now
	        if (!copy_mac_hdr)
	        {
	            ASSERT_WARN((machdr_length & 0x1) == 0);
	            length -= machdr_length;
	            payload = HW2CPU(CPU2HW(payload) + machdr_length);
	        }
			
	        rx->length = length;
	        co_copy32(rx->payload, payload, length);
	        rx->inst_nbr = *vif_idx;
	        rx->sta_idx = sta_idx;
	        rx->framectrl = framectrl;
	        rx->rssi = rssi;
	        rx->center_freq = (info.info1 >> 16) & 0xFFFF;
	        rx->band = info.info1 & 0xFF;

	        ke_msg_send(rx);
			#if CFG_MODE_SWITCH
		}
		#endif
		#endif

        upload = false;
    }

    return (upload);
}

static uint8_t rxu_mgt_search_rx_vif(struct mac_hdr *hdr, uint32_t statinfo)
{
    uint8_t vif_idx = INVALID_VIF_IDX;

    if (!(statinfo & (RX_HD_ADDRMIS | RX_HD_GA_FRAME)))
    {
        struct vif_info_tag *vif_entry = vif_mgmt_first_used();

        // Go through the VIF entries to find if one is matching the ADDR1 we got
        while (vif_entry != NULL)
        {
            if (MAC_ADDR_CMP(&vif_entry->mac_addr, &hdr->addr1))
            {
                vif_idx = vif_entry->index;
                break;
            }
            vif_entry = (struct vif_info_tag *)co_list_next(&vif_entry->list_hdr);
        }
    }
    else if (statinfo & RX_HD_GA_FRAME)
    {
        // If ProbeReq look at the address 3 to check if it is ours
    }

    return (vif_idx);
}

/**
 ****************************************************************************************
 * @brief This function processes the received frames that could carry useful information
 * for some LMAC features (connection monitoring, power-save mode, etc.)
 *
 * @param[in] swdesc SW header descriptor of the frame
 ****************************************************************************************
 */
static bool rxu_mgt_frame_check(struct rx_swdesc* swdesc, uint8_t sta_idx)
{
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
    struct rx_hd *rhd = &dma_hdrdesc->hd;
    struct rx_payloaddesc *payl_d = HW2CPU(rhd->first_pbd_ptr);
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    uint32_t *frame = payl_d->buffer;
    struct mac_hdr *hdr = (struct mac_hdr *)frame;
    bool upload = true;
    int8_t rssi;

    do
    {
        // Currently no support of fragmented management frames in UMAC
        if ((hdr->fctl & MAC_FCTRL_MOREFRAG) 
                || (hdr->seq & MAC_SEQCTRL_FRAG_MSK))
        {
            break;
        }

        // Retrieve the RSSI value from the RX vectors
        rssi = (rhd->recvec1c >> 24) & 0xFF;

        // Check if the sending STA is known. If not we search the VIF
        // index receiving the frame, if there is one
        if (sta_idx == INVALID_STA_IDX)
            rx_status->vif_idx = rxu_mgt_search_rx_vif(hdr, rhd->statinfo);

        #if NX_MFP
        if (!mfp_ignore_mgmt_frame(rx_status, frame, rhd->frmlen, &upload))
        #endif

#ifdef CONFIG_AOS_MESH
        if (rxu_mesh_monitor(swdesc) == true) {
            upload = false;
        }
        else
#endif
        {
            upload = rxu_mgt_frame_ind(hdr->fctl, 
                                        rhd->frmlen, 
                                        sta_idx, 
                                        &rx_status->vif_idx,
                                        rssi, 
                                        frame, 
                                        rx_status->machdr_len);		
		if(!upload)
		{
			break;
		}
		
         }
    	rxu_mpdu_upload_and_indicate(swdesc, RX_STAT_FORWARD | RX_STAT_ALLOC);
    } while (0);

    return (upload);
}

static void rxu_ipcdesc_init(void)
{
    uint32_t i;
    
    for (i = 0; i < HAL_RXDESC_CNT; i++)
    {
        // No data buffer directly attached to the RHD
        hal_host_rxdesc_pool[i].dma_desc.src    = CPU2HW(&hal_host_rxdesc_pool[i].rxdesc_val.host_id);
        hal_host_rxdesc_pool[i].dma_desc.length = 9;
        hal_host_rxdesc_pool[i].dma_desc.ctrl   = 0;
        hal_host_rxdesc_pool[i].rxdesc_val.status = 0;
    }
}

/*
 * PUBLIC FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief initializes the rx_context.
 *
 * This function is called when initializing the UMAC's Rx Context at system
 * start up or at system reset
 ****************************************************************************************
 */
void rxu_cntrl_init(void)
{
    uint16_t i;
    
    co_list_init(&rxu_cntrl_env.rxdesc_ready);
    co_list_init(&rxu_cntrl_env.rxdesc_pending);

    #if (NX_REORD)
    co_list_init(&rxu_cntrl_env.rxu_reord_free);

    for (i = 0; i < RX_CNTRL_REORD_POOL_SIZE; i++)
    {
        co_list_push_back(&rxu_cntrl_env.rxu_reord_free, &rxu_cntrl_reord_pool[i].list_hdr);
    }
    #endif //(NX_REORD)

    co_list_init(&rxu_cntrl_env.rxu_defrag_free);
    co_list_init(&rxu_cntrl_env.rxu_defrag_used);

    for (i = 0; i < RX_CNTRL_DEFRAG_POOL_SIZE; i++)
    {
        co_list_push_back(&rxu_cntrl_env.rxu_defrag_free, &rxu_cntrl_defrag_pool[i].list_hdr);
    }

    rxu_cntrl_env.rxu_dupli.last_seq_cntl = 0xFFFF;
}

bool rxu_cntrl_frame_handle(struct rx_swdesc* swdesc)
{
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
    struct rx_hd *rhd = &dma_hdrdesc->hd;
    uint32_t statinfo = rhd->statinfo;
    struct rx_pbd *pd = HW2CPU(rhd->first_pbd_ptr);
    uint8_t *frame = HW2CPU(pd->datastartptr);
    bool qos, upload = false;
    uint16_t frame_cntl, key_idx_hw;
    uint8_t sta_idx;
    uint8_t monitor_failed = 0;

    do
    {
        frame_cntl = co_read16(frame);
        if (!(statinfo & RX_HD_SUCCESS))
        {
            break;
        }
		else
		{			
        	if(bk_wlan_is_monitor_mode()) 
        	{        		
        		if((MAC_FCTRL_CTRL_T == (frame_cntl & MAC_FCTRL_TYPE_MASK))
					|| (MAC_FCTRL_RSV_T == (frame_cntl & MAC_FCTRL_TYPE_MASK)))
        		{
        			break;
        		}
				
        		if(MAC_FCTRL_DATA_T == (frame_cntl & MAC_FCTRL_TYPE_MASK))
        		{  
					if(!(rxu_mtr_is_mulicast_brdcast((struct mac_hdr *)frame)
					   && rxu_mtr_is_data_or_qosdata((struct mac_hdr *)frame)))
					{
						break;
					}
        		}

				rxu_mtr_record_count((struct mac_hdr *)frame);
        	}
		}		
		
        dma_hdrdesc->flags = 0;
        rx_status->sta_idx = INVALID_STA_IDX;
        rx_status->vif_idx = INVALID_VIF_IDX;

        // Read needed information in the MAC Header
        rxu_cntrl_machdr_read(frame);

        if (!(statinfo & RX_HD_KEYIDV))
        {
            // Perform the check on the PM bit if required
            rxu_cntrl_pm_mon_check(frame, statinfo);

            // When the sender is unknown, only management frames are handled here
#ifdef CONFIG_AOS_MESH
            if(rxu_mesh_monitor(swdesc) == false)
#endif
            {
            if(!bk_wlan_is_monitor_mode())
            {
	            if ((frame_cntl & MAC_FCTRL_TYPE_MASK) != MAC_FCTRL_MGT_T)
	                break;

	            // Authentication can be encrypted (when using SHARED-KEY)
	            if (RXL_CNTRL_IS_PROTECTED(rx_status->frame_cntl) &&
	                (((statinfo & RX_HD_DECRSTATUS) != RX_HD_DECR_WEPSUCCESS) ||
	                 !rxu_cntrl_protected_handle(frame, statinfo)))
	                break;
            }
			
            if (!rxu_cntrl_duplicate_nsta_check(frame))
                break;
            }

            upload = rxu_mgt_frame_check(swdesc, INVALID_STA_IDX);
            break;
        }

        // Get the HW key index
        key_idx_hw = (uint16_t)RX_HD_KEYID_GET(statinfo);

        // Retrieve the station index and instance number
        sta_idx = (uint8_t)(key_idx_hw - MM_SEC_DEFAULT_KEY_COUNT);

        // Check if the STA is registered
        if (!sta_mgmt_is_valid(sta_idx))
        {
            break;
        }

        rx_status->sta_idx = sta_idx;
        rx_status->vif_idx = sta_info_tab[sta_idx].inst_nbr;

        // Save the status information field
        rx_status->statinfo = statinfo;

        #if NX_BEACONING
        // In AP mode, check if the destination address is part of our known addresses
        if ((vif_info_tab[rx_status->vif_idx].type == VIF_AP) &&
            (!MAC_ADDR_GROUP(&rx_status->da)))
        {
            rx_status->dst_idx = hal_machw_search_addr(&rx_status->da);
        }
        #endif

        // Check 4 addresses
        if ((frame_cntl & MAC_FCTRL_TODS_FROMDS) == MAC_FCTRL_TODS_FROMDS)
        {
            dma_hdrdesc->flags |= RX_FLAGS_4_ADDR_BIT;
        }

        // Check if received frame was encrypted
        if (RXL_CNTRL_IS_PROTECTED(rx_status->frame_cntl) &&
            !rxu_cntrl_protected_handle(frame, rx_status->statinfo))
            break;

        switch (frame_cntl & MAC_FCTRL_TYPE_MASK)
        {
            case MAC_FCTRL_CTRL_T:
                #if (NX_REORD)
                if ((frame_cntl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_BAR)
                {
                    // Check the Start Sequence Numbers contained by the receive Block Ack Request PDU
                    rxu_cntrl_reord_bar_check(sta_idx, frame);
                }
                #endif //(NX_REORD)
                break;

            case MAC_FCTRL_MGT_T:
                // Perform the duplicate check
                if (!rxu_cntrl_duplicate_check(frame_cntl, sta_idx, false))
                    break;

                // Perform the PN check if required
                if ((rx_status->frame_info & RXU_CNTRL_PN_CHECK_NEEDED) &&
                    !rxu_cntrl_check_pn(&rx_status->pn, rx_status->key, TID_MGT))
                     break;

                // Now check if the management is of interest for us
                upload = rxu_mgt_frame_check(swdesc, rx_status->sta_idx);
                break;

            case MAC_FCTRL_DATA_T:
            {
                #if (RW_MESH_EN)
                // Get VIF information
                struct vif_info_tag *p_vif_entry = &vif_info_tab[rx_status->vif_idx];

                if (p_vif_entry->type == VIF_MESH_POINT)
                {
                    if (!mesh_accept_frame(sta_idx))
                    {
                        break;
                    }

                    // Extract the Mesh PS status from the received header
                    mesh_ps_rx_data_handle(rx_status->vif_idx, rx_status->sta_idx, (uint32_t)frame);
                }
                #endif //(RW_MESH_EN)

                // Check if the received frame is a NULL frame
                if (frame_cntl & MAC_NODATA_ST_BIT)
                {
                    break;
                }

                qos = ((frame_cntl & MAC_QOS_ST_BIT) != 0);

                #if (RW_MESH_EN)
                if (p_vif_entry->type == VIF_MESH_POINT)
                {
                    // Check if this frame is for us or if it has to be forwarded to another Mesh STA
                    if (!mesh_hwmp_check_data_dest(p_vif_entry, sta_idx, (uint32_t)frame, rx_status))
                    {
                        break;
                    }

                    rx_status->mesh_ctrl_len = mesh_rx_get_machdr_add_len(p_vif_entry, rx_status->a_qos_ctrl);

                    // If frame has to be forwarded to another Mesh Point, keep Mesh Control as is
                    if (rx_status->dst_idx == INVALID_STA_IDX)
                    {
                        rx_status->machdr_len += rx_status->mesh_ctrl_len;
                    }
                }
                #endif //(RW_MESH_EN)

                #if (NX_REORD)
                // Check if a BA Agreement exists for the STAID
                if (qos && mm_ba_agmt_rx_exists(sta_idx, rx_status->tid))
                {
                    /*
                     * Check if received packet was expected
                     * If yes, the packet can be uploaded in host memory
                     */
                    upload = rxu_cntrl_reord_check(swdesc, sta_idx);
                }
                else
                #endif //(NX_REORD)
                {
                    // Perform the duplicate check
                    if (!rxu_cntrl_duplicate_check(frame_cntl, sta_idx, qos))
                        break;

                    // Perform the PN check if required
                    if ((rx_status->frame_info & RXU_CNTRL_PN_CHECK_NEEDED) &&
                        !rxu_cntrl_check_pn(&rx_status->pn, rx_status->key, rx_status->tid))
                         break;

                    /*
                     * Packet has not been detected as a duplicate one. Check if reassembly procedure
                     * has to be used
                     */
                    upload = rxu_cntrl_defrag_check(swdesc, sta_idx, qos, rhd->frmlen);
                }
            } break;
        }
    } while (0);


    return (0);
}

#if (NX_REORD)
bool rxu_cntrl_reord_create(struct sta_info_tag *sta, uint8_t tid, uint16_t ssn)
{
    struct rxu_cntrl_reord *p_reord;

    // Check if we already have a reordering structure allocated for this STA/TID pair
    if (sta->ba_agmts_rx[tid] != NULL)
        return false;

    // Get a free reordering structure
    p_reord = (struct rxu_cntrl_reord *) co_list_pop_front(&rxu_cntrl_env.rxu_reord_free);
    if (!p_reord)
        return false;

    // Reset the content
    memset(p_reord, 0, sizeof(struct rxu_cntrl_reord));

    // Configure the RX SN Window
    p_reord->tid = tid;
    p_reord->win_start = ssn;
    p_reord->rx_status_pos = ssn % RX_CNTRL_REORD_WIN_SIZE;
    p_reord->sn_rx_time = hal_machw_time();
    p_reord->timer.cb = rxu_cntrl_reord_timeout_cb;
    p_reord->timer.env = p_reord;

    // Save the pointer to the reordering structure
    sta->ba_agmts_rx[tid] = p_reord;

    // Start the reordering timer
    mm_timer_set(&p_reord->timer, p_reord->sn_rx_time + RX_CNTRL_REORD_MAX_WAIT);

    return true;
}

void rxu_cntrl_reord_delete(struct sta_info_tag *sta, uint8_t tid)
{
    struct rxu_cntrl_reord *p_reord = sta->ba_agmts_rx[tid];

    // Sanity check - The element shall exist
    ASSERT_ERR(p_reord != NULL);

    // Stop the reordering timer
    mm_timer_clear(&p_reord->timer);

    // Push back the reordering structure to the free list
    co_list_push_back(&rxu_cntrl_env.rxu_reord_free,
                      &p_reord->list_hdr);

    // Delete the pointer to the reordering structure
    sta->ba_agmts_rx[tid] = NULL;
}
#endif //(NX_REORD)

void rxu_cntrl_monitor_pm(struct mac_addr *addr)
{
    // Only one address can be monitored at a time
    if (rxu_cntrl_env.pm_mon.mon)
        return;

    // Copy the MAC address
    MAC_ADDR_CPY(&rxu_cntrl_env.pm_mon.addr, addr);
    // Set the PM state to active
    rxu_cntrl_env.pm_mon.pm_state = PS_MODE_OFF;
    // Enable the PM monitoring
    rxu_cntrl_env.pm_mon.mon = true;
}

uint8_t rxu_cntrl_get_pm(void)
{
    uint8_t pm_state = rxu_cntrl_env.pm_mon.pm_state;

    // Disable the PM monitoring
    rxu_cntrl_env.pm_mon.mon = false;
    // Set the PM state to active
    rxu_cntrl_env.pm_mon.pm_state = PS_MODE_OFF;

    return (pm_state);
}

void rxu_cntrl_evt(int dummy)
{
    // Clear the kernel event
    ke_evt_clear(KE_EVT_RXUREADY_BIT);

    // Transfer the pending descriptor list
    rxu_cntrl_desc_transfer();
}


/// @}

