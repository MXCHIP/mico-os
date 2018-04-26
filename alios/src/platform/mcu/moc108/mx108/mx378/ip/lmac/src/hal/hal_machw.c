/**
 ****************************************************************************************
 *
 * @file hal_machw.c
 *
 * @brief Implementation of the Initialization of Interrupt control registers and handling
 * of the Interrupts
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 *****************************************************************************************
 * @addtogroup MACHW
 * @{
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "arch.h"
#include "co_endian.h"
#include "mac_defs.h"
#include "mm.h"
#include "ke_event.h"
#include "hal_machw.h"
#include "rxl_cntrl.h"
#include "reg_mac_core.h"
#include "reg_mac_pl.h"
#include "ps.h"

#include "include.h"
#include "arm_arch.h"
#include "intc_pub.h"
#include "mem_pub.h"
#include "uart_pub.h"

/// FSMs and FIFOs that have to be reset by the error recovery mechanism
#define MAC_HW_RESET (NXMAC_HW_FSM_RESET_BIT | NXMAC_RX_FIFO_RESET_BIT |                \
                      NXMAC_TX_FIFO_RESET_BIT | NXMAC_MAC_PHYIFFIFO_RESET_BIT |         \
                      NXMAC_ENCR_RX_FIFO_RESET_BIT)

/// Error interrupts
#define HW_ERROR_IRQ (NXMAC_RX_DMA_EMPTY_BIT | NXMAC_RX_FIFO_OVER_FLOW_BIT |            \
                      NXMAC_PT_ERROR_BIT | NXMAC_HW_ERR_BIT | NXMAC_PHY_ERR_BIT |       \
                      NXMAC_AC_0_TX_DMA_DEAD_BIT | NXMAC_AC_1_TX_DMA_DEAD_BIT |         \
                      NXMAC_AC_2_TX_DMA_DEAD_BIT | NXMAC_AC_3_TX_DMA_DEAD_BIT |         \
                      NXMAC_BCN_TX_DMA_DEAD_BIT | NXMAC_MAC_PHYIF_UNDER_RUN_BIT |       \
                      NXMAC_RX_HEADER_DMA_DEAD_BIT | NXMAC_RX_PAYLOAD_DMA_DEAD_BIT)

/// Timeout error interrupts
#define TIMEOUT_IRQ  (HAL_AC0_TIMER_BIT | HAL_AC1_TIMER_BIT | HAL_AC2_TIMER_BIT |       \
                      HAL_AC3_TIMER_BIT | HAL_BCN_TIMER_BIT | HAL_IDLE_TIMER_BIT)

/// TX interrupt bits
#if NX_AMSDU_TX
#define TX_IRQ (NXMAC_AC_0_TX_TRIGGER_BIT | NXMAC_AC_1_TX_TRIGGER_BIT |                 \
                NXMAC_AC_2_TX_TRIGGER_BIT | NXMAC_AC_3_TX_TRIGGER_BIT |                 \
                NXMAC_BCN_TX_TRIGGER_BIT | NXMAC_AC_0_TX_BUF_TRIGGER_BIT |              \
                NXMAC_AC_1_TX_BUF_TRIGGER_BIT | NXMAC_AC_2_TX_BUF_TRIGGER_BIT |         \
                NXMAC_AC_3_TX_BUF_TRIGGER_BIT | NXMAC_BCN_TX_BUF_TRIGGER_BIT | MU_MIMO_MASTER_TX_IRQ)
#else
#define TX_IRQ (NXMAC_AC_0_TX_TRIGGER_BIT | NXMAC_AC_1_TX_TRIGGER_BIT |                 \
                NXMAC_AC_2_TX_TRIGGER_BIT | NXMAC_AC_3_TX_TRIGGER_BIT |                 \
                NXMAC_BCN_TX_TRIGGER_BIT | MU_MIMO_MASTER_TX_IRQ)
#endif

#if (NX_BW_LEN_ADAPT)
#define BW_DROP_IRQ (NXMAC_AC_0BW_DROP_TRIGGER_BIT | NXMAC_AC_1BW_DROP_TRIGGER_BIT |    \
                     NXMAC_AC_2BW_DROP_TRIGGER_BIT | NXMAC_AC_3BW_DROP_TRIGGER_BIT)
#else
#define BW_DROP_IRQ  0
#endif

/// Timeout duration when a IDLE state is requested to the HW
#define IDLE_REQ_TIMEOUT  50000


#if NX_MULTI_ROLE
/// Table of conversion between a RX vector rate to a MAC HW rate
const uint8_t rxv2macrate[] = {
    0,                          /* 0 */
    1,                          /* 1 */
    2,                          /* 2 */
    3,                          /* 3 */
    (uint8_t)-1,                /* 4 */
    (uint8_t)-1,                /* 5 */
    (uint8_t)-1,                /* 6 */
    (uint8_t)-1,                /* 7 */
    10,                         /* 8 */
    8,                          /* 9 */
    6,                          /* 10 */
    4,                          /* 11 */
    11,                         /* 12 */
    9,                          /* 13 */
    7,                          /* 14 */
    5                           /* 15 */
};
#endif

