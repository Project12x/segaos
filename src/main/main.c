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
#include "boot_probe.h"
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
#ifndef VDP_TEXT_PROBE
static void boot_sequence(void);
static uint8_t wait_for_sub_ready(void);
#endif
void main_enable_interrupts(void);
void probe_bios_clear_comm(void);
#ifdef SUB_RUNTIME_SMOKE
void segaos_runtime_smoke_halt(void) __attribute__((noinline, used));
static void runtime_smoke_probe(void);
static void runtime_smoke_capture_status(void);

volatile uint16_t segaos_smoke_main_phase;
volatile uint16_t segaos_smoke_sub_flag;
volatile uint16_t segaos_smoke_stat0;
volatile uint16_t segaos_smoke_stat1;
volatile uint16_t segaos_smoke_stat2;
volatile uint16_t segaos_smoke_stat3;
volatile uint16_t segaos_smoke_trace;
volatile uint16_t segaos_smoke_done_status;
#endif
#ifdef DESKTOP_INIT_PROBE
void segaos_desktop_init_halt(void) __attribute__((noinline, used));
static void desktop_init_probe(void);
static void desktop_init_capture_status(void);
static uint8_t desktop_probe_wait_done(uint32_t poll_limit);

volatile uint16_t segaos_desktop_main_phase;
volatile uint16_t segaos_desktop_sub_flag;
volatile uint16_t segaos_desktop_stat0;
volatile uint16_t segaos_desktop_stat1;
volatile uint16_t segaos_desktop_stat2;
volatile uint16_t segaos_desktop_stat3;
volatile uint16_t segaos_desktop_stat5;
volatile uint16_t segaos_desktop_stat6;
volatile uint16_t segaos_desktop_trace;
volatile uint16_t segaos_desktop_main_flag;
volatile uint16_t segaos_desktop_ready_sub_flag;
volatile uint16_t segaos_desktop_ready_stat0;
volatile uint16_t segaos_desktop_ready_stat1;
volatile uint16_t segaos_desktop_ready_trace;
volatile uint16_t segaos_desktop_done_status;
volatile uint16_t segaos_desktop_render_status;
volatile uint16_t segaos_desktop_render_trace;
volatile uint16_t segaos_desktop_mem_mode_before;
volatile uint16_t segaos_desktop_mem_mode_after_return;
volatile uint16_t segaos_desktop_wait_polls;
volatile uint16_t segaos_desktop_text_probe_enabled;
volatile uint16_t segaos_desktop_text_wram_word0;
volatile uint16_t segaos_desktop_text_wram_word1;
volatile uint16_t segaos_desktop_text_vram_word0;
volatile uint16_t segaos_desktop_text_vram_word1;
volatile uint16_t segaos_desktop_text_wram_sig;
volatile uint16_t segaos_desktop_text_vram_sig;
volatile uint16_t segaos_desktop_text_plane_entry0;
volatile uint16_t segaos_desktop_text_plane_entry1;
volatile uint16_t segaos_desktop_text_plane_entry2;
volatile uint16_t segaos_desktop_title_probe_enabled;
volatile uint16_t segaos_desktop_title_wram_word0;
volatile uint16_t segaos_desktop_title_wram_word1;
volatile uint16_t segaos_desktop_title_vram_word0;
volatile uint16_t segaos_desktop_title_vram_word1;
#endif
#ifdef VDP_TEXT_PROBE
void segaos_vdp_text_probe(void);
#endif
#if !defined(BOOT_PROBE) && !defined(SUB_RUNTIME_SMOKE) &&                 \
    !defined(DESKTOP_INIT_PROBE) && !defined(VDP_TEXT_PROBE)
static void main_loop(void);
#endif
#ifdef BOOT_PROBE
void segaos_probe_halt(void) __attribute__((noinline, used));
void probe_enable_main_interrupts(void);
static void boot_probe(void);
static void probe_capture_boot_failure(void);
static void probe_sample_prg_ram(void);
static void probe_install_bios_vblank(void);

