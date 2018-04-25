/**
 ****************************************************************************************
 * @file mm_timer.c
 *
 * @brief MAC Management timer module function definitions.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup MM_TIMER
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// standard includes
#include "co_int.h"
#include "co_bool.h"

#include "mm_timer.h"
#include "ke_event.h"
#include "hal_machw.h"
#include "reg_mac_core.h"
#include "ke_timer.h"

#include "include.h"
#include "uart_pub.h"

#if NX_MM_TIMER
/*
 * DEFINES
 ****************************************************************************************
 */


/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct mm_timer_env_tag mm_timer_env;

/*
 * FUNCTION PROTOTYPES
 ****************************************************************************************
 */
void mm_timer_init(void)
{
    co_list_init(&mm_timer_env.prog);
}


/**
 ****************************************************************************************
 * @brief Set the HW timer with the first timer of the queue
 *
 * @param[in] timer
 ****************************************************************************************
 */
static void mm_timer_hw_set(struct mm_timer_tag *timer)
{    
    GLOBAL_INT_DECLARATION();
    
    GLOBAL_INT_DISABLE();
    if (timer)
    {
        uint32_t timer_msk;

        // set the abs timeout in HW
        nxmac_abs_timer_set(HAL_MM_TIMER, timer->time);

        // enable timer irq
        timer_msk = nxmac_timers_int_un_mask_get();
        if (!(timer_msk & HAL_MM_TIMER_BIT))
        {
            // if timer is not enabled, it is possible that the irq is raised
            // due to a spurious value, so ack it before
            nxmac_timers_int_event_clear(HAL_MM_TIMER_BIT);
            nxmac_timers_int_un_mask_set(timer_msk | HAL_MM_TIMER_BIT);
        }
    }
    else
    {
        nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() & ~HAL_MM_TIMER_BIT);
    }
    GLOBAL_INT_RESTORE();
}

/**
 ****************************************************************************************
 * @brief Compare timer absolute expiration time.
 *
 * @param[in] timerA Timer to compare.
 * @param[in] timerB Timer to compare.
 *
 * @return true if timerA will expire before timerB.
 ****************************************************************************************
 */
static bool cmp_abs_time(struct co_list_hdr const * timerA, struct co_list_hdr const * timerB)
{
    uint32_t timeA = ((struct mm_timer_tag*)timerA)->time;
    uint32_t timeB = ((struct mm_timer_tag*)timerB)->time;

    return (hal_machw_time_cmp(timeA, timeB));
}

void mm_timer_set(struct mm_timer_tag *timer, uint32_t value)
{
    bool hw_prog = false;
    GLOBAL_INT_DECLARATION();
    
    GLOBAL_INT_DISABLE();
    // Sanity check - Timeout value should not be in the past
    //ASSERT_ERR(!hal_machw_time_past(value));

    // check if timer is already present in the list
    if (co_list_pick(&mm_timer_env.prog) == (struct co_list_hdr*) timer)
    {
        // pop the timer from the list
        co_list_pop_front(&mm_timer_env.prog);
        // force the HW timer to be reprogrammed
        hw_prog = true;
    }
    else
    {
        // extract the timer from the list
        co_list_extract(&mm_timer_env.prog, &timer->list_hdr);
    }

    // update characteristics
    timer->time = value;

    // insert in sorted timer list
    co_list_insert(&mm_timer_env.prog,
                   (struct co_list_hdr*) timer,
                   cmp_abs_time);

    // check if HW timer set needed
    if (hw_prog || (co_list_pick(&mm_timer_env.prog) == (struct co_list_hdr*) timer))
    {
        mm_timer_hw_set((struct mm_timer_tag *)co_list_pick(&mm_timer_env.prog));
    }

    // Check that the timer did not expire before HW prog
    if (hal_machw_time_past(value))
    {
        // Timer already expired, so schedule the timer event immediately
        ke_evt_set(KE_EVT_MM_TIMER_BIT);
    }
	GLOBAL_INT_RESTORE();
}


void mm_timer_clear(struct mm_timer_tag *timer)
{
    GLOBAL_INT_DECLARATION();
    
    GLOBAL_INT_DISABLE();
    // Check if timer is first of the list
    if (co_list_pick(&mm_timer_env.prog) == (struct co_list_hdr*) timer)
    {
        // Pop it
        co_list_pop_front(&mm_timer_env.prog);

        // And reprogram the HW timer
        mm_timer_hw_set((struct mm_timer_tag *)co_list_pick(&mm_timer_env.prog));
    }
    else
    {
        // Extract the timer from the list
        co_list_extract(&mm_timer_env.prog, &timer->list_hdr);
    }
	GLOBAL_INT_RESTORE();
}

/**
 ****************************************************************************************
 * @brief Schedule the next timer(s).
 *
 * This function pops the first timer from the timer queue and notifies the appropriate
 * task by sending a kernel message. If the timer is periodic, it is set again;
 * if it is one-shot, the timer is freed. The function checks also the next timers
 * and process them if they have expired or are about to expire.
 ****************************************************************************************
 */
void mm_timer_schedule(int dummy)
{
    struct mm_timer_tag *timer;
    GLOBAL_INT_DECLARATION();
        
	ke_evt_clear(KE_EVT_MM_TIMER_BIT);
	
    for(;;)
    {        
        // check the next timer
        GLOBAL_INT_DISABLE();
        timer = (struct mm_timer_tag *)co_list_pick(&mm_timer_env.prog);
		GLOBAL_INT_RESTORE();
		if (!timer)
        {
            // no more timers, disable HW irq and leave
            mm_timer_hw_set(NULL);
            break;
        }

        if (!hal_machw_time_past(timer->time - 50))
        {
            // timer will expire in more than 32us, configure the HW
            mm_timer_hw_set(timer);
			
            // in most case, we will break here. However, if the timer expiration
            // time has just passed, may be the HW was set too late (due to an IRQ)
            // so we do not exit the loop to process it.
            if (!hal_machw_time_past(timer->time))
            {
                break;
            }
        }

        // at this point, the next timer in the queue has expired or is about to -> pop it
        co_list_pop_front(&mm_timer_env.prog);
		
        // notify the caller - It has to be done after popping the timer from the queue
        // as the callback may set it again
        timer->cb(timer->env);
    }
}

#endif

/// @} end of group

