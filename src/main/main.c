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

/*
 * NOTE: The BIOS has already:
 *   - Loaded the SP (Sub CPU program) to PRG-RAM $006000
 *   - Started the Sub CPU (sp_init -> sub_init -> sp_main -> sub_main)
 *   - Loaded this IP to Work RAM $FF0000
 *   - Jumped to _entry (crt0.s)
 *
 * We do NOT need to manually reset the GA, halt the Sub CPU,
 * or copy the SP binary. The BIOS handles all of that.
 */

/* Forward declarations */
static void boot_sequence(void);
static void main_loop(void);

int main(void) {
  boot_sequence();
  main_loop();
  return 0;
}

/*
 * Boot Sequence (BIOS has already loaded SP and started Sub CPU)
 *
 * 1. Set Word RAM to 1M mode for double-buffering
 * 2. Wait for Sub CPU to signal ready
 * 3. Initialize peripherals (mouse, VDP, framebuffer)
 */
static void boot_sequence(void) {
  /* Step 1: Set Word RAM to 1M mode for double-buffering */
  {
    uint16_t mem = GA_MAIN_REG16(GA_MEM_MODE);
    mem |= MEM_MODE_1M;
    GA_MAIN_REG16(GA_MEM_MODE) = mem;
  }

  /* Step 2: Wait for Sub CPU to boot and signal ready */
  while (GA_MAIN_READ_SUB_FLAG() != STATUS_IDLE) {
    /* Sub CPU is booting... */
  }

  /* Step 3: Verify Sub CPU state */
  {
    uint16_t sub_state = main_read_result(0);
    if (sub_state != SUB_STATE_READY) {
      /* Sub CPU failed to boot - halt */
      while (1) {
      }
    }
  }

  /* Step 4: Initialize Mega Mouse on controller port 1 */
  Mouse_Init(1);

  /* Step 5: Initialize VDP (standalone, no SGDK dependency) */
  VDP_Init();

  /* Step 6: Set up framebuffer tilemap + palette */
  FB_Init();
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
