/**
 ****************************************************************************************
 *
 * @file scanu.c
 *
 * @brief Definition of the Template module environment.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 * @addtogroup SCANU
 * @{
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_endian.h"
#include "co_utils.h"
#include "scanu.h"
#include "scanu_task.h"
#include "co_status.h"
#include "mac_frame.h"
#include "mac_defs.h"
#include "me_mgmtframe.h"
#include "me_utils.h"

#include "scan_task.h"
#include "rxu_task.h"
#include "me.h"
#include "mac_ie.h"

#include "vif_mgmt.h"

#if (NX_P2P)
#include "p2p.h"
#endif //(NX_P2P)

#include "include.h"
#include "uart_pub.h"
#include "mem_pub.h"

/**
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// SCAN module environment definition.
struct scanu_env_tag scanu_env;

/**
 * PRIVATE FUNCTIONS DEFINITION
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Build the probe request IE.
 *
 * The IE buffer will then be used by the LMAC to build the ProbeReq sent.
 *
 ****************************************************************************************
 */
static uint32_t scanu_build_ie(void)
{
    struct scanu_start_req const *param = scanu_env.param;
    struct scanu_add_ie_tag *add_ie = &scanu_add_ie;
    struct scan_probe_req_ie_tag *ie = &scan_probe_req_ie;
    uint32_t add_ie_buf = CPU2HW(add_ie->buf);
    uint32_t ie_buf = CPU2HW(ie->buf);
    uint8_t sup_rate_len = MAC_RATESET_LEN;
    uint8_t sup_rate_oft = 0;
    uint8_t ie_len;
    uint16_t add_ie_len = param->add_ie_len;

    // Check if the additional IE buffer is valid
    if (add_ie_len > SCANU_MAX_IE_LEN)
        add_ie_len = 0;

    // Compute the number of legacy rates depending on the band we scan
    if ((scanu_env.band == PHY_BAND_5G) || param->no_cck)
    {
        sup_rate_len = MAC_SUPP_RATES_IE_LEN;
        sup_rate_oft = 4;
    }

    // The supported rates is the first element to put
    co_write8p(ie_buf++, MAC_ELTID_RATES);
    co_write8p(ie_buf++, MAC_SUPP_RATES_IE_LEN);
    co_pack8p(ie_buf, &mac_id2rate[sup_rate_oft], MAC_SUPP_RATES_IE_LEN);
    ie_buf += MAC_SUPP_RATES_IE_LEN;

    // Check if the first element of the IE buffer is the request information
    if (add_ie_len && (co_read8p(add_ie_buf) == MAC_ELTID_REQUEST))
    {
        // Copy the IE
        ie_len = co_read8p(add_ie_buf + 1) + 2;
        co_copy8p(ie_buf, add_ie_buf, ie_len);
        ie_buf += ie_len;
        add_ie_buf += ie_len;
        add_ie_len -= ie_len;
    }

    // Extended supported rates
    if (sup_rate_len > MAC_SUPP_RATES_IE_LEN)
    {
        ie_len = sup_rate_len - MAC_SUPP_RATES_IE_LEN;
        co_write8p(ie_buf++, MAC_ELTID_EXT_RATES);
        co_write8p(ie_buf++, ie_len);
        co_pack8p(ie_buf, &mac_id2rate[MAC_SUPP_RATES_IE_LEN], ie_len);
        ie_buf += ie_len;
    }

    // Then comes the DS parameter
    if (scanu_env.band == PHY_BAND_2G4)
    {
        co_write8p(ie_buf++, MAC_ELTID_DS);
        co_write8p(ie_buf++, 1);
        ie_buf++; // Skip the channel number, as it will be filled by the LMAC
    }

    // Now we may have the Supported Operating Classes in the additional IE
    if (add_ie_len && (co_read8p(add_ie_buf) == MAC_ELTID_SUPP_OPER_CLASS))
    {
        // Copy the IE
        ie_len = co_read8p(add_ie_buf + 1) + 2;
        co_copy8p(ie_buf, add_ie_buf, ie_len);
        ie_buf += ie_len;
        add_ie_buf += ie_len;
        add_ie_len -= ie_len;
    }

    // Now we put the HT capabilities
    if (me_env.ht_supported)
    {
        co_write8p(ie_buf++, MAC_ELTID_HT_CAPA);
        co_write8p(ie_buf++, MAC_HT_CAPA_ELMT_LEN);
        co_pack8p(ie_buf, (uint8_t const *)&me_env.ht_cap, MAC_HT_CAPA_ELMT_LEN);
        ie_buf += MAC_HT_CAPA_ELMT_LEN;
    }

    // And we copy the rest of the additional IEs
    if (add_ie_len)
    {
        co_copy8p(ie_buf, add_ie_buf, add_ie_len);
        ie_buf += add_ie_len;
    }

    #if NX_VHT
    // And finally we add the VHT capability information
    if ((scanu_env.band == PHY_BAND_5G) && (me_env.vht_supported))
    {
        co_write8p(ie_buf++, MAC_ELTID_VHT_CAPA);
        co_write8p(ie_buf++, MAC_VHT_CAPA_ELMT_LEN);
        co_pack8p(ie_buf, (uint8_t const *)&me_env.vht_cap, MAC_VHT_CAPA_ELMT_LEN);
        ie_buf += MAC_VHT_CAPA_ELMT_LEN;
    }
    #endif

    // Return the complete length
    return (ie_buf - CPU2HW(ie->buf));
}


