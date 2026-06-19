/*
 * Megadev-derived dual-CPU control IP linker.
 *
 * Reference: drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef
 * Files inspected: cfg/ip.ld, examples/hello_world/src/ip.s
 * License: MIT
 * Reuse mode: close-port/pattern-only for section layout and IP envelope.
 */

OUTPUT_ARCH(m68k)
ENTRY(_entry)

MEMORY
{
  IP (rwx): ORIGIN = 0xFF0000, LENGTH = 0x0E00
}

SECTIONS
{
  .text : SUBALIGN(2)
  {
    _TEXT_ORIGIN = .;
    KEEP(*(.security*))
    KEEP(*(.text.entry*))
    *(.text*)
    *(.rodata*)
    _TEXT_LENGTH = ABSOLUTE(. - _TEXT_ORIGIN);
  } > IP

  .bss (NOLOAD) : SUBALIGN(2)
  {
    _BSS_ORIGIN = .;
    *(.bss*)
    . = ALIGN(4);
    _BSS_LENGTH = ABSOLUTE(. - _BSS_ORIGIN);
  } > IP

  /DISCARD/ :
  {
    *(.comment)
    *(.eh_frame)
    *(.eh_frame_hdr)
  }
}