volatile uint16_t segaos_probe_main_magic;
volatile uint16_t segaos_probe_main_phase;
volatile uint16_t segaos_probe_sub_state;
volatile uint16_t segaos_probe_sub_ready_magic;
volatile uint16_t segaos_probe_comm_status;
volatile uint16_t segaos_probe_sub_result0;
volatile uint16_t segaos_probe_sub_result1;
volatile uint16_t segaos_probe_sub_result2;
volatile uint16_t segaos_probe_sub_result3;
volatile uint16_t segaos_probe_sub_result4;
volatile uint16_t segaos_probe_sub_result5;
volatile uint16_t segaos_probe_sub_result6;
volatile uint16_t segaos_probe_sub_trace;
volatile uint16_t segaos_probe_wram_word0;
volatile uint16_t segaos_probe_wram_word1;
volatile uint16_t segaos_probe_wram_bank1_word0;
volatile uint16_t segaos_probe_wram_bank1_word1;
volatile uint16_t segaos_probe_wram_mcd1_bank0_word0;
volatile uint16_t segaos_probe_wram_mcd1_bank0_word1;
volatile uint16_t segaos_probe_wram_mcd1_bank1_word0;
volatile uint16_t segaos_probe_wram_mcd1_bank1_word1;
volatile uint16_t segaos_probe_wram_survey;
volatile uint16_t segaos_probe_mem_mode_before;
volatile uint16_t segaos_probe_mem_mode_after;
volatile uint16_t segaos_probe_mem_mode_after_wram;
volatile uint16_t segaos_probe_fb_wram_row0_word0;
volatile uint16_t segaos_probe_fb_wram_row0_word1;
volatile uint16_t segaos_probe_fb_wram_row1_word0;
volatile uint16_t segaos_probe_fb_wram_row1_word1;
volatile uint16_t segaos_probe_fb_vram_word0;
volatile uint16_t segaos_probe_fb_vram_word1;
volatile uint16_t segaos_probe_fb_vram_word2;
volatile uint16_t segaos_probe_fb_vram_word3;
volatile uint16_t segaos_probe_reset_before;
volatile uint16_t segaos_probe_reset_after;
volatile uint16_t segaos_probe_reset_ack_polls;
volatile uint16_t segaos_probe_mem_mode_diag;
volatile uint16_t segaos_probe_prg020000_b0;
volatile uint16_t segaos_probe_prg020000_b1;
volatile uint16_t segaos_probe_prg020000_b2;
volatile uint16_t segaos_probe_prg020000_b3;
volatile uint16_t segaos_probe_prg420000_b0;
volatile uint16_t segaos_probe_prg420000_b1;
volatile uint16_t segaos_probe_prg420000_b2;
volatile uint16_t segaos_probe_prg420000_b3;
volatile uint16_t segaos_probe_prg020000_b0_next;
volatile uint16_t segaos_probe_prg420000_b0_next;
volatile uint16_t segaos_probe_usercall0_op;
volatile uint16_t segaos_probe_usercall0_addr_hi;
volatile uint16_t segaos_probe_usercall0_addr_lo;
volatile uint16_t segaos_probe_usercall1_op;
volatile uint16_t segaos_probe_usercall1_addr_hi;
volatile uint16_t segaos_probe_usercall1_addr_lo;
volatile uint16_t segaos_probe_sub_bootstat;
volatile uint16_t segaos_probe_sub_int2flag;
volatile uint16_t segaos_probe_sub_usermode;
volatile uint16_t segaos_probe_exvec_level6_hi;
volatile uint16_t segaos_probe_exvec_level6_lo;
volatile uint16_t segaos_probe_vblank_flags;
volatile uint16_t segaos_probe_vblank_counter;
#endif

#define SUB_READY_FRAME_LIMIT 4096U
#define SUB_READY_POLLS_PER_FRAME 512U

#define PROBE_PRG_ALIAS_LOW 0x020000UL
#define PROBE_PRG_ALIAS_HIGH 0x420000UL
#define PROBE_SP_LOAD_OFFSET 0x006000UL
#define PROBE_SUB_BOOTSTAT 0x005EA0UL
#define PROBE_SUB_INT2FLAG 0x005EA4UL
#define PROBE_SUB_USERMODE 0x005EA6UL
#define PROBE_SUB_USERCALL0 0x005F28UL
#define PROBE_SUB_USERCALL1 0x005F2EUL
#define PROBE_MAIN_EXVEC_LEVEL6 0x00FFFD08UL
#define PROBE_MAIN_BIOS_VBLANK_HANDLER 0x00000290UL
#define PROBE_MAIN_BIOS_VBLANK_FLAGS 0x00FFFE26UL
#define PROBE_MAIN_BIOS_VBLANK_COUNTER 0x00FFFE27UL
#define PROBE_PRG_BANK_MASK 0x00C0U
#define PROBE_PRG_BANK_SHIFT 6U
#define PROBE_READY_POLL_LIMIT 0x00100000UL
#define PROBE_SBRQ_ACK_LIMIT 4096U

int main(void) {
#ifdef VDP_TEXT_PROBE
  segaos_vdp_text_probe();
#else
  boot_sequence();
#ifdef SUB_RUNTIME_SMOKE
  runtime_smoke_probe();
#elif defined(DESKTOP_INIT_PROBE)
  desktop_init_probe();
#elif defined(BOOT_PROBE)
  boot_probe();
#else
  main_loop();
#endif
#endif
  return 0;
}

#ifndef VDP_TEXT_PROBE
/*
 * Boot Sequence (BIOS has already loaded SP and started Sub CPU)
 *
 * 1. Set Word RAM to 1M mode for the verified boot framebuffer bank
 * 2. Wait for Sub CPU to signal ready
 * 3. Initialize peripherals (mouse, VDP, framebuffer)
 */