/**
 ****************************************************************************************
 * @brief DMA callback.
 *
 * This function proceeds to the next step of the scanning procedure
 *
 ****************************************************************************************
 */
static void scanu_dma_cb(void *env, int dma_type)
{
    // Scan the next requested band
    scanu_scan_next();
}


/**
 ****************************************************************************************
 * @brief Prepare the download of the additional IEs.
 *
 * If no additional IEs are present, or too long, then this function proceeds directly
 * to the scan.
 *
 ****************************************************************************************
 */
static void scanu_ie_download(void)
{
    struct scanu_start_req const *param = scanu_env.param;
    struct scanu_add_ie_tag *ie_desc = &scanu_add_ie;
    struct dma_desc *hw_dma = &ie_desc->dma_desc;
    struct hal_dma_desc_tag *gp_dma = &scanu_env.dma_desc;

    // Check if we have an IE buffer for this band
    if ((param->add_ies == 0) || (param->add_ie_len > SCANU_MAX_IE_LEN))
    {
        // Start the scan procedure immediately
        scanu_scan_next();
    }
    else
    {
        // Initialization of the HW DMA descriptor
        hw_dma->src = param->add_ies;
        hw_dma->length = param->add_ie_len;

        // Push the DMA request
        hal_dma_push(gp_dma, DMA_DL);
    }
}

void scanu_confirm(uint8_t status)
{
    // check if it was a fast scan request
    if (scanu_env.fast)
    {
        // send the confirmation message
        ke_msg_send_basic(SCANU_FAST_CFM, scanu_env.src_id, TASK_SCANU);
    }
    else
    {
        struct scanu_start_cfm* cfm;
		
        // allocate message
        if (scanu_env.joining)
        {
        	//os_printf("SCANU_JOIN_CFM\r\n");
            cfm = KE_MSG_ALLOC(SCANU_JOIN_CFM, scanu_env.src_id, TASK_SCANU, scanu_start_cfm);
        }
        else
        {
        	//os_printf("SCANU_START_CFM\r\n");
            cfm = KE_MSG_ALLOC(SCANU_START_CFM, scanu_env.src_id, TASK_SCANU, scanu_start_cfm);
        }
        // fill in the message parameters
        cfm->status = status;

        // Free the scan parameters structure
        ke_msg_free(ke_param2msg(scanu_env.param));
        scanu_env.param = NULL;

        // send the message to the sender
        ke_msg_send(cfm);
    }

    // move to IDLE state
    ke_state_set(TASK_SCANU, SCANU_IDLE);
}

void scanu_init(void)
{
    struct scanu_add_ie_tag *ie_desc = &scanu_add_ie;
    struct hal_dma_desc_tag *gp_dma = &scanu_env.dma_desc;

    // set the state of the task to default
    ke_state_set(TASK_SCANU, SCANU_IDLE);

    // reset the SCAN environment
    os_memset(&scanu_env, 0, sizeof(scanu_env));

    // Initialization of the GP DMA descriptor
    gp_dma->cb = scanu_dma_cb;
    gp_dma->env = NULL;
    gp_dma->dma_desc = &ie_desc->dma_desc;

    // Initialization of the HW DMA descriptor
    ie_desc->dma_desc.dest = CPU2HW(ie_desc->buf);
}

