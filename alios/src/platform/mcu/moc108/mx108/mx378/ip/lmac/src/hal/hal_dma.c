/**
 ****************************************************************************************
 *
 * @file hal_dma.c
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 * @brief General purpose DMA functions
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup CO_DMA
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// for mem* functions
#include <string.h>

// for assertions
#include "arch.h"

#include "ke_event.h"

#include "hal_dma.h"

#include "include.h"
#include "arm_arch.h"
#include "mem_pub.h"


#if NX_GP_DMA
const uint8_t dma2chan[DMA_MAX] =
{
    IPC_DMA_CHANNEL_DATA_TX,
    IPC_DMA_CHANNEL_CTRL_RX,
};


/*
 * GLOBAL VARIABLES DEFINITIONS
 ****************************************************************************************
 */
struct hal_dma_env_tag hal_dma_env;

#if (HAL_DMA_POOL)
/// Pool of General Purpose DMA descriptors
struct hal_dma_desc_tag hal_dma_desc_pool[HAL_DMA_DESC_POOL_SIZE];
#endif //(HAL_DMA_POOL)

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
#if (HAL_DMA_POOL)
__INLINE bool hal_dma_is_in_pool(struct hal_dma_desc_tag *desc)
{
    return((desc >= &hal_dma_desc_pool[0]) &&
           (desc <= &hal_dma_desc_pool[HAL_DMA_DESC_POOL_SIZE - 1]));
}
#endif //(HAL_DMA_POOL)

void hal_dma_init(void)
{
    int i;

    // Initialize DMA lists
    for (i = 0; i < DMA_MAX; i++)
    {
        co_list_init(&hal_dma_env.prog[i]);
    }
    #if (HAL_DMA_POOL)
    // Fully reset content of the GP DMA descriptors
    memset(&hal_dma_desc_pool[0], 0, HAL_DMA_DESC_POOL_SIZE * sizeof(struct hal_dma_desc_tag));

    // Initialize pool of GP DMA descriptors
    co_list_init(&hal_dma_env.free_gp_dma_descs);

    for (i = 0; i < HAL_DMA_DESC_POOL_SIZE; i++)
    {
        // Insert the descriptor in the list of free descriptors
        co_list_push_back(&hal_dma_env.free_gp_dma_descs, &hal_dma_desc_pool[i].hdr);
    }
    #endif //(HAL_DMA_POOL)
}

void hal_dma_push(struct hal_dma_desc_tag *desc, int type)
{
    struct dma_desc *dma_desc = desc->dma_desc;
    struct co_list *list = &hal_dma_env.prog[type];

    // Check if a callback function is programmed
    if ((desc->cb != NULL)
        #if (HAL_DMA_POOL)
        || hal_dma_is_in_pool(desc)
        #endif //(HAL_DMA_POOL)
       )
    {
        // Enable interrupt and LLI counter increment
        dma_desc->ctrl = 0;

        // Push the descriptor to the list
        co_list_push_back(list, &desc->hdr);
    }
    else
    {
        // Disable interrupt and LLI counter increment
        dma_desc->ctrl = 0;
    }

    // Push the DMA descriptor to the DMA engine
    if(dma_desc->length && dma_desc->dest)
    {
	    os_memcpy((void *)dma_desc->dest, (void *)dma_desc->src, dma_desc->length);
    }
	
	if((DMA_DL == type) && (desc->cb))
	{
		hal_dma_dl_irq();
	}
}

void hal_dma_evt(int dma_queue)
{
    struct hal_dma_desc_tag *desc = (struct hal_dma_desc_tag *)
                                  co_list_pop_front(&hal_dma_env.prog[dma_queue]);
	
    // Trigger the associated kernel event
    ke_evt_clear(KE_EVT_GP_DMA_DL_BIT);

	if(desc && desc->cb)
	{		
        desc->cb(desc->env, dma_queue);
	}

}

void hal_dma_dl_irq(void)
{
    ke_evt_set(KE_EVT_GP_DMA_DL_BIT);
}

#if (HAL_DMA_POOL)
struct hal_dma_desc_tag *hal_dma_get_desc(void)
{
    return ((struct hal_dma_desc_tag *)co_list_pop_front(&hal_dma_env.free_gp_dma_descs));
}

void hal_dma_release_desc(struct hal_dma_desc_tag *p_gp_dma_desc)
{
    co_list_push_back(&hal_dma_env.free_gp_dma_descs, &p_gp_dma_desc->hdr);
}
#endif //(HAL_DMA_POOL)
#endif
/// @}