static void boot_sequence(void) {
#ifdef BOOT_PROBE
  segaos_probe_main_magic = PROBE_MAIN_MAGIC;
  segaos_probe_main_phase = PROBE_PHASE_MAIN_STARTED;
  /*
   * The probe must observe Sub startup before Main-side initialization can
   * disturb communication registers or Word RAM ownership.
   */
  return;
#endif

#ifdef SUB_RUNTIME_SMOKE
  segaos_smoke_main_phase = 0x8001;
#endif

  GA_MAIN_SET_FLAG(CMD_NONE);

  /* Step 1: Wait for Sub CPU BIOS usercall startup before touching shared
   * display/Word RAM state. Keep BIOS VBlank/interrupt setup deferred until
   * after this gate; installing it here can disturb GA communication before
   * SegaOS owns the Sub command protocol. */
  if (!wait_for_sub_ready()) {
#ifdef SUB_RUNTIME_SMOKE
    runtime_smoke_capture_status();
    segaos_smoke_main_phase = 0x80fe;
    segaos_runtime_smoke_halt();
#else
    /* Sub CPU failed to boot - halt */
    while (1) {
    }
#endif
  }

#ifdef SUB_RUNTIME_SMOKE
  runtime_smoke_capture_status();
  segaos_smoke_main_phase = 0x8002;
#endif

  /* Step 2: Set Word RAM to 1M mode for the verified boot framebuffer bank */
  {
    uint16_t mem = GA_MAIN_REG16(GA_MEM_MODE);
    mem |= MEM_MODE_1M;
    GA_MAIN_REG16(GA_MEM_MODE) = mem;
  }

  /* Step 3: Initialize Mega Mouse on controller port 1 */
  Mouse_Init(1);

  /* Step 4: Initialize VDP (standalone, no SGDK dependency) */
  VDP_Init();

#ifdef BOOT_PROBE
  /*
   * Do not clear communication registers here. The Sub CPU may already have
   * reported its boot state through COMSTAT by the time Main IP reaches this
   * point, and clearing them turns a successful Sub boot into a false failure.
   */
  probe_install_bios_vblank();
#endif

  /* Step 5: Set up framebuffer tilemap + palette */
  FB_Init();
  FB_ShowBootPattern();

#if !defined(BOOT_PROBE) && !defined(BOOT_SAFE_DESKTOP)
  /* Step 6: Initialize Sub-side OS/rendering now that Word RAM and the Main
   * display path are ready. */
  main_send_cmd(CMD_INIT_OS, 0, 0, 0, 0);
  main_wait_done();
  FB_UpdateFrame(WRAM_BANK0_MAIN);
  main_return_wram_to_sub();
#elif defined(BOOT_SAFE_DESKTOP) && !defined(DESKTOP_INIT_PROBE) &&         \
    !defined(SUB_RUNTIME_SMOKE)
  main_send_cmd(CMD_RENDER_FRAME, 0, 0, 320, 224);
  main_wait_done();
  FB_UpdateFrame(WRAM_BANK0_MAIN);
  VDP_WaitDMA();
  main_return_wram_to_sub();
#endif
}

#ifdef SUB_RUNTIME_SMOKE
static void runtime_smoke_capture_status(void) {
  segaos_smoke_sub_flag = GA_MAIN_READ_SUB_FLAG();
  segaos_smoke_stat0 = main_read_result(0);
  segaos_smoke_stat1 = main_read_result(1);
  segaos_smoke_stat2 = main_read_result(2);
  segaos_smoke_stat3 = main_read_result(3);
  segaos_smoke_trace = main_read_result(7);
}

static void runtime_smoke_probe(void) {
  runtime_smoke_capture_status();
  if (segaos_smoke_sub_flag != STATUS_IDLE ||
      segaos_smoke_stat0 != SUB_STATE_READY ||
      segaos_smoke_stat1 != RUNTIME_SMOKE_READY_MAGIC) {
    segaos_smoke_main_phase = 0x80fd;
    segaos_runtime_smoke_halt();
  }

  main_send_cmd(CMD_BOOT_PROBE, 0x1357, 0x2468, 0, 0);
  segaos_smoke_main_phase = 0x8003;
  segaos_smoke_done_status = main_wait_done();
  runtime_smoke_capture_status();

  if (segaos_smoke_done_status == STATUS_DONE &&
      segaos_smoke_stat0 == RUNTIME_SMOKE_DONE_MAGIC &&
      segaos_smoke_stat1 == CMD_BOOT_PROBE &&
      segaos_smoke_trace == 0x72fe) {
    segaos_smoke_main_phase = 0x80ff;
  } else {
    segaos_smoke_main_phase = 0x80fc;
  }

  segaos_runtime_smoke_halt();
}

