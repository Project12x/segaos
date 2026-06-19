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
 *   module name/version/size fields followed by a relative jump table.
 *
 * Reference: Megadev lib/sub/sp_header.s, BIOS Manual Sections 4-5
 */

    .section .header, "a"
    .global _sp_header

_sp_header:
    .ascii  "MAIN       "   /* 10-byte module name */
    .byte   0               /* flag */
    .word   0x0100          /* version */
    .word   0               /* type */
    .long   0               /* pointer to next module */
    .long   _TEXT_LENGTH    /* resident text/rodata size */
    .long   sp_jmptbl - _sp_header
    .long   _RAM_LENGTH     /* work RAM use */

sp_jmptbl:
    .word   sp_init - sp_jmptbl
    .word   sp_main - sp_jmptbl
    .word   sp_int2 - sp_jmptbl
    .word   sp_user - sp_jmptbl
    .word   0

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
    .ifdef BOOT_PROBE
    move.w  #0x0002, 0xFF8020   /* status0: SUB_STATE_READY */
    move.w  #0x5244, 0xFF8022   /* status1: PROBE_READY_MAGIC */
    move.w  #0x5101, 0xFF802E   /* status7: entered sp_init */
    move.b  #0x00, 0xFF800F    /* CFS: STATUS_IDLE */
    rts
    .else
    move.w  #0x0002, 0xFF8020   /* status0: SUB_STATE_READY */
    move.w  #0x5101, 0xFF802E   /* status7: entered sp_init */
    move.b  #0x00, 0xFF800F    /* CFS: STATUS_IDLE */
    .endif

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

    .ifdef BOOT_PROBE
    move.w  #0x5102, 0xFF802E   /* status7: BSS clear finished */
    .endif

    /* Call C initialization */
    jsr     sub_init

    .ifdef BOOT_PROBE
    move.w  #0x5103, 0xFF802E   /* status7: sub_init returned */
    .endif

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
    .ifdef BOOT_PROBE
    move.w  #0x5201, 0xFF802E   /* status7: entered sp_main */
  .probe_wait_cmd:
    move.b  0xFF800E, %d0       /* CFM: Main command flag */
    cmpi.b  #0x03, %d0          /* CMD_BOOT_PROBE */
    bne.s   .probe_wait_cmd

    move.b  #0x01, 0xFF800F    /* CFS: STATUS_BUSY */
    move.w  0xFF8010, %d0       /* param0 */
    move.w  0xFF8012, %d1       /* param1 */
    move.w  0xFF8014, %d2       /* param2: nonzero enables WRAM survey */

    clr.w   0xFF8028            /* status4: Sub $0C0000 readback */
    clr.w   0xFF802A            /* status5: Sub $0C0002 readback */
    clr.w   0xFF802C            /* status6: Sub $0E0000 readback or MEM_MODE */
    cmpi.w  #0x0003, %d2
    beq.w   .probe_framebuffer
    tst.w   %d2
    beq.w   .probe_skip_wram_survey

    move.w  %d0, 0x0C0000       /* Sub 1M window 0 word 0 */
    move.w  %d1, 0x0C0002       /* Sub 1M window 0 word 1 */
    move.w  #0xB10B, 0x0E0000   /* Sub 1M window 1 survey word */
    move.w  #0xB11B, 0x0E0002
    move.w  0x0C0000, 0xFF8028
    move.w  0x0C0002, 0xFF802A
    move.w  0x0E0000, 0xFF802C
    cmpi.w  #0x0002, %d2
    bne.s   .probe_skip_ret_clear
    move.w  0xFF8002, %d3
    andi.w  #0xFFFE, %d3
    move.w  %d3, 0xFF8002
    move.w  0xFF8002, 0xFF802C

  .probe_skip_ret_clear:

    bra.w   .probe_skip_wram_survey

  .probe_framebuffer:
    lea     0x0C0000, %a0
    clr.w   %d6                 /* y */
    move.w  #223, %d4
  .probe_fb_row_loop:
    move.w  #39, %d5            /* 40 two-word groups per 320px row */
    btst    #0, %d6
    bne.s   .probe_fb_odd_row
  .probe_fb_even_row:
    move.w  #0x1234, (%a0)+
    move.w  #0x5678, (%a0)+
    dbra    %d5, .probe_fb_even_row
    bra.s   .probe_fb_next_row
  .probe_fb_odd_row:
    move.w  #0x9ABC, (%a0)+
    move.w  #0xDEF0, (%a0)+
    dbra    %d5, .probe_fb_odd_row
  .probe_fb_next_row:
    addq.w  #1, %d6
    dbra    %d4, .probe_fb_row_loop

    lea     0x0C00A0, %a0       /* row 1, byte offset 160 */
    move.w  0x0C0000, 0xFF8028
    move.w  0x0C0002, 0xFF802A
    move.w  (%a0), 0xFF802C
    move.w  0xFF8002, %d3
    andi.w  #0xFFFE, %d3
    move.w  %d3, 0xFF8002

  .probe_skip_wram_survey:

    move.w  #0x444E, 0xFF8020   /* status0: PROBE_DONE_MAGIC */
    move.w  %d0, 0xFF8022       /* status1: echoed word0 */
    move.w  %d1, 0xFF8024       /* status2: echoed word1 */
    move.w  0xFF8002, 0xFF8026  /* status3: Sub MEM_MODE snapshot */
    move.w  #0x52FE, 0xFF802E   /* status7: command done, no WRAM return */
    tst.w   %d2
    beq.s   .probe_set_done
    move.w  #0x53FE, 0xFF802E   /* status7: WRAM survey done, no RET wait */
    cmpi.w  #0x0002, %d2
    bne.s   .probe_set_done
    move.w  #0x54FE, 0xFF802E   /* status7: WRAM survey plus RET clear */
    bra.w   .probe_set_done

  .probe_set_done:
    cmpi.w  #0x0003, %d2
    beq.s   .probe_set_framebuffer_trace_done
    move.b  #0x03, 0xFF800F    /* CFS: STATUS_DONE */
    bra.s   .probe_wait_main_clear

  .probe_set_framebuffer_trace_done:
    move.w  #0x55FE, 0xFF802E   /* status7: framebuffer pattern plus RET clear */
    move.b  #0x03, 0xFF800F    /* CFS: STATUS_DONE */

  .probe_wait_main_clear:
    move.b  0xFF800E, %d0
    bne.s   .probe_wait_main_clear
    move.b  #0x00, 0xFF800F    /* CFS: STATUS_IDLE */
    bra.w   .probe_wait_cmd
    .else
    move.w  #0x0002, 0xFF8020   /* status0: SUB_STATE_READY */
    move.w  #0x5201, 0xFF802E   /* status7: entered sp_main */
    move.b  #0x00, 0xFF800F    /* CFS: STATUS_IDLE */
    .endif

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
