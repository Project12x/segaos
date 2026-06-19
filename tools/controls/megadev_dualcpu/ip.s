/*
 * Megadev-derived dual-CPU control IP.
 *
 * Purpose: wait briefly for a Sub CPU SP to report from sp_init/sp_main through
 * Gate Array COMSTAT registers, then halt on a debugger-visible symbol.
 *
 * Reference: drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef
 * Files inspected: examples/hello_world/src/ip.s, lib/main/gate_arr.def.h
 * License: MIT
 * Reuse mode: clean-room control using the same IP envelope and GA addresses.
 */

    .equ GA_COMFLAG_SUB, 0xA1200F
    .equ GA_COMSTAT0,   0xA12020
    .equ GA_COMSTAT1,   0xA12022
    .equ GA_COMSTAT2,   0xA12024
    .equ GA_COMSTAT3,   0xA12026
    .equ GA_COMSTAT7,   0xA1202E

    .section .text.entry, "ax"
    .global _entry
    .global megadev_control_halt

_entry:
    move.w  #0x2700, %sr
    move.l  #0x00FFF600, %sp
    move.w  #0x4D43, megadev_control_main_magic  /* "MC" */
    move.w  #0x0001, megadev_control_phase

    move.l  #0x00080000, %d7

.sample_loop:
    moveq   #0, %d0
    move.b  GA_COMFLAG_SUB, %d0
    move.w  %d0, megadev_control_sub_flag
    move.w  GA_COMSTAT0, megadev_control_stat0
    move.w  GA_COMSTAT1, megadev_control_stat1
    move.w  GA_COMSTAT2, megadev_control_stat2
    move.w  GA_COMSTAT3, megadev_control_stat3
    move.w  GA_COMSTAT7, megadev_control_trace

    cmpi.w  #0x5350, megadev_control_stat0      /* "SP" */
    bne.s   .keep_waiting
    cmpi.w  #0x494E, megadev_control_stat1      /* "IN" */
    bne.s   .keep_waiting
    move.w  #0x0002, megadev_control_phase
    bra.s   megadev_control_halt

.keep_waiting:
    subq.l  #1, %d7
    bne.s   .sample_loop
    move.w  #0x00FF, megadev_control_phase

megadev_control_halt:
    stop    #0x2700
    bra.s   megadev_control_halt

    .section .bss, "aw"
    .align 2
    .global megadev_control_main_magic
    .global megadev_control_phase
    .global megadev_control_sub_flag
    .global megadev_control_stat0
    .global megadev_control_stat1
    .global megadev_control_stat2
    .global megadev_control_stat3
    .global megadev_control_trace

megadev_control_main_magic:
    .space 2
megadev_control_phase:
    .space 2
megadev_control_sub_flag:
    .space 2
megadev_control_stat0:
    .space 2
megadev_control_stat1:
    .space 2
megadev_control_stat2:
    .space 2
megadev_control_stat3:
    .space 2
megadev_control_trace:
    .space 2