void segaos_runtime_smoke_halt(void) {
  while (1) {
  }
}
#endif

#ifdef DESKTOP_INIT_PROBE
#define DESKTOP_PROBE_WAIT_LIMIT 0x00100000UL
#define DESKTOP_TEXT_PROBE_X 64U
#define DESKTOP_TEXT_PROBE_Y 80U
#define DESKTOP_TEXT_PROBE_ROW 3U
#define DESKTOP_TEXT_PROBE_TILE_X (DESKTOP_TEXT_PROBE_X / 8U)
#define DESKTOP_TEXT_PROBE_SCALE 3U
#define DESKTOP_TEXT_PROBE_ROWS (8U * DESKTOP_TEXT_PROBE_SCALE)
#define DESKTOP_TEXT_PROBE_WORDS_PER_ROW 6U
#define DESKTOP_TEXT_PROBE_SIG_SEED 0x5347U
#define DESKTOP_TITLE_PROBE_X 113U
#define DESKTOP_TITLE_PROBE_Y 44U
#define DESKTOP_TITLE_PROBE_ROW 1U
#define DESKTOP_TITLE_PROBE_BYTE_OFFSET                                          \
  (((DESKTOP_TITLE_PROBE_Y + DESKTOP_TITLE_PROBE_ROW) * FB_LINEAR_BPR) +       \
   (DESKTOP_TITLE_PROBE_X / 2U))
#define DESKTOP_TITLE_PROBE_TILE_X (DESKTOP_TITLE_PROBE_X / 8U)
#define DESKTOP_TITLE_PROBE_TILE_Y                                             \
  ((DESKTOP_TITLE_PROBE_Y + DESKTOP_TITLE_PROBE_ROW) / 8U)
#define DESKTOP_TITLE_PROBE_TILE_ROW                                           \
  ((DESKTOP_TITLE_PROBE_Y + DESKTOP_TITLE_PROBE_ROW) & 7U)
#define DESKTOP_TITLE_PROBE_VRAM_OFFSET                                        \
  ((((DESKTOP_TITLE_PROBE_TILE_Y * FB_TILES_X) + DESKTOP_TITLE_PROBE_TILE_X) * \
    FB_BYTES_PER_TILE) +                                                       \
   (DESKTOP_TITLE_PROBE_TILE_ROW * 4U))

#ifdef BOOT_SAFE_TEXT_PROBE
static uint16_t desktop_text_read_wram_word(uint16_t row, uint16_t word_index) {
  uint16_t y = (uint16_t)(DESKTOP_TEXT_PROBE_Y + row);
  uint32_t byte_offset =
      (((uint32_t)y << 7) + ((uint32_t)y << 5) +
       (DESKTOP_TEXT_PROBE_X >> 1) + ((uint32_t)word_index << 1));
  volatile const uint16_t *word =
      (volatile const uint16_t *)(WRAM_BANK0_MAIN + byte_offset);

  return word[0];
}

static uint16_t desktop_text_read_vram_word(uint16_t row, uint16_t word_index) {
  uint16_t tile_x =
      (uint16_t)(DESKTOP_TEXT_PROBE_TILE_X + (word_index >> 1));
  uint16_t tile_y = (uint16_t)((DESKTOP_TEXT_PROBE_Y + row) >> 3);
  uint16_t tile_row = (uint16_t)((DESKTOP_TEXT_PROBE_Y + row) & 7U);
  uint16_t word_in_tile = (uint16_t)(word_index & 1U);
  uint16_t vram_offset =
      (uint16_t)((((tile_y * FB_TILES_X) + tile_x) * FB_BYTES_PER_TILE) +
                 (tile_row << 2) + (word_in_tile << 1));

  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_READ(vram_offset);
  return VDP_DATA_PORT;
}

static uint16_t desktop_text_mix_sig(uint16_t sig, uint16_t word) {
  return (uint16_t)((uint16_t)((sig << 5) | (sig >> 11)) ^ word);
}

static uint16_t desktop_text_read_plane_entry(uint16_t tile_offset) {
  uint16_t plane_offset =
      (uint16_t)(VRAM_PLANE_A +
                 ((((DESKTOP_TEXT_PROBE_Y >> 3) * 64U) +
                   DESKTOP_TEXT_PROBE_TILE_X + tile_offset)
                  << 1));

  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_READ(plane_offset);
  return VDP_DATA_PORT;
}

