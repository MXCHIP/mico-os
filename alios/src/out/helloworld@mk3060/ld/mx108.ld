INCLUDE memory.ld
OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_vector_start);
_vector_start = 0x0012000;
SECTIONS
{
 . = 0x0012000;
 .vectors :
 {
      KEEP(*(*.vectors))
 } > flash
 . = ALIGN(0x8);
 .text :
 {
     *(.text)
     *(.text.*)
     *(.stub)
     *(.gnu.warning)
     *(.gnu.linkonce.t*)
     *(.glue_7t) *(.glue_7)
 } > flash
 .rodata ALIGN(8) :
 {
  *(.rodata)
  *(.rodata.*)
  *(.gnu.linkonce.r*)
 } > flash
 .ARM.exidx :
 {
            __exidx_start = .;
     *(.ARM.exidx*)
     *(.gnu.linkonce.armexidx.*)
            __exidx_end = .;
 } > flash
 . = ALIGN(0x8);
 _begin_data = .;
 .data : AT ( _begin_data )
 {
  *(.data .data.*)
  *(.sdata)
  *(.gnu.linkonce.d*)
        SORT(CONSTRUCTORS)
 } >ram
 _data_flash_begin = LOADADDR(.data);
 _data_ram_begin = ADDR(.data);
 _data_ram_end = .;
 .bss ALIGN(8):
 {
  _bss_start = .;
  *(.bss .bss.*)
  *(.scommon)
  *(.sbss)
    *(.dynbss)
    *(COMMON)
    . = ALIGN(32 / 8);
  _bss_end = .;
 } > ram
 . = ALIGN (8);
 _empty_ram = .;
 PROVIDE(end = .);
 PROVIDE(heap_start = .);
    PROVIDE(kernel_download_addr = 0x0013200);
    PROVIDE(framework_download_addr = 0xffffffff);
    PROVIDE(app_download_addr = 0xffffffff);
 PROVIDE(_stack_unused = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0 - 0x3F0 - 0x10);
 PROVIDE(_stack_svc = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0 - 0x3F0);
 PROVIDE(_stack_irq = 0x440000 - 0x3F0 - 0x7F0 - 0xFF0);
 PROVIDE(_stack_fiq = 0x440000 - 0x3F0 - 0x7F0);
 PROVIDE(_stack_sys = 0x440000 - 0x3F0);
 ASSERT ((_stack_unused > end), "Error: No room left for the stack")
 PROVIDE(heap_end = _stack_unused);
 PROVIDE(heap_len = heap_end - heap_start);
 ASSERT ((heap_len > 0x1E000 - 1 ), "Error: No room left for the heap")
}
 GROUP(
   libgcc.a
   libg.a
   libc.a
   libm.a
   libnosys.a
 )
