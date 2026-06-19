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

    .section .text.entry, "ax"
    .globl  _start
    .globl  _entry

_start:
_entry:
    /* Keep the stack below the Sega CD BIOS/system-use region at $FFF700+. */
    lea     __stack, %sp

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

    .globl  probe_bios_clear_comm
probe_bios_clear_comm:
    jsr     0x000340
    rts

    .globl  main_enable_interrupts
main_enable_interrupts:
    andi    #0xF8FF, %sr
    rts

    .ifdef BOOT_PROBE
    .globl  probe_enable_main_interrupts
probe_enable_main_interrupts:
    jsr     main_enable_interrupts
    rts
    .endif