static void desktop_text_capture_wram(void) {
  uint16_t row;
  uint16_t word_index;
  uint16_t sig = DESKTOP_TEXT_PROBE_SIG_SEED;

  segaos_desktop_text_wram_word0 =
      desktop_text_read_wram_word(DESKTOP_TEXT_PROBE_ROW, 0);
  segaos_desktop_text_wram_word1 =
      desktop_text_read_wram_word(DESKTOP_TEXT_PROBE_ROW, 1);

  for (row = 0; row < DESKTOP_TEXT_PROBE_ROWS; row++) {
    for (word_index = 0; word_index < DESKTOP_TEXT_PROBE_WORDS_PER_ROW;
         word_index++) {
      uint16_t actual = desktop_text_read_wram_word(row, word_index);
      sig = desktop_text_mix_sig(sig, actual);
    }
  }
  segaos_desktop_text_wram_sig = sig;
}

static void desktop_text_capture_vram(void) {
  uint16_t row;
  uint16_t word_index;
  uint16_t sig = DESKTOP_TEXT_PROBE_SIG_SEED;

  segaos_desktop_text_vram_word0 =
      desktop_text_read_vram_word(DESKTOP_TEXT_PROBE_ROW, 0);
  segaos_desktop_text_vram_word1 =
      desktop_text_read_vram_word(DESKTOP_TEXT_PROBE_ROW, 1);
  segaos_desktop_text_plane_entry0 = desktop_text_read_plane_entry(0);
  segaos_desktop_text_plane_entry1 = desktop_text_read_plane_entry(1);
  segaos_desktop_text_plane_entry2 = desktop_text_read_plane_entry(2);

  for (row = 0; row < DESKTOP_TEXT_PROBE_ROWS; row++) {
    for (word_index = 0; word_index < DESKTOP_TEXT_PROBE_WORDS_PER_ROW;
         word_index++) {
      uint16_t actual = desktop_text_read_vram_word(row, word_index);
      sig = desktop_text_mix_sig(sig, actual);
    }
  }
  segaos_desktop_text_vram_sig = sig;
}
#endif

static void desktop_init_capture_status(void) {
  segaos_desktop_main_flag = GA_MAIN_REG8(GA_COMM_FLAG);
  segaos_desktop_sub_flag = GA_MAIN_READ_SUB_FLAG();
  segaos_desktop_stat0 = main_read_result(0);
  segaos_desktop_stat1 = main_read_result(1);
  segaos_desktop_stat2 = main_read_result(2);
  segaos_desktop_stat3 = main_read_result(3);
  segaos_desktop_stat5 = main_read_result(5);
  segaos_desktop_stat6 = main_read_result(6);
  segaos_desktop_trace = main_read_result(7);
}

static uint8_t desktop_probe_wait_done(uint32_t poll_limit) {
  uint32_t polls;
  uint8_t status;

  for (polls = 0; polls < poll_limit; polls++) {
    status = GA_MAIN_READ_SUB_FLAG();
    if (status == STATUS_DONE || status == STATUS_ERROR) {
      segaos_desktop_wait_polls = (uint16_t)polls;
      GA_MAIN_SET_FLAG(CMD_NONE);
      while (GA_MAIN_READ_SUB_FLAG() != STATUS_IDLE) {
      }
      return status;
    }
  }

  segaos_desktop_wait_polls = 0xffff;
  return 0xee;
}

