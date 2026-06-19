/*
 * Megadev-derived dual-CPU control SP.
 *
 * Purpose: report from sp_init and sp_main through Gate Array COMSTAT
 * registers, proving whether the BIOS user-call table reaches the SP payload.
 *
 * Reference: drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef
 * Files inspected: lib/sub/sp_header.s, examples/hello_world/src/sp.s,
 * lib/sub/gate_arr.def.h
 * License: MIT
 * Reuse mode: close-port for the SP module header shape; clean-room control
 * code for the COMSTAT breadcrumbs.
 */

    .equ GA_COMFLAG_SUB, 0xFF800F
    .equ GA_COMSTAT0,   0xFF8020
    .equ GA_COMSTAT1,   0xFF8022
    .equ GA_COMSTAT2,   0xFF8024
    .equ GA_COMSTAT3,   0xFF8026
    .equ GA_COMSTAT7,   0xFF802E

    .section .header, "a"
    .global _sp_header

_sp_header:
    .ascii  "MAIN       "
    .byte   0
    .word   0x0100
    .word   0
    .long   0
    .long   _TEXT_LENGTH
    .long   sp_jmptbl - _sp_header
    .long   _RAM_LENGTH

sp_jmptbl:
    .word   sp_init - sp_jmptbl
    .word   sp_main - sp_jmptbl
    .word   sp_int2 - sp_jmptbl
    .word   sp_user - sp_jmptbl
    .word   0

    .section .text, "ax"
    .global sp_init
    .global sp_main
    .global sp_int2
    .global sp_user

sp_init:
    move.w  #0x5350, GA_COMSTAT0      /* "SP" */
    move.w  #0x494E, GA_COMSTAT1      /* "IN" */
    move.w  #0x5101, GA_COMSTAT7
    move.b  #0x03, GA_COMFLAG_SUB
    rts

sp_main:
    move.w  #0x4D41, GA_COMSTAT2      /* "MA" */
    move.w  #0x494E, GA_COMSTAT3      /* "IN" */
    move.w  #0x5201, GA_COMSTAT7
    move.b  #0x05, GA_COMFLAG_SUB
.loop:
    bra.s   .loop

sp_int2:
    move.w  #0x1202, GA_COMSTAT7
    rts

sp_user:
    move.w  #0x5501, GA_COMSTAT7
    rts