static uint32_t g_entry_id = 0;
MONITOR_PTH_T *g_monitor_pth = 0;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief IDLE Interrupt Handler

 * This function handles the idle interrupt raised when the hardware changes to idle
 * state. The PS module sets the HW to idle and waits for this interrupt to change the HW
 * state to DOZE. The MAC management module uses this interrupt to program the HW to next
 * relevant state.
 ****************************************************************************************
 */
static void hal_machw_idle_irq_handler(void)
{
    // Sanity check: Ensure that the HW is effectively in IDLE state
    ASSERT_REC(nxmac_current_state_getf() == HW_IDLE);
    
    // Disable the go to IDLE timer interrupt
    nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() & ~HAL_IDLE_TIMER_BIT);

    #if NX_POWERSAVE
    // We can sleep again
    ps_env.prevent_sleep &= ~PS_IDLE_REQ_PENDING;
    #endif

    // Trigger the IDLE state event
    ke_evt_set(KE_EVT_HW_IDLE_BIT);
}

void hal_machw_idle_req(void)
{
    uint32_t curr_time;
    
    GLOBAL_INT_DECLARATION();

    // Sanity check: HW state is not supposed to be IDLE to handle this request
    ASSERT_REC(nxmac_current_state_getf() != HW_IDLE);

    
    // Enable timer irq
    GLOBAL_INT_DISABLE();
    
    curr_time = hal_machw_time();
    nxmac_abs_timer_set(HAL_IDLE_TIMER, curr_time + IDLE_REQ_TIMEOUT);

    nxmac_timers_int_event_clear(HAL_IDLE_TIMER_BIT);
    nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() 
									| HAL_IDLE_TIMER_BIT);

    // Request to MAC HW to switch to IDLE state
    nxmac_next_state_setf(HW_IDLE);

    #if NX_POWERSAVE
    // Prevent from sleeping during the switch to IDLE
    ps_env.prevent_sleep |= PS_IDLE_REQ_PENDING;
    #endif

    GLOBAL_INT_RESTORE();
}


void hal_machw_stop(void)
{
    // Reset all the MAC HW state machines and registers
    nxmac_soft_reset_setf(1);
    while (nxmac_soft_reset_getf());
}

