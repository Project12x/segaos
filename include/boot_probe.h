/*
 * boot_probe.h - Minimal Sega CD dual-CPU bring-up probe constants.
 *
 * The probe is compiled only when BOOT_PROBE is defined. It writes stable
 * debugger-visible symbols so emulator/GDB scripts can validate boot progress
 * before the full desktop loop is involved.
 */

#ifndef BOOT_PROBE_H
#define BOOT_PROBE_H

#include <stdint.h>

#define PROBE_MAIN_MAGIC 0x4D50u /* "MP" */
#define PROBE_SUB_MAGIC 0x5350u  /* "SP" */
#define PROBE_READY_MAGIC 0x5244u
#define PROBE_DONE_MAGIC 0x444Eu
#define PROBE_WORD0 0xA55Au
#define PROBE_WORD1 0x5AA5u
#define PROBE_FB_WORD0 0x1234u
#define PROBE_FB_WORD1 0x5678u
#define PROBE_FB_WORD2 0x9ABCu
#define PROBE_FB_WORD3 0xDEF0u

#define RUNTIME_SMOKE_READY_MAGIC 0x4343u
#define RUNTIME_SMOKE_DONE_MAGIC 0x534Du

typedef enum {
  PROBE_PHASE_RESET = 0x0000,
  PROBE_PHASE_MAIN_STARTED = 0x0001,
  PROBE_PHASE_SUB_READY = 0x0002,
  PROBE_PHASE_COMMAND_SENT = 0x0003,
  PROBE_PHASE_COMMAND_DONE = 0x0004,
  PROBE_PHASE_WORD_RAM_VISIBLE = 0x0005,
  PROBE_PHASE_PRG_DIAG = 0x0006,
  PROBE_PHASE_FAILED = 0x00FE,
  PROBE_PHASE_DONE = 0x00FF
} ProbePhase;

#endif /* BOOT_PROBE_H */
