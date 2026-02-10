/*
 * crt0.s - Main CPU Startup Assembly (Sega CD IP Entry Point)
 *
 * The Sega CD BIOS loads the IP (Initial Program) into Work RAM
 * at $FF0000 and jumps to the entry point. This is our first code.
 *
 * The BIOS has already:
 *   - Set up the 68000 vector table in system RAM
 *   - Initialized hardware basics
 *   - Loaded the SP (Sub CPU program) to PRG-RAM at $006000
 *
 * We need to:
 *   1. Set our stack pointer
 *   2. Clear .bss
 *   3. Jump to C main()
 */

    .text
    .globl  _start
    .globl  _entry

_start:
_entry:
    /* Set stack pointer to top of Work RAM */
    lea     0x01000000, %sp

    /* Clear BSS section */
    lea     _sbss, %a0
    lea     _ebss, %a1
    moveq   #0, %d0
.clear_bss:
    cmp.l   %a0, %a1
    beq.s   .bss_done
    move.l  %d0, (%a0)+
    bra.s   .clear_bss
.bss_done:

    /* Jump to C main */
    jsr     main

    /* If main() returns, halt */
.halt:
    stop    #0x2700
    bra.s   .halt
