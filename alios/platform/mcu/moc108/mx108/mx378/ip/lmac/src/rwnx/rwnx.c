/**
 ****************************************************************************************
 *
 * @file rwnx.c
 *
 * @brief This file contains the implementation of the nX-MAC platform APIs.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup MACSW
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "version.h"
#include "ke_event.h"
#include "mm.h"
#include "hal_machw.h"
#include "rxl_cntrl.h"
#include "txl_cntrl.h"
#include "ps.h"
#include "rwnx.h"

#if (NX_TX_FRAME)
#include "vif_mgmt.h"
#endif //(NX_TX_FRAME)

#include "me.h"
#include "include.h"
#include "rw_pub.h"

/*
 * GLOBAL VARIABLES DECLARATIONS
 ****************************************************************************************
 */
#if NX_POWERSAVE
struct rwnx_env_tag rwnx_env;
#endif

RW_CONNECTOR_T g_rwnx_connector = {NULLPTR};

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief This function performs all the initializations of the MAC SW.
 *
 * It first initializes the heap, then the message queues and the events. Then if required
 * it initializes the trace.
 *
 ****************************************************************************************
 */
void rwnxl_init(void)
{
    #if NX_POWERSAVE
    rwnx_env.hw_in_doze = false;
    #endif
    
    rw_ieee80211_init();    
    me_init();// UMAC initialization;
    mm_init();// LMAC initialization;
    ke_init();// Mac kernel initialization
}

void rwnxl_register_connector(RW_CONNECTOR_T *intf)
{
    g_rwnx_connector = *intf;
}

/**
 ****************************************************************************************
 * @brief This function performs all the initializations of the MAC SW.
 *
 * It first initializes the heap, then the message queues and the events. Then if required
 * it initializes the trace.
 *
 ****************************************************************************************
 */
void rwnxl_reset_evt(int dummy)
{    
    GLOBAL_INT_DECLARATION();
    
    // Do the full reset procedure with interrupt disabled
    GLOBAL_INT_DISABLE();

    // Clear the reset event
    ke_evt_clear(KE_EVT_RESET_BIT);

    // Reset the MAC HW (this will reset the PHY too)
    hal_machw_reset();

    // Reset the RX path
    rxl_reset();

    // Reset the TX path
    txl_reset();

    // Reset the MM
    mm_reset();

    #if (NX_TX_FRAME)
    // Push postponned internal frames
    vif_mgmt_reset();
    #endif //(NX_TX_FRAME)

    // Restore the interrupts
    GLOBAL_INT_RESTORE();
}

#if NX_POWERSAVE
/**
 ****************************************************************************************
 * @brief This function performs the required checks prior to go to DOZE mode
 *
 * @return true if the CPU can be put in sleep, false otherwise.
 *
 ****************************************************************************************
 */
bool rwnxl_sleep(void)
{
    bool cpu_sleep = false;

    do
    {
        // Check if some kernel processing is ongoing
        if (ke_evt_get() != 0)
            break;

        // At least CPU sleep will be allowed
        cpu_sleep = true;

        // Check if PS allows sleeping
        if (!ps_sleep_check())
            break;

        // Check if TX path allows sleeping
        if (!txl_sleep_check())
            break;

        // Check the HW timers
        if (!hal_machw_sleep_check())
            break;
        
        // Store the current HW state to recover it at wake-up
        rwnx_env.prev_hw_state = nxmac_current_state_getf();

        // Ask HW to go to IDLE
        if (nxmac_current_state_getf() != HW_IDLE)
        {
            nxmac_next_state_setf(HW_IDLE);
            while (nxmac_status_idle_interrupt_getf() != 1);
            nxmac_gen_int_ack_clear(NXMAC_IDLE_INTERRUPT_BIT);
        }

        while(nxmac_current_state_getf() != HW_IDLE)
        {
            nxmac_next_state_setf(HW_IDLE);
        }
        
        nxmac_enable_lp_clk_switch_setf(0x01);
        nxmac_bcn_cntrl_1_set(1024);
        nxmac_doze_cntrl_1_set(0x02);

        // Ask HW to go to DOZE
        rwnx_env.hw_in_doze = true;
        nxmac_next_state_setf(HW_DOZE);     
    } while(0);

    return (cpu_sleep);
}

/**
 ****************************************************************************************
 * @brief This function performs the wake up from DOZE mode.
 *
 ****************************************************************************************
 */
void rwnxl_wakeup(void)
{
    if (rwnx_env.hw_in_doze)
    {
        // Start the Wake-Up from doze procedure
        nxmac_wake_up_from_doze_setf(1);
        rwnx_env.hw_in_doze = false;

        // Wait for idle interrupt
        while (nxmac_status_idle_interrupt_getf() != 1);
        nxmac_gen_int_ack_clear(NXMAC_IDLE_INTERRUPT_BIT);

        // Move back to the previous state
        if (rwnx_env.prev_hw_state != HW_IDLE)
        {
            nxmac_next_state_setf(rwnx_env.prev_hw_state);
            while (nxmac_current_state_getf() != rwnx_env.prev_hw_state);
        }

        // Wake-Up from doze procedure is done
        nxmac_wake_up_from_doze_setf(0);
    }
}
#endif

/// @}