void hal_machw_init(void)
{	
	intc_service_register(FIQ_MAC_GENERAL, PRI_FIQ_MAC_GENERAL, hal_machw_gen_handler); 
	
    // Soft Reset
    #ifndef KEIL_SIMULATOR
    nxmac_soft_reset_setf(1);
    while (nxmac_soft_reset_getf());
    #endif

    // enable MAC HW general interrupt events
    nxmac_gen_int_enable_set(NXMAC_MASTER_GEN_INT_EN_BIT 
							    | NXMAC_RX_FIFO_OVER_FLOW_BIT 
								| NXMAC_PT_ERROR_BIT 
								| NXMAC_AC_0_TX_DMA_DEAD_BIT 
								| NXMAC_IDLE_INTERRUPT_BIT 
								| NXMAC_AC_1_TX_DMA_DEAD_BIT 
								| NXMAC_AC_2_TX_DMA_DEAD_BIT 
								| NXMAC_AC_3_TX_DMA_DEAD_BIT 
								| NXMAC_BCN_TX_DMA_DEAD_BIT 
								| NXMAC_HW_ERR_BIT 
								| NXMAC_MAC_PHYIF_UNDER_RUN_BIT 
								| NXMAC_PHY_ERR_BIT 
								| NXMAC_RX_HEADER_DMA_DEAD_BIT 
								| NXMAC_RX_PAYLOAD_DMA_DEAD_BIT 
								| NXMAC_ABS_GEN_TIMERS_BIT 
								| NXMAC_MAC_PHYIF_OVERFLOW_BIT);

    #if NX_MDM_VER >= 20
    // delegate MPIF interface flow control to PHY
    nxmac_rate_controller_mpif_setf(0);
    #else
    // On old versions of the Modem enable the rxEndForTiming Error Recovery
    nxmac_rx_end_for_timing_err_rec_setf(1);
    #endif

    #if NX_KEY_RAM_CONFIG
    ASSERT_ERR(MM_STA_TO_KEY(NX_REMOTE_STA_MAX - 1) <= nxmac_sta_key_max_index_getf());
    nxmac_encr_ram_config_pack(NX_VIRT_DEV_MAX, MM_STA_TO_KEY(NX_REMOTE_STA_MAX - 1),
                               MM_STA_TO_KEY(0));
    #endif

    // enable MAC HW TX and RX interrupt events
    nxmac_tx_rx_int_enable_set(TX_IRQ 
								| BW_DROP_IRQ
								| NXMAC_TIMER_RX_TRIGGER_BIT 
								| NXMAC_COUNTER_RX_TRIGGER_BIT
								| NXMAC_MASTER_TX_RX_INT_EN_BIT);

    #if RW_MUMIMO_TX_EN
    nxmac_sec_users_tx_int_event_un_mask_set(TX_SEC_IRQ_BITS | NXMAC_MASTER_SEC_USERS_TX_INT_EN_BIT);
    #endif

    // At initialization we are not supposed to reply to any frame (in case monitor mode
    // would be used), so disable the auto-reply capabilities of the HW
    nxmac_mac_cntrl_1_set(nxmac_mac_cntrl_1_get() | NXMAC_DISABLE_ACK_RESP_BIT |
                                                    NXMAC_DISABLE_CTS_RESP_BIT |
                                                    NXMAC_DISABLE_BA_RESP_BIT |
                                                    NXMAC_ACTIVE_CLK_GATING_BIT |
                                                    NXMAC_ENABLE_LP_CLK_SWITCH_BIT |
                                                    NXMAC_RX_RIFS_EN_BIT);

    // enable the RX flow control
    nxmac_rx_flow_cntrl_en_setf(1);

    // enable reception of all frames (i.e. monitor mode)
    nxmac_rx_cntrl_set(MM_RX_FILTER_MONITOR);

    // initialize the RX trigger timers (RPD count reached for trig 1/4 of available , packet timeout is 1*32us, absolute is 10*32us)
    nxmac_rx_trigger_timer_pack(NX_RX_PAYLOAD_DESC_CNT/4, 1, 10);

    // put default values of the beacon timings
    nxmac_bcn_cntrl_1_pack(255, 1, HAL_MACHW_BCN_TX_DELAY_US / 128, 100);

    // limit the maximum RX size to the maximum A-MSDU size we support
    nxmac_max_rx_length_set(RWNX_MAX_AMSDU_RX);

    // configure the EDCA control register
    nxmac_edca_cntrl_pack(0, 0, 0, 0);

    // Default power level
    nxmac_max_power_level_pack(0x20, 0x20);

    // reset MIB Table
    nxmac_mib_table_reset_setf(1);

    // reset Key storage RAM
    nxmac_key_sto_ram_reset_setf(1);

    // Initialize diagnostic ports
    nxmac_debug_port_sel_pack(0x1C, 0x25);

    // Enable the dynamic bandwidth feature
    nxmac_dyn_bw_en_setf(1);

    // Set the number of TX chains the HW shall use for immediate responses
    nxmac_max_phy_ntx_setf(phy_get_ntx() + 1);

    #if NX_MULTI_ROLE
    // If Multi-role is enabled, the SW manages by itself the STA TSF. It it
    // therefore necessary to disable the corresponding HW feature.
    nxmac_tsf_mgt_disable_setf(1);
    #endif

    #if RW_BFMEE_EN
    // Check if beamformee is supported by the MAC HW and PHY
    if (hal_machw_bfmee_support())
    {
        // Initialize beamformee registers
        nxmac_bfmee_nc_setf(0);
        nxmac_bfmee_nr_setf(3);
        nxmac_bfmee_codebook_setf(1);
        nxmac_bfmee_enable_setf(1);
        #if (RW_MUMIMO_RX_EN)
        if (hal_machw_mu_mimo_rx_support())
            nxmac_bfmee_mu_support_setf(1);
        #endif
    }
    #endif
}

void hal_machw_disable_int(void)
{
    nxmac_enable_master_gen_int_en_setf(0);
    nxmac_enable_master_tx_rx_int_en_setf(0);
}

