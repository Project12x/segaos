OUTPUT_ARCH(m68k)
SEARCH_DIR(.)
__DYNAMIC  =  0;

MEMORY
{
    /* PRG-RAM: 512KB total ($000000-$07FFFF)                      */
    /* First $6000 bytes reserved for BIOS vectors/system program  */
    /* Usable: $006000-$07FFFF = 488KB (0x7A000)                   */
    prg_ram (rwx) : ORIGIN = 0x006000, LENGTH = 0x07A000

    /* Word RAM: 256KB total                                        */
    /* 2M mode: $080000-$0BFFFF (all 256KB to one CPU)              */
    /* 1M mode: Bank 0 at $0C0000, Bank 1 at $0E0000 (128KB each)  */
    /* We use 1M mode, so map Bank 0 here for framebuffer           */
    word_ram (rw) : ORIGIN = 0x0C0000, LENGTH = 0x020000
}

SECTIONS
{
    /* SP Header: BIOS jump table (must be first at $006000) */
    .boot :
    {
        KEEP(*(.boot))
    } > prg_ram

    .text :
    {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        _etext = .;
    } > prg_ram

    .data :
    {
        _data = .;
        *(.data .data.*)
        . = ALIGN(4);
        _edata = .;
    } > prg_ram

    .bss :
    {
        _bss = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > prg_ram

    /* Heap: between BSS and stack guard */
    _heap_start = _ebss;
    _heap_end = _stack - 256;   /* 256 bytes stack guard */

    /* Stack at end of PRG-RAM (grows downward) */
    _stack = ORIGIN(prg_ram) + LENGTH(prg_ram);
}
