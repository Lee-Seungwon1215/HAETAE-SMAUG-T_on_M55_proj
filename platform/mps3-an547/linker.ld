ENTRY(Reset_Handler)

MEMORY
{
    ITCM (rx)  : ORIGIN = 0x00000000, LENGTH = 512K
    DTCM (rwx) : ORIGIN = 0x20000000, LENGTH = 512K
}

/*
 * HAETAE's upstream incremental-XOF API allocates a context for each public
 * rejection attempt. The benchmark reuses the arena between top-level
 * iterations, but one long-tail operation can exceed 64 KiB before returning.
 * Keep a measured 80+ KiB margin over the Phase 2 HAETAE5 stack watermark and
 * use the remaining DTCM for the per-iteration semihost heap.
 */
__stack_size__ = 240K;
__heap_size__ = 224K;
_estack = ORIGIN(DTCM) + LENGTH(DTCM);
__stack_limit__ = _estack - __stack_size__;

SECTIONS
{
    .isr_vector :
    {
        . = ALIGN(4);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > ITCM

    .text :
    {
        . = ALIGN(4);
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
        _etext = .;
    } > ITCM

    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > ITCM

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > ITCM

    .data :
    {
        . = ALIGN(8);
        _sdata = .;
        *(.data*)
        . = ALIGN(8);
        _edata = .;
    } > DTCM AT > ITCM
    _sidata = LOADADDR(.data);

    .bss (NOLOAD) :
    {
        . = ALIGN(8);
        _sbss = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(8);
        _ebss = .;
    } > DTCM

    .heap (NOLOAD) :
    {
        . = ALIGN(8);
        __heap_start__ = .;
        . += __heap_size__;
        . = ALIGN(8);
        __heap_end__ = .;
    } > DTCM

    /DISCARD/ :
    {
        *(.comment*)
        *(.note*)
    }
}

ASSERT(SIZEOF(.isr_vector) >= 64, "vector table is incomplete")
ASSERT((ORIGIN(ITCM) + LENGTH(ITCM)) >= (LOADADDR(.data) + SIZEOF(.data)),
       "code and data image overflow ITCM")
ASSERT(__heap_end__ <= __stack_limit__, "DTCM data/heap overlaps stack")
