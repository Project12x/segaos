/*
 * common.h - GS1 Inter-CPU Communication Protocol
 *
 * Defines the command/status protocol used between Main CPU and Sub CPU
 * via the Gate Array communication registers.
 *
 * Protocol Overview:
 *   1. Main sets COMM_FLAG high byte (CFM) to signal a command.
 *   2. Main writes command parameters to CMD registers ($A12010-$A1201E).
 *   3. Sub reads CFM, processes the command.
 *   4. Sub writes results to STATUS registers ($FF8020-$FF802E).
 *   5. Sub sets COMM_FLAG low byte (CFS) to acknowledge.
 *   6. Main reads CFS and STATUS registers for the result.
 *   7. Main clears CFM to 0. Sub clears CFS to 0. Handshake complete.
 */

#ifndef COMMON_H
#define COMMON_H

#include "ga_regs.h"
#include <stdint.h>

/* ============================================================
 * Memory Sizes (Verified from Sega Mega-CD Hardware Manual)
 * ============================================================ */
#define PRG_RAM_SIZE 0x80000       /* 512KB (4 Mbit) total        */
#define WORD_RAM_SIZE 0x40000      /* 256KB (2 Mbit) total        */
#define WORD_RAM_2M_BANK 0x40000   /* 256KB - 2M mode (one CPU)   */
#define WORD_RAM_1M_BANK 0x20000   /* 128KB - 1M mode (per bank)  */
#define MAIN_WORK_RAM_SIZE 0x10000 /* 64KB Genesis Work RAM       */
#define BRAM_INTERNAL_SIZE 0x2000  /* 8KB internal backup RAM     */

/* ============================================================
 * Command Codes (Main -> Sub, via CFM flag byte)
 *
 * The CFM byte is the "opcode". CMD registers carry parameters.
 * ============================================================ */
#define CMD_NONE 0x00         /* Idle / No command               */
#define CMD_BOOT 0x01         /* Sub CPU has booted, awaiting OS */
#define CMD_INIT_OS 0x02      /* Initialize OS subsystems        */
#define CMD_RENDER_FRAME 0x10 /* Render the current dirty rects  */
#define CMD_WRAM_SWAP 0x11    /* Request Word RAM bank swap      */
#define CMD_OPEN_WINDOW 0x20  /* Open a new window               */
#define CMD_CLOSE_WINDOW 0x21 /* Close a window                  */
#define CMD_MOVE_WINDOW 0x22  /* Move/drag a window              */
#define CMD_DRAW_TEXT 0x30    /* Draw text string                */
#define CMD_DRAW_ICON 0x31    /* Draw icon bitmap                */
#define CMD_CD_PLAY 0x40      /* Play CD audio track             */
#define CMD_CD_STOP 0x41      /* Stop CD audio                   */
#define CMD_FILE_READ 0x50    /* Read file from CD               */
#define CMD_FILE_WRITE 0x51   /* Write file to Backup RAM        */
#define CMD_MOUSE_EVENT 0x60  /* Mouse input event from Main CPU */

/* ============================================================
 * Status Codes (Sub -> Main, via CFS flag byte)
 * ============================================================ */
#define STATUS_IDLE 0x00  /* Sub is idle, ready for commands */
#define STATUS_BUSY 0x01  /* Sub is processing              */
#define STATUS_ACK 0x02   /* Command acknowledged           */
#define STATUS_DONE 0x03  /* Command complete, result ready */
#define STATUS_ERROR 0xFF /* Error occurred                 */

/* ============================================================
 * Sub CPU State Machine
 * (Stored in STATUS register 0 for Main CPU to monitor)
 * ============================================================ */
typedef enum {
  SUB_STATE_RESET = 0,
  SUB_STATE_BOOTING = 1,
  SUB_STATE_READY = 2,
  SUB_STATE_RENDERING = 3,
  SUB_STATE_CRASHED = 0xFF
} SubCPUState;

/* ============================================================
 * Communication Helpers
 *
 * These functions implement the handshake protocol.
 * They busy-wait, which is fine for cooperative multitasking.
 * ============================================================ */

/* --- Main CPU Side --- */
#ifdef MAIN_CPU

/* Send a command to the Sub CPU and wait for acknowledgement */
static inline void main_send_cmd(uint8_t cmd, uint16_t p0, uint16_t p1,
                                 uint16_t p2, uint16_t p3) {
  /* Wait for Sub to be idle */
  while (GA_MAIN_READ_SUB_FLAG() != STATUS_IDLE) {
  }

  /* Write parameters to CMD registers */
  GA_MAIN_REG16(GA_COMM_CMD0) = p0;
  GA_MAIN_REG16(GA_COMM_CMD1) = p1;
  GA_MAIN_REG16(GA_COMM_CMD2) = p2;
  GA_MAIN_REG16(GA_COMM_CMD3) = p3;

  /* Set command flag (triggers Sub CPU to read) */
  GA_MAIN_SET_FLAG(cmd);
}

