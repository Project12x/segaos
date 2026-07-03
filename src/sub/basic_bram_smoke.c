#include "basic_bram_smoke.h"
#include "basic.h"
#include "basic_bram_storage.h"
#include "basic_storage.h"
#include "storage.h"

static BasicBramStorage smokeBramStorage;
static BasicStorageAdapter smokeAdapter;
static BasicStorageIO smokeIo;
static BasicLine smokeSourceLines[4];
static BasicLine smokeLoadedLines[4];
static uint8_t smokeSourceStorage[256];
static uint8_t smokeLoadedStorage[256];
static uint8_t smokeIoBuffer[STG_INTERNAL_BASIC_LIMIT_BYTES];
static uint8_t smokeBeforeImage[STG_INTERNAL_BASIC_LIMIT_BYTES];
static uint8_t smokeAfterImage[STG_INTERNAL_BASIC_LIMIT_BYTES];
static char smokeLineBuffer[64];

static uint16_t bas_smoke_hash(const uint8_t *bytes, uint16_t byteCount) {
  uint16_t hash = 0x4253U;

  if (!bytes)
    return hash;

  for (uint16_t i = 0; i < byteCount; i++) {
    hash = (uint16_t)((uint16_t)((hash << 5) | (hash >> 11)) ^ bytes[i]);
  }
  return hash;
}

static void bas_smoke_copy_probe(BasicBramSmokeResult *result,
                                 const BasicBramStorage *storage) {
  if (!result || !storage)
    return;

  result->formatStatus = (uint16_t)storage->lastProbe.status;
  result->totalBlocks4K = storage->lastProbe.totalBlocks4K;
  result->freeBlocks4K = storage->lastProbe.freeBlocks4K;
  result->fileCount = storage->lastProbe.fileCount;
}

static void bas_smoke_set_status(BasicBramSmokeResult *result,
                                 BasicBramSmokeStatus status) {
  if (result)
    result->status = (uint16_t)status;
}

void BAS_ClearBramSmokeResult(BasicBramSmokeResult *result) {
  if (!result)
    return;

  result->magic = BAS_BRAM_SMOKE_MAGIC;
  result->status = BAS_BRAM_SMOKE_BAD_ARGUMENT;
  result->formatStatus = BRM_FORMAT_NO_RAM;
  result->totalBlocks4K = 0;
  result->freeBlocks4K = 0;
  result->fileCount = 0;
  result->saveOk = 0;
  result->loadOk = 0;
  result->savedBytes = 0;
  result->loadedBytes = 0;
  result->lineCount = 0;
  result->firstLineNumber = 0;
  result->secondLineNumber = 0;
  result->lastSaveTarget = STG_VOLUME_NONE;
  result->lastLoadTarget = STG_VOLUME_NONE;
  result->imageHashBefore = 0;
  result->imageHashAfter = 0;
}

uint8_t BAS_RunBramSmoke(BasicBramSmokeResult *result, const BramBiosOps *ops,
                         const char *filename) {
  const char *bramName = filename ? filename : BAS_BRAM_SMOKE_FILENAME;
  StorageVolumeInfo absentCart;
  BasicProgram sourceProgram;
  BasicProgram loadedProgram;
  BasicCommandResult commandResult;
  uint16_t imageBytes = 0;

  BAS_ClearBramSmokeResult(result);
  if (!result || !ops)
    return 0;

  if (!BAS_BramStorageInit(&smokeBramStorage, ops, bramName)) {
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_STORAGE_INIT_FAILED);
    return 0;
  }

  if (!BAS_BramStorageProbeInternal(&smokeBramStorage)) {
    bas_smoke_copy_probe(result, &smokeBramStorage);
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_PROBE_FAILED);
    return 0;
  }
  bas_smoke_copy_probe(result, &smokeBramStorage);

  BAS_InitProgram(&sourceProgram, smokeSourceLines, 4, smokeSourceStorage,
                  sizeof(smokeSourceStorage));
  BAS_InitProgram(&loadedProgram, smokeLoadedLines, 4, smokeLoadedStorage,
                  sizeof(smokeLoadedStorage));

  if (!BAS_StoreSourceLine(&sourceProgram, "10 PRINT \"BRAM OK\"") ||
      !BAS_StoreSourceLine(&sourceProgram, "20 END") ||
      !BAS_ExportProgramImage(&sourceProgram, smokeBeforeImage,
                              sizeof(smokeBeforeImage), &imageBytes)) {
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_PROGRAM_FAILED);
    return 0;
  }

  result->savedBytes = imageBytes;
  result->imageHashBefore = bas_smoke_hash(smokeBeforeImage, imageBytes);

  STG_InitVolume(&absentCart, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);
  BAS_StorageInitAdapter(&smokeAdapter, &absentCart,
                         BAS_BramStorageInternalVolume(&smokeBramStorage),
                         BAS_BramStorageWrite, BAS_BramStorageRead,
                         &smokeBramStorage);
  if (!BAS_StorageBindIO(&smokeAdapter, smokeIoBuffer, sizeof(smokeIoBuffer),
                         &smokeIo)) {
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_BIND_FAILED);
    return 0;
  }

  if (!BAS_SubmitConsoleLineWithStorage(
          &sourceProgram, "SAVE", (BasicLineSink)0, (void *)0,
          smokeLineBuffer, sizeof(smokeLineBuffer), &smokeIo,
          &commandResult)) {
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_SAVE_FAILED);
    return 0;
  }
  result->saveOk = 1;
  result->savedBytes = commandResult.bytesTransferred;
  result->lastSaveTarget =
      (uint16_t)BAS_StorageLastSavePlan(&smokeAdapter)->target;

  if (!BAS_SubmitConsoleLineWithStorage(
          &loadedProgram, "LOAD", (BasicLineSink)0, (void *)0,
          smokeLineBuffer, sizeof(smokeLineBuffer), &smokeIo,
          &commandResult)) {
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_LOAD_FAILED);
    return 0;
  }
  result->loadOk = 1;
  result->loadedBytes = commandResult.bytesTransferred;
  result->lastLoadTarget = (uint16_t)BAS_StorageLastLoadTarget(&smokeAdapter);
  result->lineCount = loadedProgram.lineCount;
  if (loadedProgram.lineCount > 0) {
    const BasicLine *line = BAS_GetLine(&loadedProgram, 0);
    result->firstLineNumber = line ? line->number : 0;
  }
  if (loadedProgram.lineCount > 1) {
    const BasicLine *line = BAS_GetLine(&loadedProgram, 1);
    result->secondLineNumber = line ? line->number : 0;
  }

  imageBytes = 0;
  if (!BAS_ExportProgramImage(&loadedProgram, smokeAfterImage,
                              sizeof(smokeAfterImage), &imageBytes)) {
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_VERIFY_FAILED);
    return 0;
  }
  result->imageHashAfter = bas_smoke_hash(smokeAfterImage, imageBytes);

  if (result->lineCount != 2 || result->firstLineNumber != 10 ||
      result->secondLineNumber != 20 ||
      result->lastSaveTarget != STG_VOLUME_INTERNAL_BRAM ||
      result->lastLoadTarget != STG_VOLUME_INTERNAL_BRAM ||
      result->imageHashBefore != result->imageHashAfter) {
    bas_smoke_set_status(result, BAS_BRAM_SMOKE_VERIFY_FAILED);
    return 0;
  }

  bas_smoke_set_status(result, BAS_BRAM_SMOKE_OK);
  return 1;
}
