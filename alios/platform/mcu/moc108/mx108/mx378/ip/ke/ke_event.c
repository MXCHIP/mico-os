/**
 ****************************************************************************************
 *
 * @file ke_event.c
 *
 * @brief This file contains the event handling primitives.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup KE_EVT
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h"
#include "arm_arch.h"

#include "sdio_pub.h"
#include "sdio_intf_pub.h"

#include <stddef.h>
#include "co_int.h"
#include "co_bool.h"

#include "rwnx_config.h"
#include "arch.h"

#include "ke_event.h"
#include "ke_env.h"
#include "mem_pub.h"
#include "mm_timer.h"
#include "ke_task.h"
#include "ke_timer.h"
#include "hal_dma.h"
#include "txl_cntrl.h"
#include "rxl_cntrl.h"
#include "rxu_cntrl.h"

#if NX_TX_FRAME
#include "txl_frame.h"
#endif

#include "txl_cfm.h"
#include "mm.h"
#include "rwnx.h"
#include "ps.h"

#include "hostapd_intf_pub.h"

#if CFG_TX_EVM_TEST
#include "tx_evm_pub.h"
#endif
#include "uart_pub.h"

const uint32_t ac_payload_evt_bit[NX_TXQ_CNT] =
{     
    KE_EVT_TXL_PAYLOAD_AC0_BIT,
    KE_EVT_TXL_PAYLOAD_AC1_BIT,
    KE_EVT_TXL_PAYLOAD_AC2_BIT,
    KE_EVT_TXL_PAYLOAD_AC3_BIT,
    
    #if NX_BEACONING
    KE_EVT_TXL_PAYLOAD_BCN_BIT
    #endif
};

uint32_t core_evt_mask = 0xffffffff; // 0x1f000000

static const struct ke_evt_tag ke_evt_hdlr[32] =
{
	{&rwnxl_reset_evt, 0},       // [KE_EVT_RESET         ]                                                                            
	
	#if NX_MM_TIMER
	{&mm_timer_schedule, 0},     // [KE_EVT_MM_TIMER      ]                                                                            
	#endif
	
	{&ke_timer_schedule, 0},     // [KE_EVT_KE_TIMER      ]   
	
#if NX_BEACONING
	{&txl_payload_handle, AC_BCN},   // [KE_EVT_IPC_EMB_TXDESC_BCN]                                                                        
#endif
	
	{&txl_payload_handle, AC_VO},    // [KE_EVT_IPC_EMB_TXDESC_AC3]                                                                        
	{&txl_payload_handle, AC_VI},    // [KE_EVT_IPC_EMB_TXDESC_AC2]                                                                        
	{&txl_payload_handle, AC_BE},    // [KE_EVT_IPC_EMB_TXDESC_AC1]                                                                        
	{&txl_payload_handle, AC_BK},    // [KE_EVT_IPC_EMB_TXDESC_AC0]   
	
	{&ke_task_schedule, 0},      // [KE_EVT_KE_MESSAGE    ]                                                                            
	{&mm_hw_idle_evt, 0},        // [KE_EVT_HW_IDLE       ]                                                                            
	
	#if NX_BEACONING || (!NX_MULTI_ROLE)
	{&mm_tbtt_evt, 0},           // [KE_EVT_PRIMARY_TBTT  ]                                                                            
	#endif
	
	#if NX_BEACONING
	{&mm_tbtt_evt, 0},           // [KE_EVT_SECONDARY_TBTT]                                                                            
	#endif
	
	{&rxu_cntrl_evt, 0},         // [KE_EVT_RXUREADY      ]
	
	#if NX_TX_FRAME
	{&txl_frame_evt, 0},         // [KE_EVT_TXFRAME_CFM   ]                                                                            
	#endif
	
	#if NX_BEACONING
	{&txl_cfm_evt, AC_BCN},      // [KE_EVT_TXCFM_BCN     ]                                                                            
	#endif
	
	{&txl_cfm_evt, AC_VO},       // [KE_EVT_TXCFM_AC3     ]                                                                            
	{&txl_cfm_evt, AC_VI},       // [KE_EVT_TXCFM_AC2     ]                                                                            
	{&txl_cfm_evt, AC_BE},       // [KE_EVT_TXCFM_AC1     ]                                                                            
	{&txl_cfm_evt, AC_BK},       // [KE_EVT_TXCFM_AC0     ]                                                                            

    #if CFG_SDIO
	{&sdio_emb_rxed_evt, 0},  
	#endif
	
    #if CFG_SDIO_TRANS
	{&sdio_trans_evt, 0},  
    #endif
    
	#if NX_GP_DMA
	{&hal_dma_evt, DMA_DL},      // [KE_EVT_GP_DMA_DL     ]                                                                            
	#endif

	#if CFG_TX_EVM_TEST
	{&evm_via_mac_evt, 0},
	#endif
};

uint32_t ke_get_ac_payload_bit(uint32_t ac)
{
    return ac_payload_evt_bit[ac];
}

/**
 ****************************************************************************************
 * @brief Set events
 *
 * This primitive sets one or more events in the event field variable. It will trigger
 * the call to the corresponding event handlers in the next scheduling call.
 *
 * @param[in]  event       Events that have to be set (bit field).
 *
 ****************************************************************************************
 */