static void desktop_init_probe(void) {
  segaos_desktop_main_phase = 0x8101;
  desktop_init_capture_status();
  segaos_desktop_ready_sub_flag = segaos_desktop_sub_flag;
  segaos_desktop_ready_stat0 = segaos_desktop_stat0;
  segaos_desktop_ready_stat1 = segaos_desktop_stat1;
  segaos_desktop_ready_trace = segaos_desktop_trace;
  if (segaos_desktop_sub_flag != STATUS_IDLE ||
      segaos_desktop_stat0 != SUB_STATE_READY) {
    segaos_desktop_main_phase = 0x81fd;
    segaos_desktop_init_halt();
  }

  segaos_desktop_mem_mode_before = GA_MAIN_REG16(GA_MEM_MODE);
  segaos_desktop_mem_mode_after_return = GA_MAIN_REG16(GA_MEM_MODE);

  main_send_cmd(CMD_RENDER_FRAME, 0, 0, 320, 224);
  segaos_desktop_main_phase = 0x8102;
  segaos_desktop_done_status =
      desktop_probe_wait_done(DESKTOP_PROBE_WAIT_LIMIT);
  desktop_init_capture_status();

  if (segaos_desktop_done_status != STATUS_DONE ||
      segaos_desktop_stat0 != SUB_STATE_READY ||
      segaos_desktop_trace != 0x7404) {
    segaos_desktop_main_phase = 0x81fe;
    segaos_desktop_init_halt();
  }

  segaos_desktop_render_status = segaos_desktop_done_status;
  segaos_desktop_render_trace = segaos_desktop_trace;
#ifdef BOOT_SAFE_TEXT_PROBE
  segaos_desktop_text_probe_enabled = 1;
  desktop_text_capture_wram();
#else
  segaos_desktop_text_probe_enabled = 0;
  segaos_desktop_text_wram_word0 = 0;
  segaos_desktop_text_wram_word1 = 0;
  segaos_desktop_text_vram_word0 = 0;
  segaos_desktop_text_vram_word1 = 0;
  segaos_desktop_text_wram_sig = 0;
  segaos_desktop_text_vram_sig = 0;
  segaos_desktop_text_plane_entry0 = 0;
  segaos_desktop_text_plane_entry1 = 0;
  segaos_desktop_text_plane_entry2 = 0;
#endif
#ifdef BOOT_SAFE_TITLE_PROBE
  {
    volatile const uint16_t *title_wram =
        (volatile const uint16_t *)(WRAM_BANK0_MAIN +
                                    DESKTOP_TITLE_PROBE_BYTE_OFFSET);
    segaos_desktop_title_probe_enabled = 1;
    segaos_desktop_title_wram_word0 = title_wram[0];
    segaos_desktop_title_wram_word1 = title_wram[1];
  }
#else
  segaos_desktop_title_probe_enabled = 0;
  segaos_desktop_title_wram_word0 = 0;
  segaos_desktop_title_wram_word1 = 0;
  segaos_desktop_title_vram_word0 = 0;
  segaos_desktop_title_vram_word1 = 0;
#endif
  FB_UpdateFrame(WRAM_BANK0_MAIN);
  VDP_WaitDMA();
#ifdef BOOT_SAFE_TEXT_PROBE
  desktop_text_capture_vram();
#endif
#ifdef BOOT_SAFE_TITLE_PROBE
  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_READ(DESKTOP_TITLE_PROBE_VRAM_OFFSET);
  segaos_desktop_title_vram_word0 = VDP_DATA_PORT;
  segaos_desktop_title_vram_word1 = VDP_DATA_PORT;
#endif
  main_return_wram_to_sub();
  segaos_desktop_main_phase = 0x81ff;

  segaos_desktop_init_halt();
}

void segaos_desktop_init_halt(void) {
  while (1) {
  }
}
#endif

static uint8_t wait_for_sub_ready(void) {
  uint16_t frame;
  uint16_t polls;

  for (frame = 0; frame < SUB_READY_FRAME_LIMIT; frame++) {
    for (polls = 0; polls < SUB_READY_POLLS_PER_FRAME; polls++) {
      if (GA_MAIN_READ_SUB_FLAG() == STATUS_IDLE &&
          main_read_result(0) == SUB_STATE_READY) {
        return 1;
      }
    }
  }

  return 0;
}
#endif

