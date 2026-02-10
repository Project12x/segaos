/*
 * main.c - Main CPU Entry Point (Genesis 68000 @ 7.6MHz)
 *
 * The Main CPU is the "I/O Processor":
 *   - Manages VDP (video display)
 *   - Polls controllers
 *   - Commands the Z80 sound driver
 *   - Transfers Word RAM -> VDP during VBlank
 *   - Sends commands to Sub CPU via Gate Array
 *
 * Built with SGDK (when configured) or standalone m68k-elf-gcc.
 */

#include "common.h"
#include "framebuffer.h"
#include "input.h"
#include "mouse.h"
#include "vdp.h"

/* Embedded Sub CPU program (linked from sub_cpu.bin via objcopy)
 * Symbol names include the path: build/sub_cpu.bin ->
 * _binary_build_sub_cpu_bin_* */
extern const uint8_t _binary_build_sub_cpu_bin_start[];
extern const uint8_t _binary_build_sub_cpu_bin_end[];
#define SUB_CPU_BIN_START _binary_build_sub_cpu_bin_start
#define SUB_CPU_BIN_END _binary_build_sub_cpu_bin_end

/* PRG-RAM window as seen by Main CPU ($420000, 128KB bank 0) */
#define PRG_RAM_WINDOW ((volatile uint8_t *)0x420000)
#define PRG_RAM_SP_OFFSET 0x6000 /* SP loads at $006000 in Sub CPU space */

/* Forward declarations */
static void boot_sequence(void);
static void main_loop(void);
static void load_sub_program(void);

int main(void) {
  boot_sequence();
  main_loop();
  return 0;
}

/*
 * Boot Sequence
 *
 * Reference: Megadev ip_sp.md, plutiedev, Sega CD Hardware Manual
 *
 * 1. Assert Sub CPU reset
 * 2. Request Sub CPU bus (SBRQ)
 * 3. Wait for bus grant
 * 4. Load Sub CPU program to PRG-RAM
 * 5. Set memory mode to 1M
 * 6. Release Sub CPU from reset
 * 7. Wait for Sub CPU to signal ready
 */
static void boot_sequence(void) {
  /* Step 1: Assert Sub CPU reset */
  GA_MAIN_REG16(GA_RESET) = 0x0000;

  /* Step 2: Request Sub CPU bus (halt Sub CPU) */
  GA_MAIN_REG16(GA_RESET) = RESET_SBRQ;

  /* Step 3: Wait for bus grant */
  while (!(GA_MAIN_REG16(GA_RESET) & RESET_SBRQ)) {
    /* Spin until Sub CPU bus is granted */
  }

  /* Step 4: Load Sub CPU program to PRG-RAM */
  load_sub_program();

  /* Step 5: Set Word RAM to 1M mode for double-buffering */
  {
    uint16_t mem = GA_MAIN_REG16(GA_MEM_MODE);
    mem |= MEM_MODE_1M;
    GA_MAIN_REG16(GA_MEM_MODE) = mem;
  }

  /* Step 6: Release Sub CPU from reset (clear SBRQ, set SRES) */
  GA_MAIN_REG16(GA_RESET) = RESET_SRES;

  /* Step 7: Wait for Sub CPU to boot and signal ready */
  while (GA_MAIN_READ_SUB_FLAG() != STATUS_IDLE) {
    /* Sub CPU is booting... */
  }

  /* Step 8: Verify Sub CPU state */
  {
    uint16_t sub_state = main_read_result(0);
    if (sub_state != SUB_STATE_READY) {
      /* Sub CPU failed to boot - halt */
      while (1) {
      }
    }
  }

  /* Step 9: Initialize Mega Mouse on controller port 1 */
  Mouse_Init(1);

  /* Step 10: Initialize VDP (standalone, no SGDK dependency) */
  VDP_Init();

  /* Step 11: Set up framebuffer tilemap + palette */
  FB_Init();
}

/*
 * Load the Sub CPU program binary into PRG-RAM.
 *
 * The Main CPU accesses PRG-RAM via a 128KB window at $420000.
 * PRG-RAM bank 0 maps Sub CPU addresses $000000-$01FFFF.
 * The SP header goes at offset $6000 within this bank.
 *
 * NOTE: The GA_MEM_MODE register bits 8-15 (WP) control which
 * PRG-RAM bank is mapped. Bank 0 is the default.
 */
static void load_sub_program(void) {
  const uint8_t *src = SUB_CPU_BIN_START;
  const uint8_t *end = SUB_CPU_BIN_END;
  volatile uint8_t *dst = PRG_RAM_WINDOW + PRG_RAM_SP_OFFSET;

  /* Copy Sub CPU program to PRG-RAM at SP offset */
  while (src < end) {
    *dst++ = *src++;
  }
}

static void main_loop(void) {
  while (1) {
    /* Wait for VBlank */
    VDP_WaitVSync();

    /* Poll Mega Mouse and forward input to Sub CPU */
    if (Mouse_Poll()) {
      Input_SendMouseEvent();
    }

    /* Request Sub CPU to render the current frame.
     * The Sub CPU will:
     *   1. Process dirty rects, draw windows, cursor
     *   2. Call sub_return_wram() to swap banks
     *   3. Signal DONE
     * After this returns, our Word RAM bank contains
     * the finished framebuffer. */
    main_send_cmd(CMD_RENDER_FRAME, 0, 0, 320, 224);
    main_wait_done();

    /* Convert the finished framebuffer from linear 4bpp
     * to VDP tile format and DMA to VRAM.
     * Main CPU sees its Word RAM bank at $200000 in 1M mode. */
    FB_UpdateFrame(WRAM_BANK0_MAIN);
  }
}