int scanu_frame_handler(struct rxu_mgt_ind const *frame)
{    
	int i;
    struct mac_scan_result *scan;
    struct scanu_start_req const *param = scanu_env.param;
    uint32_t elmt_addr, var_part_addr, var_part_len;
    uint32_t ht_op_addr = 0, vht_op_addr = 0;
    uint32_t edca_temp = 0;
    struct bcn_frame const *frm = (struct bcn_frame const *)frame->payload;
    int msg_status = KE_MSG_CONSUMED;
    struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];
	
    do
    {
        // Check if we are in a scanning process
        if (ke_state_get(TASK_SCANU) != SCANU_SCANNING)
        {
            break;
        }

        // find a scan result that has the same BSSID (or allocate it)
        scan = scanu_find_result(&frm->h.addr3, true);
        if (scan == NULL)
        {
            break;
        }

        // check if the scan scan matches the SCAN requested BSSID
        if (!MAC_ADDR_GROUP(&scanu_env.bssid) 
			&& !MAC_ADDR_CMP(&frm->h.addr3, &scanu_env.bssid))
        {
            break;
        }

        // copy the BSSID
        MAC_ADDR_CPY(&scan->bssid, &frm->h.addr3);

        // Retrieve the constant fields
        scan->beacon_period = frm->bcnint;
        scan->cap_info = frm->capa;

        // ESS or IBSS
        if ((scan->cap_info & MAC_CAPA_ESS) == MAC_CAPA_ESS)
        {
            scan->bsstype = INFRASTRUCTURE_MODE;
        }
        else
        {
            scan->bsstype = INDEPENDENT_BSS_MODE;
        }

        // Initialize the variable part address
        var_part_addr = CPU2HW(frm->variable);
        var_part_len = frame->length - MAC_BEACON_VARIABLE_PART_OFT;

        // retrieve the SSID if broadcasted
        elmt_addr = mac_ie_find(var_part_addr, var_part_len, MAC_ELTID_SSID);
        if (elmt_addr != 0)
        {
            // first, get the SSID length
            uint8_t ssid_len = co_read8p(elmt_addr + MAC_SSID_LEN_OFT);
			if (ssid_len > 0) { /* yhb added. For hidden ssid's AP, beacon's ssid len is 0, but probe response has ssid */
	            if (ssid_len > MAC_SSID_LEN)
	                ssid_len = MAC_SSID_LEN;
	            scan->ssid.length = ssid_len;
	            // copy the SSID length
	            co_unpack8p(scan->ssid.array, elmt_addr + MAC_SSID_SSID_OFT, ssid_len);
			}
        }
        

        #if (NX_P2P)
        if (scanu_env.p2p_scan)
        {
            if (memcmp(&scan->ssid.array[0], P2P_SSID_WILDCARD, P2P_SSID_WILDCARD_LEN))
            {
                break;
            }
        }
        else
        #endif //(NX_P2P)
        {
            // Check if the SSID is one we are looking for
            if (param->ssid_cnt)
            {
                for (i = 0; i < param->ssid_cnt; i++)
                {
                    struct mac_ssid const *ssid = &param->ssid[i];
                    if ((ssid->length == 0) || (MAC_SSID_CMP(ssid, &scan->ssid)))
                    {
                        // Received SSID is an expected one
                        break;
                    }
                }
                // Check if we found one SSID matching the received one
                if (i == param->ssid_cnt)
                {
                    // Received SSID does not match
                    break;
                }
            }
        }

        // retrieve the channel
        elmt_addr = mac_ie_find(var_part_addr, var_part_len, MAC_ELTID_DS);
        if (elmt_addr != 0)
        {
            uint8_t ch_nbr = co_read8p(elmt_addr + MAC_DS_CHANNEL_OFT);
            scan->chan = me_freq_to_chan_ptr(frame->band,
                                             phy_channel_to_freq(frame->band, ch_nbr));
            // check if the RSSI of the received beacon is greater than the previous one
            if (frame->rssi > scan->rssi)
            {
                scan->rssi = frame->rssi;
            }
        }
        else
        {
            // check if the RSSI of the received beacon is greater than the previous one
            if (frame->rssi > scan->rssi)
            {
                scan->chan = me_freq_to_chan_ptr(frame->band, frame->center_freq);
                scan->rssi = frame->rssi;
            }
        }

        // Check if we are in the joining procedure
        if (scanu_env.joining)
        {
            struct mac_bss_info *bss = &vif->bss_info;
            // Rate field
            struct mac_rates ratefield;

            // Copy some parameters from the scan result
            bss->bsstype = scan->bsstype;
            bss->bssid = scan->bssid;
            bss->cap_info = scan->cap_info;
            bss->beacon_period = scan->beacon_period;
            bss->ssid = scan->ssid;
            bss->chan = scan->chan;
            bss->valid_flags = 0;

            // retrieve the rate set (normal + extended)
            me_extract_rate_set(var_part_addr, var_part_len, &(bss->rate_set));
            me_rate_bitfield_legacy_build(&ratefield, &(bss->rate_set), false);

            // Get highest allowed 11b rate
            bss->high_11b_rate    = 31 - co_clz(me_basic_rate_bitfield_build(&(bss->rate_set)));
            // Get highest allowed legacy rate
            bss->high_legacy_rate = 31 - co_clz(ratefield.legacy);

            // retrieve the EDCA Parameter
            elmt_addr = mac_vsie_find(var_part_addr, var_part_len,
                                      (uint8_t const *)"\x00\x50\xF2\x02\x01", 5);
            if (elmt_addr != 0)
            {
                bss->edca_param.qos_info = co_read8p(elmt_addr + MAC_OUI_PARAM_QOS_INFO_OFT);

                bss->cap_info |= MAC_CAPA_QOS;

                // EDCA Parameter Read from frame is 0xTTTTCCAA
                // TTTT = TXOP
                // CC = Contention Window Parameters
                // AA = AIFSN Parameter.
                // This has to re-arranged as 0xAACCTTTT to as per spec. for EDCA Param
                // So to extract and display in report will be correct
                edca_temp = co_htowl(co_read32p(elmt_addr + MAC_OUI_PARAM_BE_PARAM_OFT));
                // ACM bit information
                bss->edca_param.acm = ((edca_temp & CO_BIT(4)) != 0) << AC_BE;
                // AIFSN BYTE alignment
                bss->edca_param.ac_param[AC_BE] = edca_temp & 0x0F;
                // CW BYTE alignment
                bss->edca_param.ac_param[AC_BE] |= ((edca_temp >> 8) & 0xFFFFFFF) << 4;

                edca_temp = co_htowl(co_read32p(elmt_addr + MAC_OUI_PARAM_BK_PARAM_OFT));
                // ACM bit information
                bss->edca_param.acm |= ((edca_temp & CO_BIT(4)) != 0) << AC_BK;
                // AIFSN BYTE alignment
                bss->edca_param.ac_param[AC_BK] = edca_temp & 0x0F;
                // CW BYTE alignment.
                bss->edca_param.ac_param[AC_BK] |= ((edca_temp >> 8) & 0xFFFFFFF) << 4;

                edca_temp = co_htowl(co_read32p(elmt_addr + MAC_OUI_PARAM_VI_PARAM_OFT));
                // ACM bit information
                bss->edca_param.acm |= ((edca_temp & CO_BIT(4)) != 0) << AC_VI;
                // AIFSN BYTE alignment
                bss->edca_param.ac_param[AC_VI] = edca_temp & 0x0F;
                // CW BYTE alignment
                bss->edca_param.ac_param[AC_VI] |= ((edca_temp >> 8) & 0xFFFFFFF) << 4;

                edca_temp = co_htowl(co_read32p(elmt_addr + MAC_OUI_PARAM_VO_PARAM_OFT));
                // ACM bit information
                bss->edca_param.acm |= ((edca_temp & CO_BIT(4)) != 0) << AC_VO;
                // AIFSN BYTE alignment
                bss->edca_param.ac_param[AC_VO] = edca_temp & 0x0F;
                // CW BYTE alignment
                bss->edca_param.ac_param[AC_VO] |= ((edca_temp >> 8) & 0xFFFFFFF) << 4;
                bss->valid_flags |= BSS_QOS_VALID;
            }

            // retrieve the HT Capability
            if (me_env.ht_supported)
            {
                elmt_addr = mac_ie_find(var_part_addr, var_part_len, MAC_ELTID_HT_CAPA);
                if (elmt_addr != 0)
                {
                    bss->ht_cap.ht_capa_info = co_wtohs(co_read16p(elmt_addr + MAC_HT_CAPA_INFO_OFT));

                    bss->ht_cap.a_mpdu_param = co_read8p(elmt_addr + MAC_HT_CAPA_AMPDU_PARAM_OFT);

                    co_unpack8p(bss->ht_cap.mcs_rate,
                                elmt_addr + MAC_HT_CAPA_SUPPORTED_MCS_SET_OFT,
                                MAX_MCS_LEN);
                    bss->ht_cap.ht_extended_capa =
                        co_wtohs(co_read16p(elmt_addr + MAC_HT_CAPA_EXTENDED_CAPA_OFT));

                    bss->ht_cap.tx_beamforming_capa =
                        co_wtohs(co_read16p(elmt_addr + MAC_HT_CAPA_TX_BEAM_FORMING_CAPA_OFT));

                    bss->ht_cap.asel_capa = co_read8p(elmt_addr + MAC_HT_CAPA_ASEL_CAPA_OFT);
                    bss->valid_flags |= BSS_HT_VALID;
                }

                // retrieve the HT operation field
                ht_op_addr = mac_ie_find(var_part_addr, var_part_len, MAC_ELTID_HT_OPERATION);

                #if NX_VHT
                if (me_env.vht_supported)
                {
                    elmt_addr = mac_ie_find(var_part_addr, var_part_len, MAC_ELTID_VHT_CAPA);
                    if (elmt_addr != 0)
                    {
                        bss->vht_cap.vht_capa_info = co_wtohl(co_read32p(elmt_addr + MAC_VHT_CAPA_INFO_OFT));
                        bss->vht_cap.rx_mcs_map = co_wtohs(co_read16p(elmt_addr + MAC_VHT_RX_MCS_MAP_OFT));
                        bss->vht_cap.tx_mcs_map = co_wtohs(co_read16p(elmt_addr + MAC_VHT_TX_MCS_MAP_OFT));
                        bss->vht_cap.rx_highest = co_wtohs(co_read16p(elmt_addr + MAC_VHT_RX_HIGHEST_RATE_OFT));
                        bss->vht_cap.tx_highest = co_wtohs(co_read16p(elmt_addr + MAC_VHT_TX_HIGHEST_RATE_OFT));
                        bss->valid_flags |= BSS_VHT_VALID;
                    }

                    // retrieve the VHT operation field
                    vht_op_addr = mac_ie_find(var_part_addr, var_part_len, MAC_ELTID_VHT_OPERATION);
                }
                #endif
            }

            // Initialize the BW and channel parameters
            me_bw_check(ht_op_addr, vht_op_addr, bss);

            // Get power constraint
            me_extract_power_constraint(var_part_addr, var_part_len, bss);

            // Get regulatory rules
            me_extract_country_reg(var_part_addr, var_part_len, bss);

            // Get Mobility Domain ID and FT capability and policy
            me_extract_mobility_domain(var_part_addr, var_part_len, bss);

            // We consider now the BSS information as valid
            bss->valid_flags |= BSS_INFO_VALID;
        }

        // check if the element was already allocated
        if (!scan->valid_flag)
        {
            // one more scan scan is saved
            scanu_env.result_cnt++;
        }

        // set the valid_flag
        scan->valid_flag = true;

        msg_status = KE_MSG_NO_FREE;
        ke_msg_forward_and_change_id(frame, SCANU_RESULT_IND, TASK_API, TASK_SCANU);
    } while(false);

    return msg_status;
}

