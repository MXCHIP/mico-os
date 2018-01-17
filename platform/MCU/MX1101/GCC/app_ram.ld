/* Entry Point */
ENTRY(Reset_Handler)

/* Highest address of the user mode stack */
_estack = 0x20010000;
/* Generate a link error if heap and stack don't fit into RAM */
_Min_Heap_Size = 0x1000;      /* required amount of heap  */
_Min_Stack_Size = 0x1000; /* required amount of stack */

/* Specify the memory areas */
MEMORY
{
RAM (xrw)      : ORIGIN = 0x20000000, LENGTH = 128K
FLASH (rx)     : ORIGIN = 0x0000B000, LENGTH = 1M
}

/* Define output sections */
SECTIONS
{
  /* The startup code goes first into RAM */
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Startup code */
    . = ALIGN(4);
  } >RAM

  /* The program code and other data goes into RAM */
  .text :
  {
    . = ALIGN(4);
    *(.text)           /* .text sections (code) */
    *(.text*)          /* .text* sections (code) */

    . = ALIGN(4);
    _etext = .;        /* define a global symbols at end of code */
  } >RAM

  /* Constant data goes into RAM */
  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)         /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)        /* .rodata* sections (constants, strings, etc.) */
    . = ALIGN(4);
  } >RAM

  /* used by the startup to initialize data */
  _sidata = LOADADDR(.data);

  /* Initialized data sections goes into RAM, load LMA copy after code */
  .data : 
  {
    . = ALIGN(4);
    _sdata = .;        /* create a global symbol at data start */
    *(.data)           /* .data sections */
    *(.data*)          /* .data* sections */

    . = ALIGN(4);
    _edata = .;        /* define a global symbol at data end */
  } >RAM AT> RAM

  
  /* Uninitialized data section */
  . = ALIGN(4);
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss secion */
    _sbss = .;         /* define a global symbol at bss start */
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)

    . = ALIGN(4);
    _ebss = .;         /* define a global symbol at bss end */
    __bss_end__ = _ebss;
  } >RAM

  /* User_heap_stack section, used to check that there is enough RAM left */
  ._user_heap_stack :
  {
    . = ALIGN(8);
    PROVIDE ( end = . );
    PROVIDE ( _end = . );
    PROVIDE ( _heap = . );
    . = . + _Min_Heap_Size;
    PROVIDE ( _eheap = . );
    . = . + _Min_Stack_Size;
    . = ALIGN(8);
  } >RAM

  /DISCARD/ :
  {
      *(.ARM.attributes*)
      *(.comment)
      *(.init)
      *(.preinit)
      *(.fini)
      *(.fini_array)
      *(.ARM.exidx*)
      *(.gnu.linkonce.armexidx.*)
      *(.eh_frame_hdr)
      *(.eh_frame)
      *(.gnu.linkonce.armextab.*)
      *(.v4_bx)
      *(.vfp11_veneer)
      *(.gcc_except_table)
      *(.eh_frame_hdr)
      *(.eh_frame)
      *(.glue*)
  }
}
