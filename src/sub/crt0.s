/*
 * crt0.s - Sub CPU Startup Assembly
 *
 * This provides the SP (Sub CPU Program) header required by the
 * Sega CD BIOS, plus C runtime initialization (BSS clear, stack
 * setup, jump to C main).
 *
 * The BIOS loads this to PRG-RAM at $006000 and calls through
 * the jump table defined in the SP header.
 *
 * SP Header Layout (at $006000):
 *   $006000: jmp sp_init    (usercall0 - initialization)
 *   $006006: jmp sp_main    (usercall1 - main loop)
 *   $00600C: jmp sp_int2    (usercall2 - level 2 interrupt)
 *   $006012: jmp sp_user    (usercall3 - user call)
 *
 * Reference: Megadev ip_sp.md, BIOS Manual Sections 4-5
 */

    .section .boot, "ax"
    .global _sp_header

_sp_header:
    /* SP Header: BIOS jump table (4 entries, 6 bytes each) */
    jmp     sp_init         /* usercall0: initialization */
    jmp     sp_main         /* usercall1: main entry */
    jmp     sp_int2         /* usercall2: level 2 interrupt */
    jmp     sp_user         /* usercall3: user-defined */

/*
 * sp_init - Called by BIOS during initialization
 *
 * Sets up the C runtime environment:
 *   1. Initialize stack pointer
 *   2. Clear BSS section
 *   3. Call C initialization function
 */
    .text
    .global sp_init
sp_init:
    /* Set stack pointer */
    lea     _stack, %sp

    /* Clear BSS: zero all uninitialized data */
    lea     _bss, %a0           /* Start of BSS */
    lea     _ebss, %a1          /* End of BSS */
    bra.s   .bss_check
.bss_loop:
    clr.l   (%a0)+              /* Clear 4 bytes at a time */
.bss_check:
    cmp.l   %a1, %a0
    blt.s   .bss_loop

    /* Call C initialization */
    jsr     sub_init

    rts

/*
 * sp_main - Called by BIOS as the main entry point
 *
 * This is the primary execution loop. The BIOS calls this
 * after sp_init completes. We jump to the C main loop and
 * never return.
 */
    .global sp_main
sp_main:
    /* Set stack pointer (in case BIOS clobbered it) */
    lea     _stack, %sp

    /* Enter C main loop (does not return) */
    jsr     sub_main

    /* If sub_main somehow returns, halt */
.halt:
    bra.s   .halt

/*
 * sp_int2 - Level 2 interrupt handler
 *
 * Called by BIOS for the level 2 interrupt (typically used
 * for graphics-related timing on the Sub CPU side).
 * Currently unused - just return.
 */
    .global sp_int2
sp_int2:
    rts

/*
 * sp_user - User-defined BIOS callback
 *
 * Called by BIOS for user-defined purposes.
 * Currently unused - just return.
 */
    .global sp_user
sp_user:
    rts

    .end
