ENTRY(Reset_Handler)

/*
 * STM32N657 development-mode RAM image.
 *
 * ST's FSBL templates reserve the first 1 KiB of AXISRAM2 and place their
 * vector table at 0x34180400.  Keep that convention, but use the contiguous
 * secure AXISRAM alias through 0x343C0000 so the large HAETAE differential
 * image, deterministic benchmark heap, and measured stack fit together.
 * probe-rs loads this image directly; no external flash is programmed.  The
 * link uses --nmagic so an ELF page-aligned LOAD header cannot extend backward
 * over the boot ROM's reserved 0x34180000..0x341803ff area.
 */
MEMORY
{
    AXISRAM (rwx) : ORIGIN = 0x34180400, LENGTH = 0x0023FC00
}

PHDRS
{
    code PT_LOAD FLAGS(5); /* read + execute */
    data PT_LOAD FLAGS(6); /* read + write */
}

__stack_size__ = 512K;
__heap_size__ = 512K;
_estack = ORIGIN(AXISRAM) + LENGTH(AXISRAM);
__stack_limit__ = _estack - __stack_size__;

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(1024);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > AXISRAM :code

    .text :
    {
        . = ALIGN(16);
        *(.text*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(16);
    } > AXISRAM :code

    .rodata :
    {
        . = ALIGN(16);
        *(.rodata*)
        . = ALIGN(16);
        _etext = .;
    } > AXISRAM :code

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > AXISRAM :code

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > AXISRAM :code

    .data :
    {
        . = ALIGN(16);
        _sdata = .;
        *(.data*)
        . = ALIGN(16);
        _edata = .;
    } > AXISRAM :data
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(16);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(16);
        _ebss = .;
    } > AXISRAM :data

    .heap (NOLOAD) :
    {
        . = ALIGN(16);
        __heap_start__ = .;
        . += __heap_size__;
        . = ALIGN(16);
        __heap_end__ = .;
    } > AXISRAM :data

    .bench_output (NOLOAD) :
    {
        . = ALIGN(32);
        __bench_output_start__ = .;
        KEEP(*(.bench_output))
        . = ALIGN(32);
        __bench_output_end__ = .;
    } > AXISRAM :data

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
    }
}

ASSERT(SIZEOF(.isr_vector) >= 64, "vector table is incomplete")
ASSERT(__bench_output_end__ <= __stack_limit__, "AXISRAM image/heap/output overlaps stack")
