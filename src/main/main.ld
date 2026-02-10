/*
 * main.ld - Main CPU Linker Script (Sega CD IP)
 *
 * The IP (Initial Program) runs from Work RAM at $FF0000.
 * Total Work RAM: 64KB ($FF0000 - $FFFFFF).
 *
 * The Sega CD BIOS loads the IP binary here and jumps to _entry.
 * The Sub CPU binary is NOT embedded in IP for CD boot (BIOS loads
 * it separately). For cart/Mode 1 testing, it IS embedded.
 *
 * Memory Map (Main CPU view):
 *   $000000-$3FFFFF : ROM/Cart area (not used on Sega CD disc boot)
 *   $200000-$23FFFF : Word RAM (2M mode) or $200000/$220000 (1M banks)
 *   $420000-$43FFFF : PRG-RAM window (128KB bank, controlled by GA)
 *   $A12000-$A1202F : Gate Array registers
 *   $C00000-$C00013 : VDP ports
 *   $FF0000-$FFFFFF : Work RAM (68000 internal, 64KB)
 */

OUTPUT_FORMAT("elf32-m68k")
OUTPUT_ARCH(m68k)

ENTRY(_entry)

MEMORY
{
    wram (rwx) : ORIGIN = 0xFF0000, LENGTH = 0x10000
}

/* Stack grows down from top of Work RAM */
PROVIDE(__stack = 0x01000000);

SECTIONS
{
    .text :
    {
        _stext = .;
        *(.text.entry)      /* Entry point first */
        KEEP(*(.text.entry))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        _etext = .;
    } > wram

    .data :
    {
        _sdata = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > wram

    .bss (NOLOAD) :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > wram

    /* Heap: from end of BSS to before stack guard */
    _heap_start = _ebss;
    _heap_end = __stack - 0x400; /* 1KB stack guard */

    /* Discard debug sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
