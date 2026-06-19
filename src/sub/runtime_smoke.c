/*
 * runtime_smoke.c - Minimal normal C-runtime Sub CPU control.
 *
 * This is intentionally separate from BOOT_PROBE. It proves whether the normal
 * SP header, BSS clear, C calls, and command loop execute without linking the
 * desktop kernel.
 */

#include "common.h"
#include <stdint.h>

#define SMOKE_READY_MAGIC 0x4343
#define SMOKE_DONE_MAGIC 0x534d

volatile uint16_t segaos_smoke_phase;
volatile uint16_t segaos_smoke_cmd;

void sub_init(void) {
  segaos_smoke_phase = 0x7101;
  sub_write_result(0, SUB_STATE_READY);
  sub_write_result(1, SMOKE_READY_MAGIC);
  sub_write_result(7, segaos_smoke_phase);
  GA_SUB_SET_FLAG(STATUS_IDLE);
}

void sub_main(void) {
  sub_write_result(0, SUB_STATE_READY);
  sub_write_result(1, SMOKE_READY_MAGIC);
  sub_write_result(7, 0x7201);
  GA_SUB_SET_FLAG(STATUS_IDLE);

  while (1) {
    uint8_t cmd = sub_wait_cmd();
    segaos_smoke_cmd = cmd;
    sub_ack();

    if (cmd == CMD_INIT_OS || cmd == CMD_RENDER_FRAME ||
        cmd == CMD_BOOT_PROBE) {
      sub_write_result(0, SMOKE_DONE_MAGIC);
      sub_write_result(1, (uint16_t)cmd);
      sub_write_result(7, 0x72fe);
      sub_done();
    } else {
      sub_write_result(0, 0xeeee);
      sub_write_result(1, (uint16_t)cmd);
      sub_write_result(7, 0x72ff);
      sub_error();
    }
  }
}