void ke_evt_set(evt_field_t const event)
{    
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	if(!(ke_env.evt_field & event))
	{
		ke_env.evt_field |= event;
		
		if(platform_is_in_interrupt_context())
		{
			bmsg_null_sender();
		}
		
		if(event  & (~core_evt_mask))
		{
			app_set_sema();
		}
	}
	GLOBAL_INT_RESTORE();
}

/**
 ****************************************************************************************
 * @brief Clear events
 *
 * This primitive clears one or more events in the event field variable.
 *
 * @param[in]  event       Events that have to be cleared (bit field).
 *
 ****************************************************************************************
 */
void ke_evt_clear(evt_field_t const event)
{    
    GLOBAL_INT_DECLARATION();
    
    GLOBAL_INT_DISABLE();
    ke_env.evt_field &= ~event;
    GLOBAL_INT_RESTORE();
}


/**
 ****************************************************************************************
 * @brief Event scheduler entry point.
 *
 * This primitive has to be called in the background loop in order to execute the event
 * handlers for the event that are set.
 *
 ****************************************************************************************
 */
void ke_evt_schedule(void)
{
    uint32_t field,event;

	field = ke_env.evt_field;
    while (field) // Compiler is assumed to optimize with loop inversion
    {
        // Find highest priority event set
        event = co_clz(field);

        // Sanity check
        ASSERT_ERR((event < KE_EVT_MAX) && ke_evt_hdlr[event].func);

        // Execute corresponding handler
        (ke_evt_hdlr[event].func)(ke_evt_hdlr[event].param);

        // Update the volatile value
        field = ke_env.evt_field;
    }
}

void ke_evt_mask_schedule(uint32_t mask)
{
	uint32_t field,event;

	field = (ke_env.evt_field & mask);
	while (field) // Compiler is assumed to optimize with loop inversion
	{
		// Find highest priority event set
		event = co_clz(field);

		// Sanity check
		ASSERT_ERR((event < KE_EVT_MAX) && ke_evt_hdlr[event].func);

		// Execute corresponding handler
		(ke_evt_hdlr[event].func)(ke_evt_hdlr[event].param);

		// Update the volatile value
		field = (ke_env.evt_field & mask);
	}
}

void ke_evt_core_scheduler(void)
{
	ke_evt_mask_schedule(core_evt_mask);
}

void ke_evt_none_core_scheduler(void)
{
	ke_evt_mask_schedule(~core_evt_mask);
}

/**
 ****************************************************************************************
 * @brief This function performs all the initializations of the kernel.
 *
 * It initializes first the heap, then the message queues and the events. Then if required
 * it initializes the trace.
 *
 ****************************************************************************************
 */
void ke_init(void)
{
    ke_env.queue_saved.first = NULL;
    ke_env.queue_saved.last  = NULL;
    ke_env.queue_sent.first  = NULL;
    ke_env.queue_sent.last   = NULL;
    ke_env.queue_timer.first = NULL;
    ke_env.queue_timer.last  = NULL;

    ke_evt_clear(0xFFFFFFFF);
}

/**
 ****************************************************************************************
 * @brief This function flushes all messages, timers and events currently pending in the
 * kernel.
 */
void ke_flush(void)
{
	struct ke_msg *msg;
	struct ke_timer *timer;
	
    while(1)
    {
        msg = (struct ke_msg*) ke_queue_pop(&ke_env.queue_sent);
        if(msg == NULL)
            break;
        ke_msg_free(msg);
    }

    while(1)
    {
        msg = (struct ke_msg*) ke_queue_pop(&ke_env.queue_saved);
        if(msg == NULL)
            break;
        ke_msg_free(msg);
    }

    while(1)
    {
        timer = (struct ke_timer*) ke_queue_pop(&ke_env.queue_timer);
        if(timer == NULL)
            break;
        os_free(timer);
    }

    ke_evt_clear(0xFFFFFFFF);
}
// eof