#ifdef BOOT_PROBE
static void boot_probe(void) {
  volatile const uint16_t *wram0 = (volatile const uint16_t *)WRAM_BANK0_MAIN;
  volatile const uint16_t *wram1 = (volatile const uint16_t *)WRAM_BANK1_MAIN;
  volatile const uint16_t *wram_mcd1_0 = (volatile const uint16_t *)0x600000UL;
  volatile const uint16_t *wram_mcd1_1 = (volatile const uint16_t *)0x620000UL;
  volatile const uint16_t *fb_row1 =
      (volatile const uint16_t *)(WRAM_BANK0_MAIN + FB_LINEAR_BPR);
  uint32_t ready_polls;

  segaos_probe_main_magic = PROBE_MAIN_MAGIC;
  segaos_probe_main_phase = PROBE_PHASE_MAIN_STARTED;
#ifdef BOOT_PROBE_FRAMEBUFFER
  segaos_probe_wram_survey = 3;
#endif

  for (ready_polls = 0; ready_polls < PROBE_READY_POLL_LIMIT; ready_polls++) {
    segaos_probe_sub_state = main_read_result(0);
    segaos_probe_sub_ready_magic = main_read_result(1);
    segaos_probe_sub_trace = main_read_result(7);
    if (segaos_probe_sub_state == SUB_STATE_READY &&
        segaos_probe_sub_ready_magic == PROBE_READY_MAGIC) {
      break;
    }
  }

  if (segaos_probe_sub_state != SUB_STATE_READY ||
      segaos_probe_sub_ready_magic != PROBE_READY_MAGIC) {
    probe_capture_boot_failure();
    segaos_probe_halt();
  }

  segaos_probe_main_phase = PROBE_PHASE_SUB_READY;
  segaos_probe_mem_mode_before = GA_MAIN_REG16(GA_MEM_MODE);
  GA_MAIN_REG16(GA_MEM_MODE) = segaos_probe_mem_mode_before | MEM_MODE_1M;

  main_send_cmd(CMD_BOOT_PROBE, PROBE_WORD0, PROBE_WORD1,
                segaos_probe_wram_survey, 0);
  segaos_probe_main_phase = PROBE_PHASE_COMMAND_SENT;
  segaos_probe_comm_status = main_wait_done();

  segaos_probe_sub_result0 = main_read_result(0);
  segaos_probe_sub_result1 = main_read_result(1);
  segaos_probe_sub_result2 = main_read_result(2);
  segaos_probe_sub_result3 = main_read_result(3);
  segaos_probe_sub_result4 = main_read_result(4);
  segaos_probe_sub_result5 = main_read_result(5);
  segaos_probe_sub_result6 = main_read_result(6);
  segaos_probe_sub_trace = main_read_result(7);
  segaos_probe_mem_mode_after = GA_MAIN_REG16(GA_MEM_MODE);
  segaos_probe_main_phase = PROBE_PHASE_COMMAND_DONE;

  segaos_probe_wram_word0 = wram0[0];
  segaos_probe_wram_word1 = wram0[1];
  segaos_probe_wram_bank1_word0 = wram1[0];
  segaos_probe_wram_bank1_word1 = wram1[1];
  segaos_probe_wram_mcd1_bank0_word0 = wram_mcd1_0[0];
  segaos_probe_wram_mcd1_bank0_word1 = wram_mcd1_0[1];
  segaos_probe_wram_mcd1_bank1_word0 = wram_mcd1_1[0];
  segaos_probe_wram_mcd1_bank1_word1 = wram_mcd1_1[1];
  segaos_probe_mem_mode_after_wram = GA_MAIN_REG16(GA_MEM_MODE);

  if (segaos_probe_wram_survey == 3) {
    segaos_probe_fb_wram_row0_word0 = wram0[0];
    segaos_probe_fb_wram_row0_word1 = wram0[1];
    segaos_probe_fb_wram_row1_word0 = fb_row1[0];
    segaos_probe_fb_wram_row1_word1 = fb_row1[1];
    VDP_Init();
    FB_Init();
    FB_UpdateFrame(WRAM_BANK0_MAIN);
    VDP_WaitDMA();
    VDP_SET_REG(VDP_REG_AUTOINC, 2);
    VDP_VRAM_READ(0);
    segaos_probe_fb_vram_word0 = VDP_DATA_PORT;
    segaos_probe_fb_vram_word1 = VDP_DATA_PORT;
    segaos_probe_fb_vram_word2 = VDP_DATA_PORT;
    segaos_probe_fb_vram_word3 = VDP_DATA_PORT;
  }

  if (segaos_probe_comm_status == STATUS_DONE &&
      segaos_probe_sub_result0 == PROBE_DONE_MAGIC &&
      ((segaos_probe_wram_survey == 3 &&
        segaos_probe_fb_wram_row0_word0 == PROBE_FB_WORD0 &&
        segaos_probe_fb_wram_row0_word1 == PROBE_FB_WORD1 &&
        segaos_probe_fb_wram_row1_word0 == PROBE_FB_WORD2 &&
        segaos_probe_fb_wram_row1_word1 == PROBE_FB_WORD3) ||
       (segaos_probe_wram_word0 == PROBE_WORD0 &&
        segaos_probe_wram_word1 == PROBE_WORD1))) {
    segaos_probe_main_phase = PROBE_PHASE_DONE;
  } else {
    segaos_probe_main_phase = PROBE_PHASE_FAILED;
  }

  segaos_probe_halt();
}

static uint16_t probe_read_prg_word(uint32_t address) {
  return *(volatile const uint16_t *)address;
}

static void probe_install_bios_vblank(void) {
  volatile uint16_t *vblank_flags =
      (volatile uint16_t *)PROBE_MAIN_BIOS_VBLANK_FLAGS;
  volatile uint32_t *level6_vector =
      (volatile uint32_t *)PROBE_MAIN_EXVEC_LEVEL6;

  *vblank_flags = 0;
  *level6_vector = PROBE_MAIN_BIOS_VBLANK_HANDLER;

  segaos_probe_vblank_flags = *vblank_flags;
  segaos_probe_vblank_counter =
      *(volatile const uint8_t *)PROBE_MAIN_BIOS_VBLANK_COUNTER;
  segaos_probe_exvec_level6_hi =
      probe_read_prg_word(PROBE_MAIN_EXVEC_LEVEL6);
  segaos_probe_exvec_level6_lo =
      probe_read_prg_word(PROBE_MAIN_EXVEC_LEVEL6 + 2);

  probe_enable_main_interrupts();
}

static void probe_select_prg_bank(uint16_t bank) {
  uint16_t mem = GA_MAIN_REG16(GA_MEM_MODE);
  mem &= (uint16_t)~PROBE_PRG_BANK_MASK;
  mem |= (uint16_t)((bank & 0x3U) << PROBE_PRG_BANK_SHIFT);
  GA_MAIN_REG16(GA_MEM_MODE) = mem;

  /*
   * The PRG-RAM bank select is gate-array state, not normal RAM. Give the
   * emulator/hardware bus a few 68k reads before sampling the selected bank.
   */
  (void)GA_MAIN_REG16(GA_MEM_MODE);
  (void)GA_MAIN_REG16(GA_MEM_MODE);
}