/* Wait for command completion */
static inline uint8_t main_wait_done(void) {
  /* Wait for Sub to signal DONE or ERROR */
  uint8_t status;
  do {
    status = GA_MAIN_READ_SUB_FLAG();
  } while (status != STATUS_DONE && status != STATUS_ERROR);

  /* Clear our flag to complete handshake */
  GA_MAIN_SET_FLAG(CMD_NONE);

  /* Wait for Sub to also clear */
  while (GA_MAIN_READ_SUB_FLAG() != STATUS_IDLE) {
  }

  return status;
}

/* Read a result word from STATUS registers */
static inline uint16_t main_read_result(uint8_t index) {
  return GA_MAIN_REG16(GA_COMM_STATUS0 + (index * 2));
}

/* Write a single parameter to a CMD register (for event streaming) */
static inline void main_send_param(uint8_t index, uint16_t value) {
  GA_MAIN_REG16(GA_COMM_CMD0 + (index * 2)) = value;
}

/* ---- Word RAM Bank Swap (Main CPU side) ---- */

/* Check if Main CPU currently has Word RAM bank access.
 * In 1M mode, DMNA=0 means Main has its bank. */
static inline uint8_t main_has_wram(void) {
  return !(GA_MAIN_REG16(GA_MEM_MODE) & MEM_MODE_DMNA);
}

/* Request Word RAM bank swap from Main CPU side.
 * Sets DMNA=1, waits for Sub CPU to give its bank via RET. */
static inline void main_request_swap(void) {
  uint16_t mem = GA_MAIN_REG16(GA_MEM_MODE);
  GA_MAIN_REG16(GA_MEM_MODE) = mem | MEM_MODE_DMNA;
  /* Wait until swap completes (DMNA clears after swap) */
  while (GA_MAIN_REG16(GA_MEM_MODE) & MEM_MODE_DMNA) {
  }
}

#endif /* MAIN_CPU */

/* --- Sub CPU Side --- */
#ifdef SUB_CPU

/* Wait for a command from the Main CPU. Returns the command byte. */
static inline uint8_t sub_wait_cmd(void) {
  uint8_t cmd;
  do {
    cmd = GA_SUB_READ_MAIN_FLAG();
  } while (cmd == CMD_NONE);
  return cmd;
}

/* Read a parameter word from CMD registers */
static inline uint16_t sub_read_param(uint8_t index) {
  return GA_SUB_REG16(GA_COMM_CMD0 + (index * 2));
}

/* Write a result word to STATUS registers */
static inline void sub_write_result(uint8_t index, uint16_t value) {
  GA_SUB_REG16(GA_COMM_STATUS0 + (index * 2)) = value;
}

/* Acknowledge command and signal busy */
static inline void sub_ack(void) { GA_SUB_SET_FLAG(STATUS_BUSY); }

/* Signal command completion */
static inline void sub_done(void) {
  GA_SUB_SET_FLAG(STATUS_DONE);

  /* Wait for Main to clear its flag */
  while (GA_SUB_READ_MAIN_FLAG() != CMD_NONE) {
  }

  /* Clear our flag */
  GA_SUB_SET_FLAG(STATUS_IDLE);
}

/* Signal error */
static inline void sub_error(void) {
  GA_SUB_SET_FLAG(STATUS_ERROR);

  /* Wait for Main to clear its flag */
  while (GA_SUB_READ_MAIN_FLAG() != CMD_NONE) {
  }

  GA_SUB_SET_FLAG(STATUS_IDLE);
}

/* ---- Word RAM Bank Swap (Sub CPU side) ---- */

/* Return the Sub CPU's Word RAM bank to the Main CPU.
 * Sets RET=1 in GA_MEM_MODE. The hardware swaps banks and
 * clears RET automatically. Sub CPU gets the other bank. */
static inline void sub_return_wram(void) {
  uint16_t mem = GA_SUB_REG16(GA_MEM_MODE);
  GA_SUB_REG16(GA_MEM_MODE) = mem | MEM_MODE_RET;
  /* Wait for swap to complete (RET clears after swap) */
  while (GA_SUB_REG16(GA_MEM_MODE) & MEM_MODE_RET) {
  }
}

/* Check if Sub CPU currently has Word RAM bank access.
 * In 1M mode, RET=0 means Sub has its bank. */
static inline uint8_t sub_has_wram(void) {
  return !(GA_SUB_REG16(GA_MEM_MODE) & MEM_MODE_RET);
}

#endif /* SUB_CPU */

#endif /* COMMON_H */
