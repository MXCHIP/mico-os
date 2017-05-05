/* MiCO Team
 * Copyright (c) 2017 MXCHIP Information Tech. Co.,Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mico_common.h"
#include "mico_debug.h"
#include "platform_peripheral.h"


#define CTHUNK_ADDRESS 1
#define CTHUNK_VARIABLES volatile uint32_t code[2]

#if defined (__ICCARM__)
        typedef __packed struct
        {
            CTHUNK_VARIABLES;
            volatile uint32_t instance;
            volatile uint32_t context;
            volatile uint32_t callback;
            volatile uint32_t trampoline;
        }  CThunkTrampoline;
#else
        typedef struct
        {
            CTHUNK_VARIABLES;
            volatile uint32_t instance;
            volatile uint32_t context;
            volatile uint32_t callback;
            volatile uint32_t trampoline;
        } __attribute__((__packed__)) CThunkTrampoline;
#endif



#if (defined(__CORTEX_M3) || defined(__CORTEX_M4) || defined(__CORTEX_M7) || defined(__CORTEX_A9))
/**
* CTHUNK disassembly for Cortex-M3/M4/M7/A9 (thumb2):
* * adr  r0, #4
* * ldm  r0, {r0, r1, r2, pc}
*
* This instruction loads the arguments for the static thunking function to r0-r2, and
* branches to that function by loading its address into PC.
*
* This is safe for both regular calling and interrupt calling, since it only touches scratch registers
* which should be saved by the caller, and are automatically saved as part of the IRQ context switch.
*/
#define CTHUNK_ASSIGMENT do {                              \
                             irq_swap->code[0] = 0xE890A001; \
                             irq_swap->code[1] = 0x00008007; \
                         } while (0)

#elif (defined(__CORTEX_M0PLUS) || defined(__CORTEX_M0))
/*
* CTHUNK disassembly for Cortex M0/M0+ (thumb):
* * adr  r0, #4
* * ldm  r0, {r0, r1, r2, r3}
* * bx   r3
*/
#define CTHUNK_ASSIGMENT do {                              \
                             irq_swap->code[0] = 0xC80FA001; \
                             irq_swap->code[1] = 0x00004718; \
                         } while (0)

#else
#error "Target is not currently suported."
#endif




static void trampoline( void* instance, void* context, platform_irq_callback callback )
{
    if ( callback )  (*callback)(context);
}

void platform_irq_init( platform_irq_handle *irq, void* context, platform_irq_callback callback)
{
    CThunkTrampoline *irq_swap = NULL;

    if( *irq != NULL ) return;

    irq_swap = (CThunkTrampoline *)malloc( sizeof(CThunkTrampoline));
    if( irq_swap == NULL ) return;
    *irq = irq_swap;

    /* populate thunking trampoline */
    CTHUNK_ASSIGMENT;
    irq_swap->instance = (uint32_t)NULL;
    irq_swap->context = (uint32_t)context;
    irq_swap->callback = (uint32_t)callback;
    irq_swap->trampoline = (uint32_t)&trampoline;

#if defined(__CORTEX_A9)
    /* Data cache clean */
    /* Cache control */
    {
        uint32_t start_addr = (uint32_t)&m_thunk & 0xFFFFFFE0;
        uint32_t end_addr = (uint32_t)&m_thunk + sizeof(m_thunk);
        uint32_t addr;

        /* Data cache clean and invalid */
        for (addr = start_addr; addr < end_addr; addr += 0x20) {
            __v7_clean_inv_dcache_mva((void *)addr);
        }
        /* Instruction cache invalid */
        __v7_inv_icache_all();
        __ca9u_inv_tlb_all();
        __v7_inv_btac();
    }
#endif
#if defined(__CORTEX_M7)
    /* Data cache clean and invalid */
    SCB_CleanInvalidateDCache();

    /* Instruction cache invalid */
    SCB_InvalidateICache();
#endif
    __ISB( );
    __DSB( );
}

void platform_irq_deinit( platform_irq_handle *irq )
{
    if ( *irq ) free( *irq );
    *irq = NULL;
}