static void probe_sample_prg_ram(void) {
  uint16_t original_mem_mode;
  uint16_t polls;

  original_mem_mode = GA_MAIN_REG16(GA_MEM_MODE);
  segaos_probe_reset_before = GA_MAIN_REG16(GA_RESET);

  GA_MAIN_REG8(GA_RESET + 1) = (uint8_t)(RESET_SRES | RESET_SBRQ);
  for (polls = 0; polls < PROBE_SBRQ_ACK_LIMIT; polls++) {
    if (GA_MAIN_REG8(GA_RESET + 1) & RESET_SBRQ) {
      break;
    }
  }

  segaos_probe_reset_ack_polls = polls;
  segaos_probe_reset_after = GA_MAIN_REG16(GA_RESET);

  probe_select_prg_bank(0);
  segaos_probe_prg020000_b0 =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SP_LOAD_OFFSET);
  segaos_probe_prg020000_b0_next =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SP_LOAD_OFFSET + 2);
  segaos_probe_prg420000_b0 =
      probe_read_prg_word(PROBE_PRG_ALIAS_HIGH + PROBE_SP_LOAD_OFFSET);
  segaos_probe_prg420000_b0_next =
      probe_read_prg_word(PROBE_PRG_ALIAS_HIGH + PROBE_SP_LOAD_OFFSET + 2);
  segaos_probe_usercall0_op =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_USERCALL0);
  segaos_probe_usercall0_addr_hi =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_USERCALL0 + 2);
  segaos_probe_usercall0_addr_lo =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_USERCALL0 + 4);
  segaos_probe_usercall1_op =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_USERCALL1);
  segaos_probe_usercall1_addr_hi =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_USERCALL1 + 2);
  segaos_probe_usercall1_addr_lo =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_USERCALL1 + 4);
  segaos_probe_sub_bootstat =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_BOOTSTAT);
  segaos_probe_sub_int2flag =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_INT2FLAG);
  segaos_probe_sub_usermode =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SUB_USERMODE);

  probe_select_prg_bank(1);
  segaos_probe_prg020000_b1 =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SP_LOAD_OFFSET);
  segaos_probe_prg420000_b1 =
      probe_read_prg_word(PROBE_PRG_ALIAS_HIGH + PROBE_SP_LOAD_OFFSET);

  probe_select_prg_bank(2);
  segaos_probe_prg020000_b2 =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SP_LOAD_OFFSET);
  segaos_probe_prg420000_b2 =
      probe_read_prg_word(PROBE_PRG_ALIAS_HIGH + PROBE_SP_LOAD_OFFSET);

  probe_select_prg_bank(3);
  segaos_probe_prg020000_b3 =
      probe_read_prg_word(PROBE_PRG_ALIAS_LOW + PROBE_SP_LOAD_OFFSET);
  segaos_probe_prg420000_b3 =
      probe_read_prg_word(PROBE_PRG_ALIAS_HIGH + PROBE_SP_LOAD_OFFSET);

  GA_MAIN_REG16(GA_MEM_MODE) = original_mem_mode;
  segaos_probe_mem_mode_diag = GA_MAIN_REG16(GA_MEM_MODE);
}

static void probe_capture_boot_failure(void) {
  segaos_probe_sub_state = main_read_result(0);
  segaos_probe_sub_ready_magic = main_read_result(1);
  segaos_probe_comm_status = GA_MAIN_READ_SUB_FLAG();
  segaos_probe_sub_trace = main_read_result(7);
  segaos_probe_vblank_counter =
      *(volatile const uint8_t *)PROBE_MAIN_BIOS_VBLANK_COUNTER;
  segaos_probe_main_phase = PROBE_PHASE_PRG_DIAG;
  probe_sample_prg_ram();
  segaos_probe_main_phase = PROBE_PHASE_FAILED;
}

void segaos_probe_halt(void) {
  while (1) {
  }
}
#endif

#if !defined(BOOT_PROBE) && !defined(SUB_RUNTIME_SMOKE) &&                 \
    !defined(DESKTOP_INIT_PROBE) && !defined(VDP_TEXT_PROBE)
static void main_loop(void) {
  while (1) {
    /* Wait for VBlank */
    VDP_WaitVSync();

#ifdef BOOT_SAFE_DESKTOP
    continue;
#endif

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
     * Main CPU sees returned bank 0 at $200000 in 1M mode. */
    FB_UpdateFrame(WRAM_BANK0_MAIN);

    /* Give bank 0 back to Sub before the next render command. This is the
     * first repeated-frame policy: single-bank ping-pong, not true
     * alternating double buffering. */
    main_return_wram_to_sub();
  }
}
#endif