void hal_machw_reset(void)
{
    // Disable clock gating during SW reset
    nxmac_active_clk_gating_setf(0);

    // Request to MAC HW to switch to IDLE state, in order to ensure that it will be
    // IDLE after reset of state machines and FIFOs
    nxmac_next_state_setf(HW_IDLE);

    // Reset state machines and FIFOs
    nxmac_mac_err_rec_cntrl_set(MAC_HW_RESET);
    //while (nxmac_mac_err_rec_cntrl_get() & MAC_HW_RESET);
    while (nxmac_current_state_getf() != HW_IDLE);

    #if NX_POWERSAVE
    // We are in IDLE so no need to wait anymore for the IDLE interrupt
    ps_env.prevent_sleep &= ~PS_IDLE_REQ_PENDING;
    #endif

    // Reenable the RX flow control
    nxmac_rx_flow_cntrl_en_setf(1);

    // Disable the timeout error interrupts
    nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() & ~TIMEOUT_IRQ);

    // Acknowledge possibly pending interrupts
    nxmac_tx_rx_int_ack_clear(0xFFFFFFFF);
    nxmac_gen_int_ack_clear(NXMAC_IDLE_INTERRUPT_BIT 
								| HW_ERROR_IRQ 
								| NXMAC_IMP_PRI_TBTT_BIT 
								| NXMAC_IMP_PRI_DTIM_BIT 
								| NXMAC_IMP_SEC_TBTT_BIT 
								| NXMAC_IMP_SEC_DTIM_BIT);

    nxmac_enable_master_gen_int_en_setf(1);
    nxmac_enable_master_tx_rx_int_en_setf(1);

    // Re-enable clock gating during SW reset
    nxmac_active_clk_gating_setf(1);
}

uint8_t hal_machw_search_addr(struct mac_addr *addr)
{
    uint8_t sta_idx = INVALID_STA_IDX;
    uint32_t enc_cntrl;

    // Copy the MAC addr
    nxmac_encr_mac_addr_low_set(addr->array[0] | (((uint32_t)addr->array[1]) << 16));
    nxmac_encr_mac_addr_high_set(addr->array[2]);

    // Write the control field
    nxmac_encr_cntrl_set(NXMAC_NEW_SEARCH_BIT);

    // Poll for the completion of the search
    do
    {
        enc_cntrl = nxmac_encr_cntrl_get();
    } while(enc_cntrl & NXMAC_NEW_SEARCH_BIT);

    // Check if the search was successful or not
    if (!(enc_cntrl & NXMAC_SEARCH_ERROR_BIT))
        // Compute the SW STA index from the HW index
        sta_idx = ((enc_cntrl & NXMAC_KEY_INDEX_RAM_MASK) >> NXMAC_KEY_INDEX_RAM_LSB)
                    - MM_SEC_DEFAULT_KEY_COUNT;

    return (sta_idx);
}

void hal_init_monitor_buf(void)
{
	uint32_t size;
	
	if(g_monitor_pth)
	{
		return;
	}

	size = sizeof(MONITOR_PTH_T)
				* (KEY_ENTRY_MAX_ID - KEY_ENTRY_MIN_ID + 1);
	g_monitor_pth = (MONITOR_PTH_T *)os_zalloc(size);
	ASSERT(g_monitor_pth);
}

void hal_uninit_monitor_buf(void)
{
	if(g_monitor_pth)
	{
		os_free(g_monitor_pth);
		
		g_monitor_pth = 0;
	}
}

uint32_t hal_monitor_get_id(uint64_t address)
{
	MONITOR_PTH_T *l_pth;
	uint32_t addr_count, i;
	uint32_t index = 0;

	l_pth = g_monitor_pth;
	addr_count = KEY_ENTRY_MAX_ID - KEY_ENTRY_MIN_ID + 1;
	for(i = 0; i < addr_count; i ++)
	{
		if(address == l_pth[i].mac_addr)
		{
			index = i + 1;
			break;
		}
	}
	
	return index;
}

uint32_t hal_monitor_get_pth_id(struct mac_hdr *machdr)
{
	uint8_t *addr = 0;
	uint32_t index;
	uint32_t ds_status;
	uint64_t mac_address = 0;

	ds_status = (machdr->fctl >> 8) & 0x03;	
	switch(ds_status)
	{
		case 0:/*ToDS FromDS:0 0 ibss*/
			addr = (uint8_t *)&machdr->addr3;
			break;
			
		case 1:/*ToDS FromDS:1 0 infra*/
			addr = (uint8_t *)&machdr->addr1;
			break;
			
		case 2:/*ToDS FromDS:0 1 infra*/
			addr = (uint8_t *)&machdr->addr2;
			break;
			
		case 3:/*ToDS FromDS:1 1 wds*/
		default:
			break;
	}
	
	if(0 == addr)
	{
		return 0;
	}
	os_memcpy(&mac_address, addr, sizeof(machdr->addr2));

	index = hal_monitor_get_id(mac_address);
	
	return index;
}

uint32_t hal_monitor_record_count(struct mac_hdr *machdr)
{
	uint32_t index;

	index = hal_monitor_get_pth_id(machdr);
	if(index)
	{
		g_monitor_pth[index - 1].count += 1; 
	}
	
	return g_monitor_pth[index - 1].count;
}

