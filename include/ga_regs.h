/*
 * ga_regs.h - Sega CD Gate Array Register Definitions
 *
 * Verified against Sega Mega-CD Hardware Manual and community documentation.
 *
 * The Gate Array mediates ALL communication between the Main CPU (Genesis
 * 68000 @ 7.6MHz) and the Sub CPU (Sega CD 68000 @ 12.5MHz).
 *
 * Main CPU sees the GA at $A12000.
 * Sub CPU sees the GA at $FF8000.
 *
 * Key rule: Each CPU can only WRITE to its own set of registers.
 * Both CPUs can READ all registers.
 */

#ifndef GA_REGS_H
#define GA_REGS_H

#include <stdint.h>

/* ============================================================
 * Gate Array Base Addresses
 * ============================================================ */
#define GA_MAIN_BASE 0xA12000
#define GA_SUB_BASE 0xFF8000

/* ============================================================
 * Register Offsets (from base)
 * Verified from Sega Mega-CD Hardware Manual
 * ============================================================ */

#define GA_RESET 0x00 /* Sub CPU Reset/Halt (Main only) */
                      /*   bit 0: SRES (0=reset, 1=run)  */
                      /*   bit 1: SBRQ (1=halt sub CPU)  */

#define GA_MEM_MODE 0x02 /* Memory Mode / Write Protect     */
                         /*   bit 0: RET  (Sub returns WRAM)*/
                         /*   bit 1: DMNA (Main req swap)   */
                         /*   bit 2: MODE                   */
                         /*     0=2M (256KB, one CPU)       */
                         /*     1=1M (128KB x2, both CPUs)  */
                         /*   bits 8-15: Write protect      */

#define GA_CDC_MODE 0x04    /* CDC Mode / Device Destination   */
#define GA_HINT_VECTOR 0x06 /* H-INT vector (Main only)        */
#define GA_CDC_HOST 0x08    /* CDC Host Data                   */
#define GA_STOPWATCH 0x0C   /* Stopwatch (30.72 us/tick)       */

#define GA_COMM_FLAG 0x0E /* Communication Flags (16-bit)    */
                          /*   bits 15-8: CFM (Main writes)  */
                          /*   bits  7-0: CFS (Sub writes)   */

/* Main -> Sub Communication Data (16 bytes, 8 x 16-bit words)
 * Main CPU: writable at $A12010-$A1201F
 * Sub CPU:  read-only at $FF8010-$FF801F
 */
#define GA_COMM_CMD0 0x10
#define GA_COMM_CMD1 0x12
#define GA_COMM_CMD2 0x14
#define GA_COMM_CMD3 0x16
#define GA_COMM_CMD4 0x18
#define GA_COMM_CMD5 0x1A
#define GA_COMM_CMD6 0x1C
#define GA_COMM_CMD7 0x1E

/* Sub -> Main Communication Data (16 bytes, 8 x 16-bit words)
 * Sub CPU:  writable at $FF8020-$FF802F
 * Main CPU: read-only at $A12020-$A1202F
 */
#define GA_COMM_STATUS0 0x20
#define GA_COMM_STATUS1 0x22
#define GA_COMM_STATUS2 0x24
#define GA_COMM_STATUS3 0x26
#define GA_COMM_STATUS4 0x28
#define GA_COMM_STATUS5 0x2A
#define GA_COMM_STATUS6 0x2C
#define GA_COMM_STATUS7 0x2E

/* Timer (Sub CPU only) */
#define GA_TIMER 0x30

/* Interrupt Mask (Sub CPU only) */
#define GA_INT_MASK 0x32

/* ============================================================
 * ASIC / Graphics Processor Registers (Sub CPU only, $FF8058+)
 * ============================================================ */
#define GA_STAMP_SIZE 0x58 /* Stamp size/map config           */
                           /*   bit 15: GRON (1=ASIC busy)    */
                           /*   bit  2: SMS  (stamp map size) */
                           /*   bit  1: STS  (stamp size)     */
                           /*   bit  0: RPT  (repeat)         */

#define GA_STAMP_MAP_BASE 0x5A   /* Stamp map base address          */
#define GA_IMG_BUFFER_VSIZE 0x5C /* Image buffer V cell size        */
#define GA_IMG_BUFFER_START 0x5E /* Image buffer start address      */
#define GA_IMG_BUFFER_OFST 0x60  /* Image buffer offset             */
#define GA_IMG_BUFFER_HDOT 0x62  /* Image buffer H dot size         */
#define GA_IMG_BUFFER_VDOT 0x64  /* Image buffer V dot size         */
#define GA_TRACE_VECTOR 0x66     /* Trace vector base address       */
                                 /* Writing here STARTS the ASIC    */

/* ============================================================
 * Accessor Macros
 * ============================================================ */

/* Volatile 16-bit register read/write */
#define GA_MAIN_REG16(offset) (*(volatile uint16_t *)(GA_MAIN_BASE + (offset)))

#define GA_SUB_REG16(offset) (*(volatile uint16_t *)(GA_SUB_BASE + (offset)))

/* 8-bit access (for comm flags, each CPU writes one byte) */
#define GA_MAIN_REG8(offset) (*(volatile uint8_t *)(GA_MAIN_BASE + (offset)))

#define GA_SUB_REG8(offset) (*(volatile uint8_t *)(GA_SUB_BASE + (offset)))

/* ============================================================
 * Communication Flag Helpers
 *
 * COMM_FLAG at offset 0x0E is 16 bits:
 *   High byte (offset 0x0E): Main CPU flag (CFM). Main writes.
 *   Low byte  (offset 0x0F): Sub CPU flag  (CFS). Sub writes.
 * ============================================================ */

/* Main CPU sets its flag byte */
#define GA_MAIN_SET_FLAG(val) GA_MAIN_REG8(GA_COMM_FLAG) = (uint8_t)(val)

/* Sub CPU sets its flag byte */
#define GA_SUB_SET_FLAG(val) GA_SUB_REG8(GA_COMM_FLAG + 1) = (uint8_t)(val)

/* Main CPU reads Sub's flag */
#define GA_MAIN_READ_SUB_FLAG() GA_MAIN_REG8(GA_COMM_FLAG + 1)

/* Sub CPU reads Main's flag */
#define GA_SUB_READ_MAIN_FLAG() GA_SUB_REG8(GA_COMM_FLAG)

/* ============================================================
 * Word RAM Control Helpers
 * ============================================================ */

/* Memory mode bits */
#define MEM_MODE_RET 0x0001  /* Sub returns Word RAM bank       */
#define MEM_MODE_DMNA 0x0002 /* Main requests Word RAM swap     */
#define MEM_MODE_1M 0x0004   /* 0 = 2M (256KB to one CPU)       */
                             /* 1 = 1M (128KB per bank, both)   */

/* Reset register bits */
#define RESET_SRES 0x0001 /* 0 = assert reset, 1 = run       */
#define RESET_SBRQ 0x0002 /* 1 = halt Sub CPU bus             */

#endif /* GA_REGS_H */
