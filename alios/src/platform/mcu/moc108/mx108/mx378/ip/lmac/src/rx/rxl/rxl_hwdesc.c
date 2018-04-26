/**
 ****************************************************************************************
 * @file rxl_hwdesc.c
 *
 * @brief Implementation of the API function used to initialize the pools.
 *
 * Copyright (C) RivieraWaves 2011-2016
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup RX_HWDESC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "rxl_hwdesc.h"
#include "rx_swdesc.h"
#include "rxl_cntrl.h"
#include "co_utils.h"

#include "include.h"
#include "ll.h"
#include "uart_pub.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct rxl_hwdesc_env_tag rx_hwdesc_env;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void rxl_hwdesc_init(void)
{
    uint32_t i;

    // Initialize the DMA header descriptor
    for (i = 0; i < NX_RXDESC_CNT; i++)
    {
        // No data buffer directly attached to the RHD
        rx_dma_hdrdesc[i].hd.datastartptr = 0;
        rx_dma_hdrdesc[i].hd.dataendptr = 0;

        // Fill in the Upattern
        rx_dma_hdrdesc[i].hd.upatternrx = RX_HEADER_DESC_PATTERN;

        // clear the MPDU status info
        rx_dma_hdrdesc[i].hd.statinfo = 0;

        // clear the control field
        rx_dma_hdrdesc[i].hd.headerctrlinfo = 0;

        // Chain to the next RHD
        rx_dma_hdrdesc[i].hd.next = CPU2HW(&rx_dma_hdrdesc[i + 1].hd);

        // reset the first payload descriptor pointer
        rx_dma_hdrdesc[i].hd.first_pbd_ptr = 0;

        // Link the HW descriptor with the corresponding SW one
        rx_dma_hdrdesc[i].hd.swdesc = &rx_swdesc_tab[i];

        // clear the frame length
        rx_dma_hdrdesc[i].hd.frmlen = 0;
        rx_dma_hdrdesc[i].hd.ampdu_stat_info = 0;

        // Prepare DMA descriptor for the RX vectors DMA transfer
        rx_dma_hdrdesc[i].dma_desc.src = CPU2HW(&rx_dma_hdrdesc[i].hd.frmlen);
        rx_dma_hdrdesc[i].dma_desc.length = RXL_HEADER_INFO_LEN;
    }

    // Reset the next pointer of the last RHD
    rx_dma_hdrdesc[NX_RXDESC_CNT - 1].hd.next = 0;

    // write the buffer descriptor into the receive header head pointer register
    nxmac_rx_header_head_ptr_set(CPU2HW(&rx_dma_hdrdesc[1].hd));

    // set new head bit in DMA control register
    nxmac_dma_cntrl_set(NXMAC_RX_HEADER_NEW_HEAD_BIT);

    // Initialize the buffer descriptors
    // sanity check
    ASSERT_ERR((CPU2HW(&rx_payload_desc[0].buffer) & WORD_ALIGN) == 0);

    for (i = 0 ; i < NX_RX_PAYLOAD_DESC_CNT ; i++)
    {
        struct rx_payloaddesc *desc = &rx_payload_desc[i];
        struct rx_pbd *pbd = &desc->pbd;

        pbd->next = CPU2HW(&rx_payload_desc[i+1].pbd);

        // Update the upattern
        pbd->upattern = RX_PAYLOAD_DESC_PATTERN;

        // Clear the bufstatinfo
        pbd->bufstatinfo = 0;

        //   (end pointer is inclusive, hence the -1)
        pbd->datastartptr = CPU2HW(&(desc->buffer[0]));
        pbd->dataendptr   = pbd->datastartptr + NX_RX_PAYLOAD_LEN - 1;
        
        // Prepare src pointer of the IPC DMA descriptor
        desc->dma_desc.src = pbd->datastartptr;
    }

    // Reset the next pointer on the last one
    rx_payload_desc[NX_RX_PAYLOAD_DESC_CNT - 1].pbd.next = 0;

    // program new buffer desc header in list for the MAC HW
    nxmac_rx_payload_head_ptr_set(CPU2HW(&rx_payload_desc[1].pbd));

    // set the new head bit in the DMA control register
    nxmac_dma_cntrl_set(NXMAC_RX_PAYLOAD_NEW_HEAD_BIT);

    // initialize the first element of the DMA header pool
    rxl_cntrl_env.free = &rx_dma_hdrdesc[0];
    rxl_cntrl_env.first = &rx_dma_hdrdesc[1];
    rxl_cntrl_env.last = &rx_dma_hdrdesc[NX_RXDESC_CNT - 1];

    // Record the last buffer desc
    rx_hwdesc_env.free  = &rx_payload_desc[0].pbd;
    rx_hwdesc_env.first = &rx_payload_desc[1].pbd;
    rx_hwdesc_env.last  = &rx_payload_desc[NX_RX_PAYLOAD_DESC_CNT - 1].pbd;
}

void rxl_hd_append(struct rx_dmadesc *desc)
{
    struct rx_dmadesc *free_desc;
	GLOBAL_INT_DECLARATION();

    // sanity check: function can not be called with NULL
    ASSERT_ERR(desc != NULL);

    // check if the HW is still pointing to the spare descriptor
    GLOBAL_INT_DISABLE();
    if (HW2CPU(nxmac_debug_rx_hdr_c_ptr_get()) == rxl_cntrl_env.free)
    {
        // The HW is still pointing to the spare descriptor, therefore we cannot release
        // it now. Instead we release the current descriptor.
        free_desc = desc;
    }
    else
    {
        // Otherwise we free the spare descriptor
        free_desc = rxl_cntrl_env.free;

        // Save the new free descriptor
        rxl_cntrl_env.free = desc;
    }

    // clear the MPDU status info
    free_desc->hd.statinfo = 0;

    // reset the next pointer
    free_desc->hd.next = 0;

    // reset the first payload descriptor pointer
    free_desc->hd.first_pbd_ptr = 0;

    // reset the frame length field
    free_desc->hd.frmlen = 0;
    free_desc->hd.ampdu_stat_info = 0;

    // Link the RHD to the current list and trigger a newTail
    rxl_cntrl_env.last->hd.next = CPU2HW(&(free_desc->hd));
    nxmac_dma_cntrl_set(NXMAC_RX_HEADER_NEW_TAIL_BIT);

    // record the last pointer
    rxl_cntrl_env.last = free_desc;

    // check if we reached the end of the RX header descriptor list
    if (rxl_cntrl_env.first == NULL)
    {
        rxl_cntrl_env.first = free_desc;
    }
	GLOBAL_INT_RESTORE();
}

void rxl_pd_append(struct rx_pbd *first, struct rx_pbd* last, struct rx_pbd* spare)
{
	GLOBAL_INT_DECLARATION();
    // sanity check: the function can not be called with NULL
	if(spare == NULL)
	{
		os_printf("rxl_pd_append_null\r\n");
		return; // wangzhilei test bug maybe
	}
	
    ASSERT_ERR(spare != NULL);

    // check if the HW is still pointing to the spare descriptor
    GLOBAL_INT_DISABLE();
    if (HW2CPU(nxmac_debug_rx_pay_c_ptr_get()) == rx_hwdesc_env.free)
    {
        // The HW is still pointing to the spare descriptor, therefore we cannot release
        // it now. Instead we release the current descriptor.
        // Add the free element at the end of the list
        if (last == NULL)
        {
            // No element in the list
            first = spare;
        }
        last = spare;

        // Reset the stat info
        last->bufstatinfo = 0;
    }
    else
    {
        // Otherwise we free the spare descriptor
        struct rx_pbd *free = rx_hwdesc_env.free;

        // Save the new free descriptor
        rx_hwdesc_env.free = spare;

        // Add the free element at the end of the list
        if (last == NULL)
        {
            // No element in the list
            first = last = free;
        }
        else
        {
            free->next = CPU2HW(first);
            first = free;
        }

        // Reset the stat info
        first->bufstatinfo = 0;
    }

    // reset the next pointer
    last->next = 0;

    // Link the descriptors to the current list and trigger a newTail
    rx_hwdesc_env.last->next = CPU2HW(first);
    nxmac_dma_cntrl_set(NXMAC_RX_PAYLOAD_NEW_TAIL_BIT);

    // record the last pointer
    rx_hwdesc_env.last = last;

    // check if the pointer to the first descriptor is NULL (debug and error logs only)
    if (rx_hwdesc_env.first == NULL)
    {
        rx_hwdesc_env.first = first;
    }
	GLOBAL_INT_RESTORE();
}

/// @} // end of group RXHWDESC
