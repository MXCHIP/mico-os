/**
 ****************************************************************************************
 * @file rxl_cntrl.c
 *
 * @brief Implementation of Rx path APIs.
 *
 * Copyright (C) RivieraWaves 2011-2016
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_int.h"
#include "co_bool.h"
#include <string.h>
#include "compiler.h"
#include "co_endian.h"
#include "mac_defs.h"
#include "rxl_cntrl.h"
#include "ke_event.h"
#include "mac_frame.h"
#include "ps.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "dma.h"
#include "hal_machw.h"
#include "mm.h"
#include "rxl_hwdesc.h"
#include "rx_swdesc.h"
#include "mac_ie.h"
#include "rwnx.h"
#include "wlan_ui_pub.h"

#if NX_AMPDU_TX
#include "txl_cfm.h"
#endif

#if (NX_P2P)
#include "p2p.h"
#endif //(NX_P2P)

#if (NX_TD)
#include "td.h"
#endif //(NX_TD)

#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#if (NX_REORD)
#include "mac_frame.h"
#endif //(NX_REORD)

#include "rxu_cntrl.h"
#include "tdls.h"

#include "include.h"
#include "arm_arch.h"
#include "uart_pub.h" 
#include "sdio_intf_pub.h" 
#include "intc_pub.h"
#include "mem_pub.h"
#include "lwip/pbuf.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct rxl_cntrl_env_tag rxl_cntrl_env;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Preparation of the DMA MPDU transfer to UMAC
 *
 * @param[in] swdesc   Pointer to the rx SW descriptor for which the MPDU transfer is prepared
 *
 * @return     true if MPDU transfer has been successfully programmed, false if it was not
 *             due to a missing host buffer
 ****************************************************************************************
 */
#if(CFG_USE_LWIP_NETSTACK)
#include "lwip_intf.h"
#endif

static uint8_t rx_header[RXL_HEADER_INFO_LEN + 2];

extern void bmsg_rx_sender(void *arg);

#ifdef CONFIG_AOS_MESH
#include "reg_mac_core.h"

extern int beken_get_sm_connect_flag(void);
static bool filter_frame(struct rx_swdesc *swdesc)
{
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
    struct rx_hd *rhd = &dma_hdrdesc->hd;
    struct rx_payloaddesc *payl_d = HW2CPU(rhd->first_pbd_ptr);
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    uint32_t *frame = payl_d->buffer;
    struct mac_hdr *hdr = (struct mac_hdr *)frame;
    struct scan_chan_tag const *chan = NULL;
    struct mac_addr bssid;
    uint32_t bssid_low;
    struct mac_addr *org_bssid;

    if (beken_get_sm_connect_flag() == 0) {
        return false;
    }

    bssid_low = nxmac_bss_id_low_get();
    bssid.array[0] = bssid_low;
    bssid.array[1] = (bssid_low & 0xffff0000) >> 16;
    bssid.array[2] = nxmac_bss_id_high_getf();

    // check TODS bit
    switch ((hdr->fctl >> 8) & 0x03) {
        case 0:
            org_bssid = (uint8_t *)hdr->addr3.array;
            break;
        case 1:
            org_bssid = (uint8_t *)hdr->addr1.array;
            break;
        case 2:
            org_bssid = (uint8_t *)hdr->addr2.array;
            break;
        default:
            return false;
    }

    if (memcmp(&bssid, org_bssid, sizeof(bssid)) == 0) {
        return false;
    }

    return true;
}
#endif

int rxl_data_monitor(struct rx_swdesc *swdesc, uint8_t *payload,
                              uint16_t length)
{
	monitor_data_cb_t fn;
	int monitor_flag = 0;
        struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
        struct rx_hd *rhd = &dma_hdrdesc->hd;
        hal_wifi_link_info_t link_info;

        link_info.rssi = (rhd->recvec1c >> 24) & 0xff;
	if(bk_wlan_is_monitor_mode())
	{
		monitor_flag = 1;

		fn = bk_wlan_get_monitor_cb();		
		(*fn)((uint8_t *)payload, length, &link_info);
	}
#ifdef CONFIG_AOS_MESH
        else if (wlan_is_mesh_monitor_mode()) {
            fn = wlan_get_mesh_monitor_cb();
            (*fn)((uint8_t *)payload, length, &link_info);
        }
#endif

	return monitor_flag;
}