struct mac_scan_result *scanu_find_result(struct mac_addr const *bssid_ptr,
                                          bool allocate)
{
    uint8_t i=0;
    struct mac_scan_result* scan_rslt = NULL;

    // search in the scan list using the MAC address
    for (i = 0; i < MAX_BSS_LIST; i++)
    {
        struct mac_scan_result *scan = &scanu_env.scan_result[i];

        // if it is a valid BSS.
        if (scan->valid_flag)
        {
            if (MAC_ADDR_CMP(&scan->bssid, bssid_ptr))
            {
                // required BSS found
                scan_rslt = scan;
                break;
            }
        }
        else if (allocate)
        {
            // empty entry: if allocation was requested, then return this pointer
            scan_rslt = scan;
            break;
        }
    }
    return (scan_rslt);
}

struct mac_scan_result *scanu_search_by_bssid(struct mac_addr const *bssid)
{
    return (scanu_find_result(bssid, false));
}

struct mac_scan_result *scanu_search_by_ssid(struct mac_ssid const *ssid)
{
    uint8_t i;
    int8_t rssi = 0x80;
    struct mac_scan_result* scan_rslt = NULL;

    do
    {
        // Check if the SSID is valid
        if (!ssid->length)
            break;

        // Search in the scan list using the SSID. If several BSSes share the same SSID,
        // the one with the highest RSSI will be returned.
        for (i = 0; i < MAX_BSS_LIST; i++)
        {
            struct mac_scan_result *scan = &scanu_env.scan_result[i];

            // Check if it is a valid BSS.
            if (!scan->valid_flag)
                break;

            // Check if the SSID is matching and if the RSSI is greater than the previous
            // one that was found
            if ((scan->rssi > rssi) && (MAC_SSID_CMP(&scan->ssid, ssid)))
            {
                scan_rslt = scan;
                rssi = scan->rssi;
            }
        }
    } while(0);

