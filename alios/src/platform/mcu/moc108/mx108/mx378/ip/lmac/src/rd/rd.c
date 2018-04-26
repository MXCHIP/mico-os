/**
 ****************************************************************************************
 * @file rd.c
 *
 * @brief Implementation of radar detection driver.
 *
 * Copyright (C) RivieraWaves 2011-2016
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup RD
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_int.h"
// For bool
#include "co_bool.h"
// For NULL
#include <string.h>
// For INLINE
#include "compiler.h"
// for Radar pulse interface
#include "phy.h"
// RX control inclusions
#include "rd.h"
// GP DMA descriptor allocation
#include "hal_dma.h"
// IPC host buffer allocation
// For debug functions
// For HW/CPU buffer pointer manipulations
#include "co_utils.h"

#if NX_RADAR_DETECT

/*
 * MACROS
 ****************************************************************************************
 */

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct rd_env_tag rd_env;


/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief RD platform DMA handler.
 ****************************************************************************************
 */
static void rd_dma_handler(void *env, int dma_type)
{
    // Retrieve radar event from env
    struct radar_event_desc *desc = (struct radar_event_desc *)env;

    // Push it back to the free list
    co_list_push_back(&rd_env.event_free_list, &desc->list_hdr);

    // call the LMAC-UMAC interface function to handle the frame
    ipc_emb_radar_event_ind();
}

void rd_init(void)
{
    int i;

    /// Initialize radar event lists
    co_list_init(&rd_env.event_free_list);

    for (i = 0; i < RADAR_EVENT_MAX; i++)
    {
        struct radar_event_desc *desc = &radar_event_desc_array[i];

        // Initialize descriptors
        desc->dma_desc.src = CPU2HW(&desc->pulse_array);
        desc->dma_desc.length = sizeof_b(desc->pulse_array);

        desc->gp_dma_desc.dma_desc = &desc->dma_desc;
        desc->gp_dma_desc.cb = rd_dma_handler;
        desc->gp_dma_desc.env = desc;

        co_list_push_back(&rd_env.event_free_list, &desc->list_hdr);
    }
}

void rd_event_ind(int rd_idx)
{
    struct radar_event_desc *desc;
    uint32_t hostbuf;
    int i;

    // Loop until no more radar pulses are available in the PHY
    while (phy_has_radar_pulse(rd_idx))
    {
        // Get a free event descriptor
        desc = (struct radar_event_desc *)co_list_pop_front(&rd_env.event_free_list);
        if (desc == NULL)
            break;

        // Get a host buffer for this event
        hostbuf = 0;
        if (hostbuf == 0)
        {
            // Push back the descriptor to the free list
            co_list_push_back(&rd_env.event_free_list, &desc->list_hdr);
            break;
        }

        // Initialize the descriptor
        desc->pulse_array.cnt = 0;
        desc->pulse_array.idx = rd_idx;
        desc->dma_desc.dest = hostbuf;

        // Call the PHY driver to get the pulses
        for (i = 0; i < RADAR_PULSE_MAX; i++)
        {
            if (!phy_get_radar_pulse(rd_idx, &desc->pulse_array.pulse[i]))
                break;
            desc->pulse_array.cnt++;
        }

        // Sanity check - we shall not send an empty event to the host
        ASSERT_ERR(desc->pulse_array.cnt != 0);

        // Program the DMA transfer to the host
        hal_dma_push(&desc->gp_dma_desc, DMA_UL);
    }
}

#endif

/// @}
