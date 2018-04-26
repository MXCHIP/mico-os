/**
 ****************************************************************************************
 *
 * @file ke_timer.c
 *
 * @brief This file contains the scheduler primitives called to create or delete
 * a task. It contains also the scheduler itself.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup KETIMER
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stddef.h>
#include "co_int.h"
#include "co_bool.h"
#include "arch.h"

#include "ke_config.h"
#include "mem_pub.h"
#include "ke_env.h"
#include "ke_event.h"
#include "ke_timer.h"
#include "ke_task.h"
#include "hal_machw.h"


/**
 ****************************************************************************************
 * @brief Set the HW timer with the first timer of the queue
 *
 * @param[in] timer
 ****************************************************************************************
 */
static void ke_timer_hw_set(struct ke_timer *timer)
{    
    GLOBAL_INT_DECLARATION();
    
    GLOBAL_INT_DISABLE();
    if (timer)
    {
        uint32_t timer_msk;

        // set the abs timeout in HW
        nxmac_abs_timer_set(HAL_KE_TIMER, timer->time);

        // enable timer irq
        timer_msk = nxmac_timers_int_un_mask_get();
        if (!(timer_msk & HAL_KE_TIMER_BIT))
        {
            // if timer is not enabled, it is possible that the irq is raised
            // due to a spurious value, so ack it before
            nxmac_timers_int_event_clear(HAL_KE_TIMER_BIT);
            nxmac_timers_int_un_mask_set(timer_msk | HAL_KE_TIMER_BIT);
        }
    }
    else
    {
        // disable timer irq
        nxmac_timers_int_un_mask_set(nxmac_timers_int_un_mask_get() & ~HAL_KE_TIMER_BIT);
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
    uint32_t timeA = ((struct ke_timer*)timerA)->time;
    uint32_t timeB = ((struct ke_timer*)timerB)->time;

    return (((uint32_t)(timeA - timeB)) > KE_TIMER_DELAY_MAX);
}

/**
 ****************************************************************************************
 * @brief Compare timer and task IDs callback
 *
 * @param[in] timer Pointer to the timer element
 * @param[in] timer_task Integer formed with the task_id and timer_id
 *
 * @return true if the timer matches with the timer_id and task_id passed in timer_task
 ****************************************************************************************
 */
static bool cmp_timer_id(struct co_list_hdr const * timer, uint32_t timer_task)
{
    // trick to pack 2 u16 in u32
    ke_msg_id_t timer_id = timer_task >> 16;
    ke_task_id_t task_id = timer_task & 0xFFFF;

    // insert the timer just before the first one older
    return (timer_id == ((struct ke_timer*)timer)->id)
        && (task_id == ((struct ke_timer*)timer)->task);
}


/**
 ****************************************************************************************
 * @brief Set a timer.
 *
 * The function first cancel the timer if it is already existing, then
 * it creates a new one. The timer can be one-shot or periodic, i.e. it
 * will be automatically set again after each trigger.
 *
 * When the timer expires, a message is sent to the task provided as
 * argument, with the timer id as message id.
 *
 * The timer is programmed in microseconds but is not accurate to the microsecond, due to
 * the kernel message propagation delay and the configuration time.
 *
 * @param[in] timer_id      Timer identifier (message identifier type).
 * @param[in] task_id       Task identifier which will be notified
 * @param[in] delay         Delay in time units.
 ****************************************************************************************
 */
void ke_timer_set(ke_msg_id_t const timer_id,
                  ke_task_id_t const task_id,
                  uint32_t const delay)
{
    uint32_t abs_time;
    bool hw_prog = false;
    struct ke_timer *first;
    struct ke_timer *timer;

    // Sanity checks
    ASSERT_ERR(delay > 0); // Delay should not be zero
    ASSERT_ERR(delay < KE_TIMER_DELAY_MAX); // Delay should not be more than maximum allowed

    // Check if requested timer is first of the list of pending timer
    first = (struct ke_timer *) ke_env.queue_timer.first;
    if ((first->id == timer_id) && (first->task == task_id))
        // Indicate that the HW timer will have to be reprogrammed
        hw_prog = true;

    // Extract the timer from the list if required
    timer = (struct ke_timer*)
        ke_queue_extract(&ke_env.queue_timer, cmp_timer_id, (uint32_t)timer_id << 16 | task_id);

    if (timer == NULL)
    {
        // Create new one
        timer = (struct ke_timer*) os_malloc(sizeof(struct ke_timer));
        ASSERT_ERR(timer);
        timer->id = timer_id;
        timer->task = task_id;
    }

    // update characteristics
    abs_time = ke_time() + delay;
    timer->time = abs_time;

    // insert in sorted timer list
    co_list_insert(&ke_env.queue_timer,
                   (struct co_list_hdr*) timer,
                   cmp_abs_time);

    // check if HW timer set needed
    if (hw_prog || (ke_env.queue_timer.first == (struct co_list_hdr*) timer))
    {
        ke_timer_hw_set((struct ke_timer *)ke_env.queue_timer.first);
    }

    // Check that the timer did not expire before HW prog
    if (ke_time_past(abs_time))
    {
        // Timer already expired, so schedule the timer event immediately
        ke_evt_set(KE_EVT_KE_TIMER_BIT);
    }
}


/**
 ****************************************************************************************
 * @brief Remove an registered timer.
 *
 * This function search for the timer identified by its id and its task id.
 * If found it is stopped and freed, otherwise an error message is returned.
 *
 * @param[in] timer_id  Timer identifier.
 * @param[in] task_id   Task identifier.
 ****************************************************************************************
 */
void ke_timer_clear(ke_msg_id_t const timer_id,
                           ke_task_id_t const task_id)
{
    struct ke_timer *first;
    struct ke_timer *timer = (struct ke_timer *) ke_env.queue_timer.first;

    if (ke_env.queue_timer.first != NULL)
    {
        if ((timer->id == timer_id) && (timer->task == task_id))
        {
            // timer found and first to expire! pop it
            ke_queue_pop(&ke_env.queue_timer);

            first = (struct ke_timer *) ke_env.queue_timer.first;

            // and set the following timer HW if any
            ke_timer_hw_set(first);

            // Check that the timer did not expire before HW prog
            ASSERT_ERR(!first || !ke_time_past(first->time));
        }
        else
        {
            timer = (struct ke_timer *)
                    ke_queue_extract(&ke_env.queue_timer, cmp_timer_id,
                            (uint32_t)timer_id << 16 | task_id);
        }

        if (timer != NULL)
        {
            os_free(timer);
        }
    }
}


/**
 ****************************************************************************************
 * @brief Schedule the next timer(s).
 *
 * This function pops the first timer from the timer queue and notifies the appropriate
 * task by sending a kernel message. If the timer is periodic, it is set again;
 * if it is one-shot, the timer is freed. The function checks also the next timers
 * and process them if they have expired or are about to expire.
 *
 * @param[in] dummy Parameter not used but required to follow the kernel event callback
 * format
 ****************************************************************************************
 */
void ke_timer_schedule(int dummy)
{
    struct ke_timer *timer;

    for(;;)
    {
        ke_evt_clear(KE_EVT_KE_TIMER_BIT);

        // check the next timer
        timer = (struct ke_timer*) ke_env.queue_timer.first;
        if (!timer)
        {
            // no more timers, disable HW irq and leave
            ke_timer_hw_set(NULL);
            break;
        }

        if (!ke_time_past(timer->time - 50))
        {
            // timer will expire in more than 32us, configure the HW
            ke_timer_hw_set(timer);

            // in most case, we will break here. However, if the timer expiration
            // time has just passed, may be the HW was set too late (due to an IRQ)
            // so we do not exit the loop to process it.
            if (!ke_time_past(timer->time))
                break;
        }

        // at this point, the next timer in the queue has expired or is about to -> pop it
        timer = (struct ke_timer*) ke_queue_pop(&ke_env.queue_timer);

        // notify the task
        ke_msg_send_basic(timer->id, timer->task, TASK_NONE);

        // free the memory allocated for the timer
        os_free(timer);
    }
}
/**
 ****************************************************************************************
 * @brief Checks if a requested timer is active.
 *
 * This function goes through the list of timers until it finds a timer matching the
 * timer_id and task_id.
 *
 * @param[in] timer_id Identifier of the timer
 * @param[in] task_id Task indentifier
 *
 * @return true if a timer in the list matches the passed timer_id and task_id, false
 * otherwise.
 ****************************************************************************************
 */
bool ke_timer_active(ke_msg_id_t const timer_id, ke_task_id_t const task_id)
{
    struct ke_timer *timer;
    bool result;

    // check the next timer
    timer = (struct ke_timer*) ke_env.queue_timer.first;

    /* scan the timer queue to look for a message element with the same id and destination*/
    while (timer != NULL)
    {
        if ((timer->id == timer_id) &&
            (timer->task == task_id) )
        {
            /* Element has been found                                                   */
            break;
        }

        /* Check  next timer                                                            */
        timer = timer->next;
    }

    /* If the element has been found                                                    */
    if (timer != NULL)
        result = true;
    else
        result = false;

    return(result);
}

///@} TIMER
