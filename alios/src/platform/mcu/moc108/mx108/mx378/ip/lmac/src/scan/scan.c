/**
 ****************************************************************************************
 *
 * @file scan.c
 *
 * @brief Scanning module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup SCAN
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// for mode
#include "scan.h"
#include "scan_task.h"
#include "mac_frame.h"
#include "hal_dma.h"
#include "mac_ie.h"
#include "vif_mgmt.h"
#include "txl_frame.h"
#include "chan.h"
#include "tpc.h"

#include "include.h"
#include "arm_arch.h"
#include "uart_pub.h"

#if NX_HW_SCAN
/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
///  Global data for maintaining BSS and STA information

/**  LMAC MM Context variable, used to store MM Context data
 */
struct scan_env_tag scan_env;

void scan_search_ds(void)
{
    struct scan_start_req const *param = scan_env.param;
    struct scan_probe_req_ie_tag *ie_desc = &scan_probe_req_ie;

    // Search for the DS IE
    scan_env.ds_ie = mac_ie_find(CPU2HW(ie_desc->buf), param->add_ie_len,
                                 MAC_ELTID_DS);
}

static void dma_cb(void *env, int dma_type)
{
    // Search the channel information element in the 2.4GHz IE buffer
    scan_search_ds();

    // Send the set channel request
    scan_set_channel_request();
}


void scan_init(void)
{
    struct scan_probe_req_ie_tag *ie_desc = &scan_probe_req_ie;
    struct tx_pbd *pbd = &ie_desc->pbd;
    struct hal_dma_desc_tag *gp_dma = &scan_env.dma_desc;

    // Reset the SCAN environment
    memset(&scan_env, 0, sizeof(scan_env));

    // Set the state of the SCAN task
    ke_state_set(TASK_SCAN, SCAN_IDLE);

    // Initialize the DMA descriptors

    // Initialization of the GP DMA descriptor
    gp_dma->cb = dma_cb;
    gp_dma->env = NULL;
    gp_dma->dma_desc = &ie_desc->dma_desc;

    // Initialization of the HW DMA descriptor
    ie_desc->dma_desc.dest = CPU2HW(ie_desc->buf);

    // Initialization of the PBD descriptor
    pbd->bufctrlinfo = 0;
    pbd->datastartptr = CPU2HW(ie_desc->buf);
    pbd->upatterntx = TX_PAYLOAD_DESC_PATTERN;
    pbd->next = 0;
}

void scan_ie_download(struct scan_start_req const *param)
{
    struct scan_probe_req_ie_tag *ie_desc = &scan_probe_req_ie;
    struct tx_pbd *pbd = &ie_desc->pbd;

    do
    {
        // Search for the channel IE in the buffer
        scan_search_ds();

        // Launch the scan request
        scan_set_channel_request();

        // Initialization of the PBD descriptor
        pbd->bufctrlinfo = 0;
        pbd->dataendptr = pbd->datastartptr + param->add_ie_len - 1;
    } while(0);
}

void scan_set_channel_request(void)
{
    struct scan_chan_tag const *chan = &scan_env.param->chan[scan_env.chan_idx];

    // Request the scan channel to the MM channel manager
    chan_scan_req(chan->band, chan->freq,  chan->tx_power,
                  (chan->flags & SCAN_PASSIVE_BIT)?SCAN_PASSIVE_DURATION:SCAN_ACTIVE_DURATION,
                  scan_env.param->vif_idx);

    // Fill-in the DS information element field
    if (scan_env.ds_ie != 0)
    {
        co_write8p(scan_env.ds_ie + MAC_DS_CHANNEL_OFT, phy_freq_to_channel(chan->band, chan->freq));
    }

    // Change the task state
    ke_state_set(TASK_SCAN, SCAN_WAIT_CHANNEL);
}