    return (scan_rslt);
}

void scanu_start(void)
{
    int i;

    if (!scanu_env.joining)
    {
        // reset the scan results before starting a new scan
        for (i = 0; i < MAX_BSS_LIST; i++)
        {
        	scanu_env.scan_result[i].ssid.length = 0;
            scanu_env.scan_result[i].valid_flag = false;
            scanu_env.scan_result[i].rssi = 0x80;
        }

        // reset the valid result counter
        scanu_env.result_cnt = 0;
    }

    #if (NX_P2P)
    if ((scanu_env.param->ssid_cnt == 1) && (scanu_env.param->ssid[0].length == P2P_SSID_WILDCARD_LEN) &&
         !memcmp(&scanu_env.param->ssid[0].array[0], P2P_SSID_WILDCARD, P2P_SSID_WILDCARD_LEN))
    {
        scanu_env.p2p_scan = true;
    }
    else
    {
        scanu_env.p2p_scan = false;
    }
    #endif //(NX_P2P)

    // move to SCANNING state (done before scanu_next because it could reset the state)
    ke_state_set(TASK_SCANU, SCANU_SCANNING);

    // Download the additional IE if available
    scanu_ie_download();
}

void scanu_scan_next(void)
{
    int i;
    struct scanu_start_req const *param = scanu_env.param;
    struct scan_start_req *req;

    do
    {
        // Check if we scanned all the requested bands
        if (scanu_env.band > PHY_BAND_5G)
            break;

        // Go through the list of channels to find the first channel to be scanned
        for (i = 0; i < param->chan_cnt; i++)
        {
            if (param->chan[i].band == scanu_env.band)
                break;
        }

        // Check if we have at least one channel to scan
        if (i == param->chan_cnt)
        {
            scanu_env.band++;
            continue;
        }

        // Allocate the scan request for the LMAC
        req = KE_MSG_ALLOC(SCAN_START_REQ, TASK_SCAN, TASK_SCANU, scan_start_req);

        // Fill-in the request
        req->vif_idx = param->vif_idx;
        req->bssid = param->bssid;
        req->ssid_cnt = param->ssid_cnt;
        req->no_cck = param->no_cck;

        // Prepare the channel list
        for (; i < param->chan_cnt; i++)
        {
            // Check if the current channel is on the band we have to scan
            if (param->chan[i].band == scanu_env.band)
            {
                req->chan[req->chan_cnt] = param->chan[i];
                req->chan_cnt++;
            }
        }
        // Prepare the SSIDs
        for (i = 0; i < param->ssid_cnt; i++)
        {
            req->ssid[i] = param->ssid[i];
        }

        // Prepare the IE buffer
        req->add_ie_len = scanu_build_ie();
        req->add_ies = 0;

        // send a scan start request
        ke_msg_send(req);

        return;
    } while(1);

    // Scan is done
    scanu_confirm(CO_OK);
}

/// @} end of group

