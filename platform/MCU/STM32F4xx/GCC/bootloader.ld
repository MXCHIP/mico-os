/**
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 *
 */
 
Reset_Handler = _mico_start;

ENTRY( _mico_start );

/* Include memory map */
INCLUDE memory.ld

SECTIONS
{
    .ramcode_copy :
    {
        KEEP(*(*.interrupt_vector_table_copy_ramcode))
        KEEP(*(.copy_ramcode))
        . = ALIGN(512);
    }>BL_FLASH  AT> BL_FLASH

    .text :
    {
    	link_run_from_ram_code_flash_location = LOADADDR( .text );
    	link_run_from_ram_code_ram_location = .;
        link_interrupt_vectors_location = .;
        KEEP(*(*.interrupt_vector_table))
        KEEP(*(.stub_section))
        
        
        . = ALIGN(4);

        link_code_location = .;
        KEEP(*(.text.irq ))
        *(.text .text.* .gnu.linkonce.t.*)
        KEEP(*(.init))
        KEEP(*(.fini))
        link_code_end = .;

        /* .ctors */
        *crtbegin.o(.ctors)
        *crtbegin?.o(.ctors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .ctors)
        *(SORT(.ctors.*))
        *(.ctors)

        /* .dtors */
        *crtbegin.o(.dtors)
        *crtbegin?.o(.dtors)
        *(EXCLUDE_FILE(*crtend?.o *crtend.o) .dtors)
        *(SORT(.dtors.*))
        *(.dtors)

        *(.rodata*)

        KEEP(*(.eh_frame*))
        
        link_run_from_ram_code_ram_end = .;
    }>SRAM  AT> BL_FLASH

    .data : /* Contains the non-zero initialised global variables */
    {
        link_global_data_initial_values = LOADADDR( .data ); /* This is the location in flash of the initial values of global variables */
        link_global_data_start = .;
        *(.data*)

        . = ALIGN(4);
        /* preinit data */
        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP(*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);

        . = ALIGN(4);
        /* init data */
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP(*(SORT(.init_array.*)))
        KEEP(*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);

        . = ALIGN(4);
        /* finit data */
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP(*(SORT(.fini_array.*)))
        KEEP(*(.fini_array))
        PROVIDE_HIDDEN (__fini_array_end = .);

        KEEP(*(.jcr*))
        . = ALIGN(4);

        link_global_data_end = .;
        /* All data end */
        __data_end__ = .;
        _edata = .;
    }> SRAM AT> BL_FLASH


    .bss : /* Zero initialised memory used for zero initialised variables */
    {
        link_bss_location = ALIGN(., 4);
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        link_bss_end = .;
        . = ALIGN(., 4);
        __bss_end__ = .;
    }> SRAM AT> SRAM

    .stack : /* Contains the initial stack */
    {
        link_stack_location = ALIGN(., 4);
        *(.stack)
        . = ALIGN(MAX(link_stack_location + __STACKSIZE__ , .), 4);
        link_stack_end = .;
    }> SRAM AT> SRAM

}
 
/* Declare libc Heap to start at end of allocated RAM */

PROVIDE( _heap = link_stack_end );
PROVIDE(__stack = link_stack_end);

/* End of the heap is top of RAM, aligned 8 byte */

PROVIDE( _eheap = ALIGN( ORIGIN( SRAM ) + LENGTH( SRAM ) - 8, 8 ) );