uint32_t hal_monitor_get_iv_len(struct mac_hdr *machdr)
{
	uint32_t index;
	uint32_t len = 0;
	uint32_t cipher_type;

	index = hal_monitor_get_pth_id(machdr);
	if(index)
	{
		cipher_type = g_monitor_pth[index - 1].group_cipher_type; 
		if((CTYPERAM_WEP == cipher_type)
			|| (CTYPERAM_TKIP == cipher_type))
	{
			len = 4;
	}
		else if(CTYPERAM_CCMP == cipher_type)
		{
			len = 8;
		}
	}
	return len;
}

uint32_t hal_monitor_printf_buffering_mac_address(void)
{
	MONITOR_PTH_T *l_pth;
	uint32_t *cnt_of_ap;
	uint32_t addr_count, i;

	if(0 == g_monitor_pth)
	{
		return MONITOR_FAILURE;
	}

	l_pth = g_monitor_pth;
	addr_count = KEY_ENTRY_MAX_ID - KEY_ENTRY_MIN_ID + 1;
	
	return MONITOR_SUCCESS;
}

uint32_t hal_monitor_record_pth_info(uint64_t address,
	uint32_t index, uint8_t grp_cipher_type)
{
	MONITOR_PTH_T *local_pth;
	
	if(0 == g_monitor_pth)
	{
		return MONITOR_FAILURE;
	}
	
	local_pth = g_monitor_pth;
	local_pth[index].mac_addr = address;
	local_pth[index].count = 1;
	local_pth[index].group_cipher_type = grp_cipher_type;

	return MONITOR_SUCCESS;
}

uint32_t hal_monitor_is_including_mac_address(uint64_t address)
{
	uint32_t addr_count, i;
	uint32_t hit_flag = 0;
	MONITOR_PTH_T *local_pth;

	if(0 == g_monitor_pth)
	{
		goto check_out;
	}

	local_pth = g_monitor_pth;
	addr_count = KEY_ENTRY_MAX_ID - KEY_ENTRY_MIN_ID + 1;
	for(i = 0; i < addr_count; i ++)
	{
		if(address == local_pth[i].mac_addr)
		{
			hit_flag = 1;
			break;
		}
	}
	
check_out:
	return hit_flag;
}

void hal_program_cipher_key
(
    uint8_t   useDefaultKey,   // Use Default Key
    uint32_t  *cipherKey,       // Cipher Key
    uint64_t   macAddress,      // MAC Address
    uint8_t    cipherLen,       // Cipher Length
    uint8_t    sppRAM,          // SPP RAM
    uint8_t    vlanIDRAM,       // VLAN ID RAM
    uint16_t   keyIndexRAM,     // Key Index RAM
    uint8_t    cipherType,      // Cipher Type
    uint8_t    newWrite,        // New Write
    uint8_t    newRead         // New Read
)   
{
    nxmac_encr_key_0_set(cipherKey[0]);
    nxmac_encr_key_1_set(cipherKey[1]);
    nxmac_encr_key_2_set(cipherKey[2]);
    nxmac_encr_key_3_set(cipherKey[3]);
	
    nxmac_encr_mac_addr_low_set(macAddress);
    nxmac_encr_mac_addr_high_set(macAddress>>32);
	
    nxmac_encr_cntrl_pack(newRead, newWrite, 0, 0, 
							keyIndexRAM, cipherType,
							vlanIDRAM, sppRAM, 
							useDefaultKey, cipherLen);
   
}
 
uint16_t hal_get_secret_key_entry_id(void)
{	
	uint16_t entry_id = 0;
	uint32_t i, num, minv;
	MONITOR_PTH_T *local_pth;

	local_pth = g_monitor_pth;
	minv = local_pth[0].count;
	num = KEY_ENTRY_MAX_ID - KEY_ENTRY_MIN_ID + 1;

	for(i = 0; i < num; i ++)
	{
		if(0 == local_pth[i].count)
		{
			entry_id = i;
			break;
	}
		else if(local_pth[i].count < minv)
	{
			minv = local_pth[i].count;
			entry_id = i;
		}
	}	

	return entry_id;
}