uint32_t rxu_cntrl_patch_get_compensation_len(
	uint8_t *p_frame)
{
    struct mac_hdr *machdr_ptr = (struct mac_hdr *)p_frame;
    struct mac_hdr_qos *machdr_qos_ptr = (struct mac_hdr_qos *)p_frame;
	uint8_t *data = (uint8_t *)&machdr_ptr[1];
	uint8_t key_id, ver;
	uint8_t qos_flag = 0;
	uint8_t *addr, *ra_addr;
	uint8_t *encrypt_param;
	uint32_t comp_len = 0;
	
	if(!bk_wlan_is_monitor_mode())
	{
		goto comp_exit;
	}

	ra_addr = (uint8_t *)&machdr_ptr->addr1;
	if(!(ra_addr[0] & 0x01))
	{
		/* hardware issue */
		goto comp_exit;
	}
	
	if((0x02 == ((machdr_ptr->fctl >> 2) & 0x03))
		&& (machdr_ptr->fctl & 0x80))
	{
		qos_flag = 1;
	}
	
	if(0 == (machdr_ptr->fctl & 0x4000))
	{
		/*.1.. .... = Protected flag: Data is protected;
		  .0.. .... = open system;*/ 
		return comp_len;
	}
	
	encrypt_param = (uint8_t *)&machdr_ptr[1];
	if(qos_flag)
	{
		encrypt_param = (uint8_t *)&machdr_qos_ptr[1];
		data = (uint8_t *)&machdr_qos_ptr[1];
	}
	key_id = (data[3] >> 6) & 0x03;
	ver = (data[3] >> 5) & 0x01;

	if(0 == ver)
	{
		/*cipherType: CTYPERAM_WEP;*/
		comp_len = 4;
	}
	else
	{		
		if(encrypt_param[1] == ((encrypt_param[0] | 0x20) & 0x7f))
		{
			/*cipherType: CTYPERAM_TKIP;*/
			comp_len = 4;
		}
		if(0 == encrypt_param[2])
		{
			/*cipherType: CTYPERAM_CCMP;*/
			comp_len = 8;
		}

		if((0 == encrypt_param[2])
			&& (encrypt_param[1] == ((encrypt_param[0] | 0x20) & 0x7f)))
		{
		}
	}

comp_exit:	
	return comp_len;
}

