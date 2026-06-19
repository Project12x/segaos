/*
 * Megadev-derived dual-CPU control SP linker.
 *
 * Reference: drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef
 * Files inspected: cfg/sp.ld, lib/sub/sp_header.s
 * License: MIT
 * Reuse mode: close-port/pattern-only for section layout and module metadata.
 */

OUTPUT_ARCH(m68k)

MEMORY
{
  SP (rx): ORIGIN = 0x6000, LENGTH = 0x4000
}

SECTIONS
{
  .text : SUBALIGN(2)
  {
    _TEXT_ORIGIN = .;
    KEEP(*(.header*))
    *(.text*)
    *(.rodata*)
    . = ALIGN(2);
    _TEXT_LENGTH = ABSOLUTE(. - _TEXT_ORIGIN);
  } > SP

  .data : SUBALIGN(2)
  {
    _RAM_ORIGIN = .;
    _RAM_DATA_ORIGIN = .;
    *(.data*)
    . = ALIGN(4);
    _RAM_DATA_LENGTH = ABSOLUTE(. - _RAM_DATA_ORIGIN);
  } > SP

  .bss : SUBALIGN(2)
  {
    _BSS_ORIGIN = .;
    *(.bss*)
    . = ALIGN(4);
    _BSS_LENGTH = ABSOLUTE(. - _BSS_ORIGIN);
  } > SP

  _RAM_LENGTH = _RAM_DATA_LENGTH + _BSS_LENGTH;
}