void hal_update_secret_key(uint64_t macAddress,
	uint8_t cipherType)
{	
	uint16_t key_index;	
	uint16_t entry_id;	
	uint32_t    pKey[4] = {0xabc47fd0, 0x57498892, 
							0x11320490, 0x10815562};

	key_index = hal_get_secret_key_entry_id();
	entry_id = key_index + KEY_ENTRY_MIN_ID;

	hal_monitor_record_pth_info(macAddress, key_index, cipherType);

	hal_program_cipher_key(1,	          // useDefaultKey
					pKey,
					0, 		               // macAddress
					1,					   // cipherLen
					0,					   // sppRAM
					cipherType,		 	   // vlanIDRAM
					entry_id,              // keyIndexRAM
					cipherType,		       // cipherType
					1,					   // newWrite
					0); 				   // newRead

	hal_program_cipher_key(1,	           // useDefaultKey
					pKey,
					macAddress, 		   // macAddress
					1,					   // cipherLen
					0,					   // sppRAM
					cipherType,		 	   // vlanIDRAM
					entry_id,			   // keyIndexRAM
					cipherType,		       // cipherType
					1,					   // newWrite
					0); 				   // newRead

}

void hal_init_vlan_cipher(uint8_t useDefaultKey,
	uint8_t vlanIDRAM,
	uint8_t cipherType)
{
	uint32_t    pKey[4] = {0xabc47fd0, 0x57498892, 
							0x11320490, 0x10815562};
	hal_program_cipher_key(useDefaultKey,	// useDefaultKey
					pKey,
					0xFFFFFFFFFFFF, 		// macAddress
					1,					    // cipherLen
					0,					    // sppRAM
					vlanIDRAM,			    // vlanIDRAM 
					(vlanIDRAM << 2) + 0,	// keyIndexRAM
					cipherType,		        // cipherType - TKIP
					1,					    // newWrite
					0); 				    // newRead
					
	hal_program_cipher_key(useDefaultKey,	// useDefaultKey
					pKey,
					0xFFFFFFFFFFFF, 		// macAddress
					1,					    // cipherLen
					0,					    // sppRAM
					vlanIDRAM,			    // vlanIDRAM 
					(vlanIDRAM << 2) + 1,	// keyIndexRAM
					cipherType,		        // cipherType - TKIP
					1,					    // newWrite
					0); 				  // newRead
 
	hal_program_cipher_key(useDefaultKey,	// useDefaultKey
					pKey,
					0xFFFFFFFFFFFF, 		// macAddress
					1,					    // cipherLen
					0,					    // sppRAM
					vlanIDRAM,			    // vlanIDRAM 
					(vlanIDRAM << 2) + 2,	// keyIndexRAM
					cipherType,		        // cipherType - TKIP
					1,					    // newWrite
					0);					 // newRead
 
	hal_program_cipher_key(useDefaultKey,	// useDefaultKey
					pKey,
					0xFFFFFFFFFFFF, 		// macAddress
					1,					    // cipherLen
					0,					    // sppRAM
					vlanIDRAM,			    // vlanIDRAM
					(vlanIDRAM << 2) + 3,	// keyIndexRAM
					cipherType,		        // cipherType - TKIP
					1,					    // newWrite
					0); 

}

void hal_init_cipher_keys(void)
{
    // reset Key storage RAM
    nxmac_key_sto_ram_reset_setf(1);  
	g_entry_id = 0;

	hal_init_vlan_cipher(0,0,0); // null key
	hal_init_vlan_cipher(0,1,1); // wep
	hal_init_vlan_cipher(0,2,2); // tkip
	hal_init_vlan_cipher(0,3,3); // ccmp
}

void hal_machw_enter_monitor_mode(void)
{	
    os_printf("hal_machw_enter_monitor_mode\r\n");
	g_entry_id = 0;
	hal_init_monitor_buf();
	
    nxmac_enable_imp_pri_tbtt_setf(0); // 0xC0008074
    nxmac_enable_imp_sec_tbtt_setf(0);

    // Disable the auto-reply capabilities of the HW reg:0xC000004C
    nxmac_mac_cntrl_1_set(nxmac_mac_cntrl_1_get() 
						    | NXMAC_DISABLE_ACK_RESP_BIT 
						    | NXMAC_DISABLE_CTS_RESP_BIT 
						    | NXMAC_DISABLE_BA_RESP_BIT);

    // Enable reception of all frames (i.e. monitor mode)
    mm_rx_filter_umac_set(0xFFFFFFFF & ~(NXMAC_EXC_UNENCRYPTED_BIT
    									| NXMAC_ACCEPT_BAR_BIT 
    									| NXMAC_ACCEPT_ERROR_FRAMES_BIT
                                        | NXMAC_ACCEPT_BA_BIT 
                                        | NXMAC_ACCEPT_CTS_BIT
                                        | NXMAC_ACCEPT_RTS_BIT
                                        | NXMAC_ACCEPT_ACK_BIT
                                        | NXMAC_ACCEPT_PS_POLL_BIT
                                        | NXMAC_ACCEPT_QO_S_NULL_BIT
                                        | NXMAC_ACCEPT_QO_S_NULL_BIT
                                        | NXMAC_ACCEPT_CF_END_BIT
                                        | NXMAC_ACCEPT_UNKNOWN_BIT
                                        | NXMAC_ACCEPT_CFWO_DATA_BIT));
    
	// set default mode of operation
    nxmac_abgn_mode_setf(MODE_802_11N_5);

	hal_init_cipher_keys();
}