void rxl_mpdu_transfer(struct rx_swdesc *swdesc)
{    
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    uint8_t payl_offset = rx_status->payl_offset;    
    uint32_t hostbuf, hostbuf_start;
    uint16_t dma_len;
    uint16_t mpdu_len;
    struct dma_desc *dma_desc;
    struct rx_payloaddesc *pd;
    struct rx_payloaddesc *prev_pd = NULL;
    struct dma_desc *first_dma_desc;
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
    struct pbuf *pbuf = NULL;
    uint32_t du_len;
	uint8_t *du_ptr;
	
    // Cache the pointers for faster access
    pd = (struct rx_payloaddesc *)HW2CPU(dma_hdrdesc->hd.first_pbd_ptr);
	
    // Get the MPDU body length
    mpdu_len = dma_hdrdesc->hd.frmlen;

	if(bk_wlan_is_monitor_mode())
	{
		du_len = mpdu_len;		
    	du_ptr = (uint8_t *)os_malloc(du_len);	
	    hostbuf_start = (uint32_t)du_ptr;	 
	}
#ifdef CONFIG_AOS_MESH
    else if (rxu_mesh_monitor(swdesc)) {
        du_len = mpdu_len;		
        du_ptr = (uint8_t *)os_malloc(du_len);	
        hostbuf_start = (uint32_t)du_ptr;	 
    } else if (filter_frame(swdesc) == false)
#else
	else
#endif
	{		
	    if(g_rwnx_connector.rx_alloc_func)
	    {
	        du_len = mpdu_len;
	        (*g_rwnx_connector.rx_alloc_func)(&pbuf, du_len);
	    }
	    hostbuf_start = (uint32_t)pbuf->payload;
	}
	
    rxu_cntrl_env.rx_ipcdesc_stat.host_id = hostbuf_start;

    // Copy at the beginning of the host buffer + PHY VECT + 2 offset
    hostbuf = hostbuf_start;

    // Sanity check - The frame length cannot be NULL...
    ASSERT_REC(mpdu_len != 0);
    // as well as not greater than the max allowed length
    ASSERT_REC(mpdu_len <= RWNX_MAX_AMSDU_RX);

    // Get the information on the current channel from the PHY driver
    phy_get_channel(&dma_hdrdesc->phy_info, PHY_PRIM);

    // Get the IPC DMA descriptor of the MAC header
    dma_desc = &pd->dma_desc;
    // Save the pointer to the first desc, as it will be passed to the DMA driver later
    first_dma_desc = dma_desc;

    // Loop as long as the MPDU still has data to copy
    while (1)
    {
        // Fill the destination address of the DMA descriptor
        dma_desc->dest = hostbuf;
        dma_desc->src = pd->pbd.datastartptr + payl_offset;

        // Check if we have reached the last payload buffer containing the MPDU
        if ((mpdu_len + payl_offset) <= NX_RX_PAYLOAD_LEN)
        {
            // DMA only the remaining bytes of the payload
            dma_len = mpdu_len;
        }
        else
        {
            // The complete payload buffer has to be DMA'ed
            dma_len = NX_RX_PAYLOAD_LEN - payl_offset;
        }

        // Reset the offset
        payl_offset = 0;

        // Fill the DMA length in the IPC DMA descriptor
        dma_desc->length = dma_len;

        // By default no DMA IRQ on this intermediate transfer
        dma_desc->ctrl = 0;

        // Move the pointer in the host buffer
        hostbuf += dma_len;

        // Compute remaining length to be DMA'ed to host
        mpdu_len -= dma_len;

        // Check if we have finished to program the payload transfer
        if (mpdu_len == 0)
        {
            break;
        }
        else
        {
            // Move to the next RBD
            prev_pd = pd;
            pd = (struct rx_payloaddesc *)HW2CPU(pd->pbd.next);

            // Sanity check - There shall be a payload descriptor available
            ASSERT_REC(pd != NULL);
        }

        // Link the new descriptor with the previous one
        dma_desc->next = CPU2HW(&pd->dma_desc);

        // Retrieve the new DMA descriptor from the payload descriptor
        dma_desc = &pd->dma_desc;
    }

    // Save position of next fragment upload
    rx_status->host_buf_addr = hostbuf;

	if (dma_desc->length <= RXL_HEADER_INFO_LEN+2) 
	{// yhb added, recv header: struct rx_hd->frmlen
	    // Now elaborate the final DMA sending for the PHY Vectors and status
	    // "Stamp" it as the final DMA of this MPDU transfer: insert a pattern for the upper
	    // layer to be able to know that this hostbuf has been filled
	    dma_hdrdesc->pattern = DMA_HD_RXPATTERN;
	    dma_desc->next = CPU2HW(&dma_hdrdesc->dma_desc);
	    dma_desc = &dma_hdrdesc->dma_desc;
	    dma_desc->dest = (uint32_t)rx_header;
	    dma_desc->ctrl = 0;
	}

#ifdef CONFIG_AOS_MESH
	if(bk_wlan_is_monitor_mode()  || rxu_mesh_monitor(swdesc))
#else
	if(bk_wlan_is_monitor_mode())
#endif
	{	 
		if(du_ptr)
		{
			dma_push(first_dma_desc, dma_desc, IPC_DMA_CHANNEL_DATA_RX);
			du_len += rxu_cntrl_patch_get_compensation_len(du_ptr);
#ifdef CONFIG_AOS_MESH
			if(du_len >= 42 || rxu_mesh_monitor(swdesc))
#else
			if(du_len >= 42)
#endif
			{
				rxl_data_monitor(swdesc, (uint8_t *)((uint32_t)du_ptr), du_len);
			}
			
			os_free(du_ptr);
		}
	}
	else if(pbuf)
	{
	    dma_push(first_dma_desc, dma_desc, IPC_DMA_CHANNEL_DATA_RX);
	    
	    if(g_rwnx_connector.data_outbound_func)
	    {
	        (*g_rwnx_connector.data_outbound_func)(pbuf, du_len);
	    }
		else
		{
			pbuf_free(pbuf);
		}
	}	

    // Now go through the additional RBD if any (e.g. for FCS, ICV)
    while (pd)
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
    swdesc->last_pbd = &prev_pd->pbd;
    swdesc->spare_pbd = &pd->pbd;

    // Store the pointer to the next available RBD (for debug and error logs only)
    rx_hwdesc_env.first = HW2CPU(swdesc->spare_pbd->next);
}

/**
 ****************************************************************************************
 * @brief This function processes the received control frames.
 *
 * @param[in] swdesc SW header descriptor of the frame
 *
 * @return true if the packet was handled internally, false otherwise
 ****************************************************************************************
 */
