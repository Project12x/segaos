/*
 * bram_bios_68k.s - Raw Sega CD Sub BIOS Backup RAM vector calls.
 *
 * These routines are the only SegaOS code that calls the Sub BIOS $005F16
 * BURAM vector directly. The C adapter in bram_bios.c exposes them through
 * BramBiosOps so policy/UI code never depends on register-level BIOS details.
 */

    .text
    .align 2

    .global BRM_BiosCallInit
BRM_BiosCallInit:
    move.l  4(%sp), %a0         /* work buffer */
    move.l  8(%sp), %a1         /* display string buffer */
    move.w  #0x0000, %d0        /* BRMINIT */
    jsr     0x005F16
    bcs.s   .Linit_store
    move.w  #0x0003, %d1        /* carry clear means Sega-formatted */
.Linit_store:
    move.l  12(%sp), %a0        /* totalBlocks4K out */
    move.w  %d0, (%a0)
    move.l  16(%sp), %a0        /* statusCode out */
    move.w  %d1, (%a0)
    moveq   #1, %d0
    rts

    .global BRM_BiosCallStat
BRM_BiosCallStat:
    move.l  4(%sp), %a1         /* display string buffer */
    move.w  #0x0001, %d0        /* BRMSTAT */
    jsr     0x005F16
    move.l  8(%sp), %a0         /* freeBlocks4K out */
    move.w  %d0, (%a0)
    move.l  12(%sp), %a0        /* fileCount out */
    move.w  %d1, (%a0)
    moveq   #1, %d0
    rts

    .global BRM_BiosCallSearch
BRM_BiosCallSearch:
    move.l  4(%sp), %a0         /* filename */
    move.w  #0x0002, %d0        /* BRMSERCH */
    jsr     0x005F16
    bcs.s   .Lsearch_fail
    move.l  8(%sp), %a0         /* blocks out */
    move.w  %d0, (%a0)
    move.l  12(%sp), %a0        /* mode out */
    move.b  %d1, (%a0)
    moveq   #1, %d0
    rts
.Lsearch_fail:
    moveq   #0, %d0
    rts

    .global BRM_BiosCallRead
BRM_BiosCallRead:
    move.l  4(%sp), %a0         /* filename */
    move.l  8(%sp), %a1         /* destination buffer */
    move.w  #0x0003, %d0        /* BRMREAD */
    jsr     0x005F16
    bcs.s   .Lread_fail
    move.l  12(%sp), %a0        /* blocks out */
    move.w  %d0, (%a0)
    move.l  16(%sp), %a0        /* mode out */
    move.b  %d1, (%a0)
    moveq   #1, %d0
    rts
.Lread_fail:
    moveq   #0, %d0
    rts

    .global BRM_BiosCallWrite
BRM_BiosCallWrite:
    move.l  4(%sp), %a0         /* file info */
    move.l  8(%sp), %a1         /* source data */
    moveq   #0, %d1             /* Tech Bulletin #1: D1 must be zero */
    move.w  #0x0004, %d0        /* BRMWRITE */
    jsr     0x005F16
    bcs.s   .Lwrite_fail
    moveq   #1, %d0
    rts
.Lwrite_fail:
    moveq   #0, %d0
    rts

    .global BRM_BiosCallDir
BRM_BiosCallDir:
    move.l  4(%sp), %a0         /* pattern */
    move.l  8(%sp), %a1         /* directory buffer */
    moveq   #0, %d1
    move.w  14(%sp), %d1        /* skip low word from 32-bit arg slot */
    swap    %d1
    move.w  18(%sp), %d1        /* dirBytes low word from 32-bit arg slot */
    move.w  #0x0007, %d0        /* BRMDIR */
    jsr     0x005F16
    bcs.s   .Ldir_fail
    moveq   #1, %d0
    rts
.Ldir_fail:
    moveq   #0, %d0
    rts

    .end
