/**
 ****************************************************************************************
 * @file rx_swdesc.c
 *
 * @brief LMAC SW descriptor data structures allocation and release API implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 ****************************************************************************************
 */

// standard include
#include "co_int.h"

// For INVALID STA/TID IDX
#include "mac_common.h"
#include "hal_desc.h"
// Structures and prototypes
#include "rx_swdesc.h"
// for task
#include "ke_task.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct rx_swdesc rx_swdesc_tab[NX_RXDESC_CNT];

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void rx_swdesc_init(void)
{
    int i;
    
    // Initialize the DMA header descriptor
    for (i = 0; i < NX_RXDESC_CNT; i++)
    {
        // Link the HW descriptor with the corresponding SW one
        rx_swdesc_tab[i].dma_hdrdesc = &rx_dma_hdrdesc[i];
    }
}

/// @} // end of group RX_SWDESC