static bool rxl_rxcntrl_frame(struct rx_swdesc* swdesc)
{
    bool handled = true;
    bool release = true;
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;

#if CFG_RX_SENSITIVITY_TEST
    extern UINT32 g_rxsens_start;
    if(g_rxsens_start) 
    {
        rxl_mpdu_free(swdesc);
        return (handled);
    }
#endif

    // Check if we received a NDP frame
    if (dma_hdrdesc->hd.frmlen != 0)
    {
        uint16_t framectrl;
        #if NX_AMPDU_TX
        uint32_t statinfo;
        #endif
        struct rx_pbd *pd = HW2CPU(dma_hdrdesc->hd.first_pbd_ptr);

        // Sanity check: frames with length not NULL have at least 1 buffer descriptor
        ASSERT_REC_VAL(pd != NULL, true);

        // Get the frame control
        framectrl = co_read16(HW2CPU(pd->datastartptr));

        #if NX_AMPDU_TX
        // Get the status information from the RX header descriptor
        statinfo = dma_hdrdesc->hd.statinfo;
        #endif

        // Decode the frame control to know if we have to handle the frame internally
        switch(framectrl & MAC_FCTRL_TYPESUBTYPE_MASK)
        {
            #if NX_AMPDU_TX
            case MAC_FCTRL_BA :
                // Check if this BA is a response to a sent A-MPDU
                if ((statinfo & (RX_HD_RSP_FRM | RX_HD_ADDRMIS | RX_HD_SUCCESS)) ==
                                                       (RX_HD_RSP_FRM | RX_HD_SUCCESS))
                {
                    //keep BA rxdesc in list to analyse in tx confirm event handler
                    txl_ba_push(swdesc);
                    release = false;
                }
                break;
            #endif
            default:
                // LMAC does not handle this control frame, so forward it to upper MAC
                handled = false;
                break;
        }
    }
    else
    {
        // Sanity check: NDP frames have no buffer descriptor attached
        ASSERT_REC_VAL(dma_hdrdesc->hd.first_pbd_ptr == 0, true);
    }

    // Check if the frame was handled
    if (handled)
    {
        // Control and NDP frames have only one or less RBD
        if (swdesc->dma_hdrdesc->hd.first_pbd_ptr != 0)
        {
            swdesc->spare_pbd = HW2CPU(swdesc->dma_hdrdesc->hd.first_pbd_ptr);

            // Store the pointer to the next available RBD (for debug and error logs only)
            rx_hwdesc_env.first = HW2CPU(swdesc->spare_pbd->next);
        }
        else
        {
            swdesc->spare_pbd = NULL;
        }
        swdesc->last_pbd = NULL;

        // Check if the frame has to be released immediately
        if (release)
        {
            if (swdesc->spare_pbd != NULL)
            {
                // release the payload descriptors associated with this SW descriptor
                rxl_pd_append(NULL, NULL, swdesc->spare_pbd);
            }

            // release the HW DMA descriptors
            rxl_hd_append(swdesc->dma_hdrdesc);
        }
    }
    return (handled);
}

#if (NX_RX_FRAME_HANDLING)
/**
 ****************************************************************************************
 * @brief This function processes the received frames that could carry useful information
 * for some LMAC features (connection monitoring, power-save mode, etc.)
 *
 * @param[in] swdesc SW header descriptor of the frame
 ****************************************************************************************
 */
