/*
 * basic_bram_smoke.h - Narrow BASIC SAVE/LOAD smoke test over BRAM ops.
 *
 * This is a probe seam: host tests can run it against fake BramBiosOps, while
 * the target build can run the same path against the internal Sub BIOS adapter.
 */

#ifndef BASIC_BRAM_SMOKE_H
#define BASIC_BRAM_SMOKE_H

#include "bram.h"
#include <stdint.h>

#define BAS_BRAM_SMOKE_MAGIC 0x5342U /* "SB" */
#define BAS_BRAM_SMOKE_FILENAME "BASPROBE"

typedef enum {
  BAS_BRAM_SMOKE_OK = 0,
  BAS_BRAM_SMOKE_BAD_ARGUMENT = 1,
  BAS_BRAM_SMOKE_STORAGE_INIT_FAILED = 2,
  BAS_BRAM_SMOKE_PROBE_FAILED = 3,
  BAS_BRAM_SMOKE_PROGRAM_FAILED = 4,
  BAS_BRAM_SMOKE_BIND_FAILED = 5,
  BAS_BRAM_SMOKE_SAVE_FAILED = 6,
  BAS_BRAM_SMOKE_LOAD_FAILED = 7,
  BAS_BRAM_SMOKE_VERIFY_FAILED = 8
} BasicBramSmokeStatus;

typedef struct {
  uint16_t magic;
  uint16_t status;
  uint16_t formatStatus;
  uint16_t totalBlocks4K;
  uint16_t freeBlocks4K;
  uint16_t fileCount;
  uint16_t saveOk;
  uint16_t loadOk;
  uint16_t savedBytes;
  uint16_t loadedBytes;
  uint16_t lineCount;
  uint16_t firstLineNumber;
  uint16_t secondLineNumber;
  uint16_t lastSaveTarget;
  uint16_t lastLoadTarget;
  uint16_t imageHashBefore;
  uint16_t imageHashAfter;
} BasicBramSmokeResult;

void BAS_ClearBramSmokeResult(BasicBramSmokeResult *result);
uint8_t BAS_RunBramSmoke(BasicBramSmokeResult *result, const BramBiosOps *ops,
                         const char *filename);

#endif /* BASIC_BRAM_SMOKE_H */