void hal_machw_exit_monitor_mode(void)
{
    os_printf("hal_machw_exit_monitor_mode\r\n");
		
	nxmac_beacon_int_setf(2000);
    nxmac_enable_imp_pri_tbtt_setf(1);
    nxmac_enable_imp_sec_tbtt_setf(1);

    // Enable the auto-reply capabilities of the HW
    nxmac_mac_cntrl_1_set(nxmac_mac_cntrl_1_get() 
						    & (~NXMAC_DISABLE_ACK_RESP_BIT) 
						    & (~NXMAC_DISABLE_CTS_RESP_BIT) 
						    & (~NXMAC_DISABLE_BA_RESP_BIT));

    // Enable reception of some frames (i.e. active mode)
    mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE);
	
	hal_uninit_monitor_buf();
}

bool hal_machw_sleep_check(void)
{
	int i;
    uint32_t timer_msk = nxmac_timers_int_un_mask_get();

    #if !NX_MULTI_ROLE
    uint32_t tbtt = hal_machw_time() + ((uint32_t)nxmac_next_tbtt_get() << 5);

    if (hal_machw_time_past(tbtt - 2000))
        return false;
    #endif

    // Go through all the enabled timer to check if one will expire too soon to sleep
    for (i = 0; i < HAL_TIMER_MAX; i++)
    {
        uint32_t timer_bit = CO_BIT(i);

        if ((timer_msk & timer_bit) && (hal_machw_time_past(nxmac_abs_timer_get(i) - 2000)))
        {
            ASSERT_ERR(!hal_machw_time_past(nxmac_abs_timer_get(i) + 5000));
            return false;
        }
    }

    return true;
}

void hal_assert_rec(void)
{
    GLOBAL_INT_DECLARATION();

    // Disable the interrupts
    GLOBAL_INT_DISABLE();

    // Display a trace message showing the error
    // Check if a recovery is already pending
    if (!(ke_evt_get() & KE_EVT_RESET_BIT))
    {
        os_printf("set KE_EVT_RESET_BIT\r\n");
		
        // Disable MAC HW interrupts
        hal_machw_disable_int();

        // Trigger the reset procedure
        ke_evt_set(KE_EVT_RESET_BIT);
    }

    // Restore the interrupts
    GLOBAL_INT_RESTORE();
}

#define HAL_FATAL_ERROR_RECOVER(cond)    \
    do {                         \
        if (!(cond)) {           \
            hal_assert_rec();    \
        }                        \
    } while(0)


/**
 ****************************************************************************************
 * @brief Absolute timer interrupt Handler

 * This function handles the HW timer interrupts. It checks the source of the interrupt
 * and execute the required functions.
 ****************************************************************************************
 */
void hal_machw_abs_timer_handler(void)
{
    uint32_t timer_pending;
	
	timer_pending = nxmac_timers_int_event_get();
	nxmac_timers_int_event_clear(timer_pending);

    if (timer_pending & HAL_KE_TIMER_BIT)
    {
        ke_evt_set(KE_EVT_KE_TIMER_BIT);
    }

    if (timer_pending & HAL_RX_TIMER_BIT)
    {
        rxl_timeout_int_handler();
    }

	if(timer_pending & HAL_AC0_TIMER_BIT)
	{
		os_printf("----------------------------------abs0_timeout_reset_agc\r\n");
	}
	
	if(timer_pending & HAL_AC1_TIMER_BIT)
	{
		os_printf("----------------------------------abs1_timeout_reset_agc\r\n");
	}

	if(timer_pending & HAL_AC2_TIMER_BIT)
	{
		os_printf("----------------------------------abs2_timeout_reset_agc\r\n");
	}
	
	if(timer_pending & HAL_AC3_TIMER_BIT)
	{
		os_printf("----------------------------------abs3_timeout_reset_agc\r\n");
	}

    #if NX_MM_TIMER
    if (timer_pending & HAL_MM_TIMER_BIT)
    {
        ke_evt_set(KE_EVT_MM_TIMER_BIT);    
	}
    #endif

    // Check for timeout errors
    ASSERT_REC(!(timer_pending & HAL_AC0_TIMER_BIT));
    HAL_FATAL_ERROR_RECOVER(!(timer_pending & HAL_AC1_TIMER_BIT));
    HAL_FATAL_ERROR_RECOVER(!(timer_pending & HAL_AC2_TIMER_BIT));
    HAL_FATAL_ERROR_RECOVER(!(timer_pending & HAL_AC3_TIMER_BIT));
    ASSERT_REC(!(timer_pending & HAL_BCN_TIMER_BIT));
    ASSERT_REC(!(timer_pending & HAL_IDLE_TIMER_BIT));
}