void scan_probe_req_tx(void)
{
    struct scan_start_req const *param = scan_env.param;
    struct scan_chan_tag const *chan = &param->chan[scan_env.chan_idx];
    struct scan_probe_req_ie_tag *ie_desc = &scan_probe_req_ie;
    struct vif_info_tag *vif_entry = &vif_info_tab[param->vif_idx];
    struct txl_frame_desc_tag *frame;
    struct preq_frame *buf;
    uint32_t ssid_addr;
    int txtype;
    int i;
	
    for (i = 0; i < param->ssid_cnt; i++)
    {
        int length;
        struct tx_hd *thd;
        struct mac_ssid const *ssid = &param->ssid[i];

        #if (NX_P2P)
        if (vif_entry->p2p)
        {
            txtype = TX_DEFAULT_5G;
        }
        else
        #endif //(NX_P2P)
        {
            // Chose the right rate according to the band
            txtype = ((chan->band == PHY_BAND_2G4) && (!param->no_cck))?TX_DEFAULT_24G:TX_DEFAULT_5G;
        }

        // Compute the ProbeReq length
        length = MAC_SHORT_MAC_HDR_LEN + MAC_SSID_SSID_OFT 
                                        + ssid->length 
                                        + param->add_ie_len;

        // Allocate a frame descriptor from the TX path
        frame = txl_frame_get(txtype, length);
        if (frame == NULL)
            break;

        // Get the buffer pointer
        #if NX_AMSDU_TX
        buf = (struct preq_frame *)frame->txdesc.lmac.buffer[0]->payload;
        #else
        buf = (struct preq_frame *)frame->txdesc.lmac.buffer->payload;
        #endif
        thd = &frame->txdesc.lmac.hw_desc->thd;

        // Prepare the MAC Header
        buf->h.fctl = MAC_FCTRL_PROBEREQ;
        buf->h.durid = 0;

		if(MAC_ADDR_IS_BSCT(&param->bssid))
		{
	        buf->h.addr1 = mac_addr_bcst;
		}
		else
		{
	        buf->h.addr1 = param->bssid;
		}
		
        buf->h.addr2 = vif_entry->mac_addr;
        buf->h.addr3 = param->bssid;
        buf->h.seq = txl_get_seq_ctrl();

        // Copy the SSID in the packet
        ssid_addr = CPU2HW(&buf->payload);
        co_write8p(ssid_addr++, MAC_ELTID_SSID);
        co_write8p(ssid_addr++, ssid->length);
        co_pack8p(ssid_addr, ssid->array, ssid->length);

        // Attach the IE buffer to the ProbeReq THD
        thd->first_pbd_ptr = CPU2HW(&ie_desc->pbd);
        thd->dataendptr -= param->add_ie_len;

        // Fill-in the confirmation structure
        frame->cfm.cfm_func = NULL;
        frame->cfm.env = NULL;

        #if (NX_CHNL_CTXT || NX_P2P)
        // Set VIF and STA indexes
        frame->txdesc.host.vif_idx = param->vif_idx;
        frame->txdesc.host.staid   = 0xFF;
        #endif //(NX_CHNL_CTXT || NX_P2P)

        txl_frame_push(frame, AC_VO);
    }
}

void scan_send_cancel_cfm(uint8_t status, ke_task_id_t dest_id)
{
    struct scan_cancel_cfm *cfm = KE_MSG_ALLOC(SCAN_CANCEL_CFM,
                                               dest_id, TASK_SCAN,
                                               scan_cancel_cfm);

    cfm->status = status;

    // Send the response
    ke_msg_send(cfm);
}
struct scan_chan_tag const *scan_get_chan(void)
{
    return (&scan_env.param->chan[scan_env.chan_idx]);
}
#endif

#if CFG_TX_EVM_TEST
#include "include.h"
#include "arm_arch.h"
#include "mem_pub.h"

uint32_t evm_req_tx(struct mac_addr const *mac_addr)
{
    struct txl_frame_desc_tag *frame;
    struct preq_frame *buf;
    uint32_t ssid_addr;
    int txtype;

    int length;
    struct tx_hd *thd;
    struct mac_ssid const ssid = {
			4,
			{'e', 'v', 'm' , ' '}
		};
	
	os_printf("[EVM]evm_req_tx\r\n");

    // Chose the right rate according to the band
    txtype = TX_DEFAULT_24G;

    // Compute the ProbeReq length
    length = MAC_SHORT_MAC_HDR_LEN + MAC_SSID_SSID_OFT + ssid.length;

    // Allocate a frame descriptor from the TX path
    frame = txl_frame_get(txtype, length);
    if (frame == NULL)
        goto tx_exit;

    // Get the buffer pointer
    buf = (struct preq_frame *)frame->txdesc.lmac.buffer->payload;
    thd = &frame->txdesc.lmac.hw_desc->thd;

    // Prepare the MAC Header
    buf->h.fctl = MAC_FCTRL_PROBEREQ;
    buf->h.durid = 0;
    buf->h.addr1 = mac_addr_bcst;
	os_memcpy(&buf->h.addr2, mac_addr, sizeof(struct mac_addr));
	os_memset(&buf->h.addr3, 0xff, sizeof(struct mac_addr));
    buf->h.seq = txl_get_seq_ctrl();

    // Copy the SSID in the packet
    ssid_addr = CPU2HW(&buf->payload);
    co_write8p(ssid_addr++, MAC_ELTID_SSID);
    co_write8p(ssid_addr++, ssid.length);
    co_pack8p(ssid_addr, ssid.array, ssid.length);

    // Attach the IE buffer to the ProbeReq THD
    thd->first_pbd_ptr = 0;
    thd->dataendptr = 0;

    // Fill-in the confirmation structure
    frame->cfm.cfm_func = NULL;
    frame->cfm.env = NULL;

    #if (NX_CHNL_CTXT || NX_P2P)
    // Set VIF and STA indexes
    frame->txdesc.host.vif_idx = 0;
    frame->txdesc.host.staid   = 0xFF;
    #endif //(NX_CHNL_CTXT || NX_P2P)

    // Push the frame for TX
    txl_frame_push(frame, AC_VO);

	return 0;
	
tx_exit:
	return (uint32_t)-1;
}

#endif // CFG_TX_EVM_TEST

/// @} end of group