static void rxl_pm_check(uint8_t *frame, uint8_t sta_idx, uint8_t vif_idx)
{
    struct vif_info_tag *vif = &vif_info_tab[vif_idx];
    struct sta_info_tag *sta = &sta_info_tab[sta_idx];
    uint16_t frame_ctrl;

    do
    {
        // Check the PM state only in AP mode
        if (vif->type != VIF_AP)
            break;

        // Get the frame control field from the packet
        frame_ctrl = co_read16(frame);

        // Check if the station is in PS or not
        if (sta->ps_state == PS_MODE_ON)
        {
            // Check if the peer device is waking up (PWRMGMT bit and MOREFRAG bit have to be set to 0)
            if ((frame_ctrl & (MAC_FCTRL_TYPE_MASK | MAC_FCTRL_PWRMGT |
                                            MAC_FCTRL_MOREFRAG)) == MAC_FCTRL_DATA_T)
            {
                // Indicate the PS mode change to the upper layers
                mm_ps_change_ind(sta_idx, PS_MODE_OFF);

                // Update the number of PS stations
                vif->u.ap.ps_sta_cnt--;

                if (!vif->u.ap.ps_sta_cnt)
                {
                    mm_ps_change_ind(VIF_TO_BCMC_IDX(vif_idx), PS_MODE_OFF);
                }

                #if (NX_P2P_GO)
                p2p_go_ps_state_update(vif);
                #endif //(NX_P2P_GO)
                break;
            }

            // Check if the received frame shall trigger some transmissions from us
            if ((frame_ctrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_PSPOLL)
            {
                mm_traffic_req_ind(sta_idx, 1, false);
            }

            // Check if the received frame shall trigger some transmissions from us
            if ((frame_ctrl & (MAC_FCTRL_TYPE_MASK | MAC_QOS_ST_BIT)) ==
                (MAC_FCTRL_DATA_T | MAC_QOS_ST_BIT))
            {
                uint8_t tid;

                // Get the TID
                if ((frame_ctrl & (MAC_FCTRL_FROMDS | MAC_FCTRL_FROMDS)) ==
                    (MAC_FCTRL_FROMDS | MAC_FCTRL_FROMDS))
                {
                    struct mac_hdr_long_qos *hdr = (struct mac_hdr_long_qos *)frame;
                    // Get the QoS control field in the frame and the TID
                    tid = (hdr->qos & MAC_QOSCTRL_UP_MSK) >> MAC_QOSCTRL_UP_OFT;
                }
                else
                {
                    struct mac_hdr_qos *hdr = (struct mac_hdr_qos *)frame;
                    // Get the QoS control field in the frame and the TID
                    tid = (hdr->qos & MAC_QOSCTRL_UP_MSK) >> MAC_QOSCTRL_UP_OFT;
                }

                // Check if the received frame is a UAPSD trigger frame
                if (mac_ac2uapsd[mac_tid2ac[tid]] & sta->info.uapsd_queues)
                {
                    // Check if UAPSD traffic is available on host
                    if (sta->uapsd_traffic_avail)
                    {
                        // Traffic is available on host. Check if a SP is already ongoing
                        if (!sta->uapsd_sp_ongoing)
                        {
                            // No SP ongoing. Ask for packets and open
                            // the service period
                            sta->uapsd_sp_ongoing = true;
                            mm_traffic_req_ind(sta_idx, 0, true);
                        }
                    }
                    else
                    {
                        // No traffic available on host. Reply with a QoS NULL frame
                        // indicating the end of service period
                        uint16_t qos = MAC_QOSCTRL_EOSP | (tid << MAC_QOSCTRL_UP_OFT);
                        txl_frame_send_qosnull_frame(sta->staid, qos, NULL, NULL);
                    }
                }
            }
        }
        else
        {
            // Check if the peer device is going to sleep
            if ((frame_ctrl & (MAC_FCTRL_PWRMGT | MAC_FCTRL_MOREFRAG))
                                                               == MAC_FCTRL_PWRMGT)
            {
                // Indicate the PS mode change to the upper layers
                mm_ps_change_ind(sta_idx, PS_MODE_ON);

                // Update the number of PS stations
                if (!vif->u.ap.ps_sta_cnt)
                {
                    mm_ps_change_ind(VIF_TO_BCMC_IDX(vif_idx), PS_MODE_ON);
                }

                // Update the number of PS stations
                vif->u.ap.ps_sta_cnt++;

                #if (NX_P2P_GO)
                p2p_go_ps_state_update(vif);
                #endif //(NX_P2P_GO)
                break;
            }
        }
    } while (0);
}

/**
 ****************************************************************************************
 * @brief This function processes the received frames that could carry useful information
 * for some LMAC features (connection monitoring, power-save mode, etc.)
 *
 * @param[in] swdesc SW header descriptor of the frame
 *
 * @return true if the frame shall be uploaded to UMAC, false otherwise
 ****************************************************************************************
 */
static uint8_t rxl_frame_handle(struct rx_swdesc* swdesc, bool *dont_free)
{
    struct sta_info_tag *p_sta_entry;
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
    struct rx_hd *rhd = &dma_hdrdesc->hd;
    struct rx_pbd *pd;
    uint32_t statinfo;
    uint16_t key_idx_hw;
    uint8_t sta_idx;
    struct vif_info_tag *vif_entry;
    uint8_t *frame;
    uint16_t framectrl;
    bool upload = true;

    do
    {
        // Sanity check: frames with length not NULL have at least 1 buffer descriptor
        ASSERT_REC_VAL(rhd->first_pbd_ptr != 0, false);

        // Get value of the buffer descriptor pointer
        pd = HW2CPU(rhd->first_pbd_ptr);

        // Get the status information from the RX header descriptor
        statinfo = rhd->statinfo;
        // Check if the frame comes from a known device and is valid
        if ((statinfo & (RX_HD_KEYIDV | RX_HD_SUCCESS)) != (RX_HD_KEYIDV | RX_HD_SUCCESS))
            break;

        // Get the HW key index
        key_idx_hw = (uint16_t)RX_HD_KEYID_GET(statinfo);

        // Sanity check
        ASSERT_REC_VAL(key_idx_hw >= MM_SEC_DEFAULT_KEY_COUNT, false);

        // Retrieve the station index and instance number
        sta_idx = (uint8_t)(key_idx_hw - MM_SEC_DEFAULT_KEY_COUNT);

        // Check if the STA is registered
        if (!sta_mgmt_is_valid(sta_idx))
        {
            rhd->statinfo &= ~RX_HD_KEYIDV;
            break;
        }

        // Retrieve the associated VIF and STA entry
        p_sta_entry = &sta_info_tab[sta_idx];
        vif_entry = &vif_info_tab[p_sta_entry->inst_nbr];

        // Get the pointer to the frame data
        frame = HW2CPU(pd->datastartptr);

        // Get the frame control
        framectrl = co_read16(frame);

        rxl_pm_check(frame, sta_idx, vif_entry->index);

        #if (RW_BFMER_EN)
        {
            uint8_t bfr_status = (bfr_is_enabled()) ? bfr_rx_frame_ind(sta_idx, swdesc, frame)
                                                    : BFR_RX_STATUS_NOT_VALID;

            if (bfr_status != BFR_RX_STATUS_NOT_VALID)
            {
                // Frame is well a beamforming report, not needed to upload the whole frame.
                upload = false;

                if (bfr_status == BFR_RX_STATUS_VALID)
                {
                    *dont_free = true;
                }

                break;
            }
        }
        #endif //(RW_BFMER_EN)

        // Check if this STA interface is associated with an AP
        if (!vif_entry->active)
            break;

        #if (NX_TD)
        if (((framectrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_DATA_T) ||
            ((framectrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_MGT_T))
        {
            td_pck_ind(vif_entry->index, sta_idx, true);
        }
        #endif //(NX_TD)

        // Check if this interface is a STA or a MP interface
        if (vif_entry->type == VIF_STA)
        {
            // Decode the frame control to know if we have to handle the frame internally
            if ((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_BEACON)
            {
                #if NX_POWERSAVE || NX_CONNECTION_MONITOR || NX_MULTI_ROLE
                uint32_t tim;
                #endif

                #if NX_CONNECTION_MONITOR || NX_MULTI_ROLE
                // Let the MM handle the connection monitoring procedures
                upload = mm_check_beacon(rhd, vif_entry, p_sta_entry, &tim);
                #elif NX_POWERSAVE
                tim = mac_ie_find(pd->datastartptr + MAC_BEACON_VARIABLE_PART_OFT,
                                  dma_hdrdesc->hd.frmlen - MAC_BEACON_VARIABLE_PART_OFT,
                                  MAC_ELTID_TIM);
                #endif

                #if NX_POWERSAVE
                // Let the PS module check if it has something to do with this beacon
                ps_check_beacon(tim, dma_hdrdesc->hd.frmlen, vif_entry);
                #endif

                #if (NX_P2P || NX_CHNL_CTXT)
                vif_mgmt_bcn_recv(vif_entry);
                #endif //(NX_P2P || NX_CHNL_CTXT)

                #if (NX_CHNL_CTXT)
                if (vif_entry->chan_ctxt)
                {
                    chan_tbtt_switch_update(vif_entry, vif_entry->tbtt_timer.time);
                }
                #endif //(NX_CHNL_CTXT)

                #if (NX_P2P)
                if (upload)
                {
                    // Look for NOA Attribute only if beacon has been modified
                    p2p_cli_bcn_check_noa(vif_entry, pd, dma_hdrdesc);
                }
                #endif //(NX_P2P)
            }
            else if (((framectrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_DATA_T) ||
                     ((framectrl & MAC_FCTRL_TYPE_MASK) == MAC_FCTRL_MGT_T))
            {
                #if NX_POWERSAVE
                // Let the PS module check if it has something to do with this frame
                ps_check_frame(frame, statinfo, vif_entry);
                #endif

                #if (NX_P2P)
                if (vif_entry->p2p)
                {
                    if ((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_ACTION)
                    {
                        p2p_cli_handle_action(vif_entry, pd->datastartptr, dma_hdrdesc->hd.frmlen, rhd->tsflo);
                    }
                }
                #endif //(NX_P2P)
                
                #if (TDLS_ENABLE)
                // Check if the frame comes from the TDLS STA and if TDLS STA exists
                if ((vif_entry->u.sta.sta_tdls) &&
                    (sta_idx == vif_entry->u.sta.sta_tdls->sta_idx))
                {
                    upload = tdls_check_frame(frame, vif_entry);
                }
                #endif
            }
        }
        #if (RW_UMESH_EN)
        else if (vif_entry->type == VIF_MESH_POINT)
        {
            // Decode the frame control to know if we have to handle the frame internally
            if ((framectrl & MAC_FCTRL_TYPESUBTYPE_MASK) == MAC_FCTRL_BEACON)
            {
                uint32_t tim;

                // Extract the TBTT
                mm_check_beacon(rhd, vif_entry, p_sta_entry, &tim);

                // Provide the beacon to the Mesh PS Module
                mesh_ps_rx_beacon_handle(vif_entry->index, sta_idx,
                                         pd->datastartptr, dma_hdrdesc->hd.frmlen);

                // Do not upload the beacon
                upload = false;
            }
        }
        #endif //(RW_UMESH_EN)
    } while (0);

    return (upload);
}
#endif //(NX_RX_FRAME_HANDLING)

/**
 ****************************************************************************************
 * @brief   This function initializes the Rx Context Data.
 ****************************************************************************************
 */
static void rxl_cntrl_init(void)
{    
    co_list_init(&rxl_cntrl_env.ready);

    // Initialize the bridge DMA counts
    rxl_cntrl_env.bridgedmacnt = 0;
    rxl_cntrl_env.packet_cnt = RX_FRAME_THD - 1;
}

void rxl_init(void)
{
    intc_service_register(FIQ_MAC_RX_TRIGGER, PRI_FIQ_MAC_RX_TRIGGER, rxl_timer_int_handler); 
    intc_service_register(FIQ_MAC_TX_RX_TIMER, PRI_FIQ_MAC_TX_RX_TIMER, rxl_timer_int_handler); 
    intc_service_register(FIQ_MAC_TX_RX_MISC, PRI_FIQ_MAC_TX_RX_MISC, intc_spurious); 

    rxl_hwdesc_init();
    rx_swdesc_init();
    rxl_cntrl_init();
    rxu_cntrl_init();
}

void rxl_cntrl_evt(int dummy)
{
    int prep_cnt = 0;
    struct rx_swdesc *swdesc;
    
    #if NX_RX_FRAME_HANDLING
    uint8_t upload;
    #endif
    
    GLOBAL_INT_DECLARATION();

    while (1)
    {
        // Check if we reached the threshold for DMA interrupt handling
        if (prep_cnt >= RX_FRAME_PREP_THD)
        {
            // Set the kernel event to handle next frames immediately after the DMA
            bmsg_rx_sender(0);
            break;
        }		

        GLOBAL_INT_DISABLE();	
        swdesc = (struct rx_swdesc *)co_list_pick(&rxl_cntrl_env.ready);
        if (NULL == swdesc)
        {            
			GLOBAL_INT_RESTORE();
            break;
        }
		co_list_pop_front(&rxl_cntrl_env.ready);
        GLOBAL_INT_RESTORE();	

        do
        {
            #if (NX_RX_FRAME_HANDLING)
            bool dont_free = false;

            // Check if the packet is of interest for the LMAC
            upload = rxl_frame_handle(swdesc, &dont_free);
            if (!upload)
            {
                if (!dont_free)
                {
                    rxl_mpdu_free(swdesc);
                }

                break;
            }
            #endif //(NX_RX_FRAME_HANDLING)

            // Perform the UMAC related handling
            upload = rxu_cntrl_frame_handle(swdesc);
            if ((!upload) || (bk_wlan_is_monitor_mode()))
            {
                rxl_mpdu_free(swdesc);
                break;
            }
        } while (0);
	
        prep_cnt++;
    }

    return;
}

void rxl_timer_int_handler(void)
{
    struct rx_dmadesc *dma_hdrdesc;
    struct rx_swdesc *swdesc;
    
    nxmac_tx_rx_int_ack_clear(NXMAC_TIMER_RX_TRIGGER_BIT | NXMAC_COUNTER_RX_TRIGGER_BIT);

    while (1)
    {
        // check if there are more elements to handle
        if ((rxl_cntrl_env.first == NULL) ||
            (!RX_HD_DONE_GET(rxl_cntrl_env.first->hd.statinfo)))
        {
            break;
        }

        // retrieve the pointer to the first element of the list
        dma_hdrdesc = rxl_cntrl_env.first;

        // prepare the SW descriptor for this frame
        swdesc = dma_hdrdesc->hd.swdesc;

        // get the next Header descriptor pointer now as it may be modified by
        // rxl_rxcntrl_frame() if the frame is released immediately
        rxl_cntrl_env.first = (struct rx_dmadesc*)HW2CPU(dma_hdrdesc->hd.next);

        // check if it is a control frame and handle it if necessary
        if (!rxl_rxcntrl_frame(swdesc))
        {
            co_list_push_back(&rxl_cntrl_env.ready, &swdesc->list_hdr);
        }
    }

    // Check if frames are ready
    if (!co_list_is_empty(&rxl_cntrl_env.ready))
    {		
        bmsg_rx_sender(0);
    }
}

void rxl_timeout_int_handler(void)
{
    // Current A-MPDU reception is considered as finished
    // Check if we have some packets to indicate to the host
    if (rxl_cntrl_env.packet_cnt > 0)
    {
        // Some packets are available, so indicate them
    }

    // Reset the packet counter in order to force the host to be warned upon the next
    // A-MPDU reception start
    rxl_cntrl_env.packet_cnt = RX_FRAME_THD - 1;

    // Disable the RX timeout interrupt
    nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() & ~HAL_RX_TIMER_BIT);
}

void rxl_dma_int_handler(void)
{
    struct hal_host_rxdesc *p_rx_desc;
    struct rx_swdesc *swdesc;    
    uint32_t curr_time;
    
    p_rx_desc = (struct hal_host_rxdesc *)co_list_pick(&rxu_cntrl_env.rxdesc_pending);
    while (p_rx_desc)
    {
        swdesc = p_rx_desc->p_rx_swdesc;
        if (swdesc)
        {
            // Release the payload descriptors associated with this SW descriptor
            rxl_pd_append(HW2CPU(swdesc->dma_hdrdesc->hd.first_pbd_ptr),
                                 swdesc->last_pbd, swdesc->spare_pbd);

            // release the HW DMA descriptors
            rxl_hd_append(swdesc->dma_hdrdesc);
        }

        // Pop the descriptor
        co_list_pop_front(&rxu_cntrl_env.rxdesc_pending);

        // call the LMAC-UMAC interface function to handle the frame
        rxl_cntrl_env.packet_cnt++;

        // Check if we need to trigger an interrupt to the host
        if (rxl_cntrl_env.packet_cnt >= RX_FRAME_THD)
        {
            rxl_cntrl_env.packet_cnt = 0;
        }

        // increment the handled LLI counter
        rxl_cntrl_env.bridgedmacnt = 0;

        // Handle next descriptor
        // Reset descriptor value
        p_rx_desc->rxdesc_val.status = 0;

        // Get next descriptor
        p_rx_desc = (struct hal_host_rxdesc *)co_list_pick(&rxu_cntrl_env.rxdesc_pending);
    }

    // Restart the RX timeout
    curr_time = hal_machw_time();
    nxmac_abs_timer_set(HAL_RX_TIMER, curr_time + RX_TIMEOUT);
    nxmac_timers_int_event_clear(HAL_RX_TIMER_BIT);
    nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() | HAL_RX_TIMER_BIT);    
}

void rxl_frame_release(struct rx_swdesc* swdesc)
{
    // release the payload descriptors associated with this SW descriptor
    rxl_pd_append(HW2CPU(swdesc->dma_hdrdesc->hd.first_pbd_ptr), swdesc->last_pbd, swdesc->spare_pbd);

    // release the HW DMA descriptors
    rxl_hd_append(swdesc->dma_hdrdesc);
}

void rxl_mpdu_free(struct rx_swdesc *swdesc)
{
    struct rx_payloaddesc *pd;
    struct rx_payloaddesc *prev_pd = NULL;
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;

    // Cache the pointers for faster access
    pd = (struct rx_payloaddesc *)HW2CPU(dma_hdrdesc->hd.first_pbd_ptr);
	if(0 == pd)
	{
		os_printf("rxl_mpdu_free_strange\r\n");
		return;
	}
	
    while(1)
    {
        uint32_t statinfo = pd->pbd.bufstatinfo;

        // If this is the last RBD, exit the loop
        if (statinfo & RX_PD_LASTBUF)
            break;

        // Otherwise move to the next one
        prev_pd = pd;
        pd = (struct rx_payloaddesc *)HW2CPU(pd->pbd.next);

        // Sanity check - There shall be a payload descriptor available
        ASSERT(pd != NULL);
    };

    // Store the pointer to the last RBD consumed by the HW. It will be used when freeing the frame
    swdesc->last_pbd = &prev_pd->pbd;
    swdesc->spare_pbd = &pd->pbd;

    // Store the pointer to the next available RBD (for debug and error logs only)
    rx_hwdesc_env.first = HW2CPU(swdesc->spare_pbd->next);

    // Release the frame
    rxl_frame_release(swdesc);
}

void rxl_current_desc_get(struct rx_hd **rhd, struct rx_pbd **rbd)
{
    // First RX Header Descriptor chained to the HW
    *rhd = &(rxl_cntrl_env.free->hd);

    // First RX Buffer Descriptor chained to the HW
    *rbd = rx_hwdesc_env.free;
}

void rxl_reset(void)
{
    struct hal_host_rxdesc *p_rx_desc = (struct hal_host_rxdesc *)co_list_pick(&rxu_cntrl_env.rxdesc_pending);

    while (p_rx_desc)
    {
        // Compute the target LLI count of the current descriptor
        uint16_t next_lli_cnt = rxl_cntrl_env.bridgedmacnt + 1;

        co_list_pop_front(&rxu_cntrl_env.rxdesc_pending);

        // call the LMAC-UMAC interface function to handle the frame
        // increment the handled LLI counter
        rxl_cntrl_env.bridgedmacnt = next_lli_cnt;

        p_rx_desc->rxdesc_val.status = 0;
        p_rx_desc = (struct hal_host_rxdesc *)co_list_pick(&rxu_cntrl_env.rxdesc_pending);
    }

    // Re-initialize the HW descriptor lists
    rxl_hwdesc_init();

    // Re-initialize the list of ready frames
    co_list_init(&rxl_cntrl_env.ready);
}

/// @}