void hal_machw_gen_handler(void)
{
    uint32_t genirq_pending = nxmac_gen_int_status_get() & nxmac_gen_int_enable_get();
    
    nxmac_gen_int_ack_clear(genirq_pending);// clear all the interrupts

    if(genirq_pending & (~((1 << 0) | (1 << 1) | (1 << 2) | (1 << 3)| (1 << 6) | (1 << 18) | (1 << 19))))
    {
        fatal_prf("gen:%x\r\n", genirq_pending);

        if(genirq_pending & ((1 << 13) | (1 << 14)| (1 << 15)| (1 << 16)))
        {
            while(4545);
        }
    }
	    
    #if NX_BEACONING || (!NX_MULTI_ROLE) // Check for primary TBTT interrupts   
    if (genirq_pending & (NXMAC_IMP_PRI_TBTT_BIT | NXMAC_IMP_PRI_DTIM_BIT))
    {
        ke_evt_set(KE_EVT_PRIMARY_TBTT_BIT);
    }
    #endif

    #if NX_BEACONING // Check for secondary TBTT interrupts    
    if (genirq_pending & (NXMAC_IMP_SEC_TBTT_BIT | NXMAC_IMP_SEC_DTIM_BIT))
    {
        ke_evt_set(KE_EVT_SECONDARY_TBTT_BIT);
    }
    #endif

    if (genirq_pending & NXMAC_IDLE_INTERRUPT_BIT)
    {
        hal_machw_idle_irq_handler();
    }

    if (genirq_pending & NXMAC_ABS_GEN_TIMERS_BIT)
    {
        hal_machw_abs_timer_handler();
    }
    
#ifdef PHYIF_OVERFLOW_LOG
    if(genirq_pending & NXMAC_MAC_PHYIF_OVERFLOW_BIT)
    {
        fatal_prf("phyif_overflow\r\n");
    }
#endif

    if(genirq_pending & NXMAC_RX_DMA_EMPTY_BIT)
    {
        fatal_prf("rx_dma_descriptor_empty\r\n");
    }
    if(genirq_pending & NXMAC_RX_FIFO_OVER_FLOW_BIT)
    {
        fatal_prf("rx_fifo_over_flow\r\n");
    }
    if(genirq_pending & NXMAC_PT_ERROR_BIT)
    {
        fatal_prf("policy_table_error\r\n");
    }
    if(genirq_pending & NXMAC_AC_0_TX_DMA_DEAD_BIT)
    {
        fatal_prf("ac0_tx_dma_dead\r\n");
    }
    if(genirq_pending & NXMAC_AC_1_TX_DMA_DEAD_BIT)
    {
        fatal_prf("ac1_tx_dma_dead\r\n");
    }
    if(genirq_pending & NXMAC_AC_2_TX_DMA_DEAD_BIT)
    {
        fatal_prf("ac2_tx_dma_dead\r\n");
    }
    if(genirq_pending & NXMAC_AC_3_TX_DMA_DEAD_BIT)
    {
        fatal_prf("ac3_tx_dma_dead\r\n");
    }
    if(genirq_pending & NXMAC_BCN_TX_DMA_DEAD_BIT)
    {
        fatal_prf("beacon_tx_dma_dead\r\n");
    }
    if(genirq_pending & NXMAC_HW_ERR_BIT)
    {
        fatal_prf("nx_hw_error\r\n");
    }
    if(genirq_pending & NXMAC_MAC_PHYIF_UNDER_RUN_BIT)
    {
        fatal_prf("phy_interface_underrun\r\n");
    }
    if(genirq_pending & NXMAC_PHY_ERR_BIT)
    {
        fatal_prf("phy_error\r\n");
    }
    if(genirq_pending & NXMAC_RX_HEADER_DMA_DEAD_BIT)
    {
        fatal_prf("rx_header_dma_dead\r\n");
    }
    if(genirq_pending & NXMAC_RX_PAYLOAD_DMA_DEAD_BIT)
    {
        fatal_prf("rx_payload_dma_dead\r\n");
    }
	HAL_FATAL_ERROR_RECOVER(!(genirq_pending & NXMAC_RX_HEADER_DMA_DEAD_BIT));
	HAL_FATAL_ERROR_RECOVER(!(genirq_pending & NXMAC_RX_FIFO_OVER_FLOW_BIT));
}

/// @}  // end of group HAL_MACHW
