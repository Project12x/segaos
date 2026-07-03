#include "basic_bram_storage.h"
#include "basic.h"
#include "basic_storage.h"
#include "bram.h"
#include "storage.h"
#include <stdio.h>

static int failures;

typedef struct {
  uint16_t initBlocks;
  BramFormatStatus initStatus;
  uint16_t statFreeBlocks;
  uint16_t statFileCount;
  uint16_t searchBlocks;
  uint8_t searchMode;
  uint16_t readBlocks;
  uint8_t readMode;
  uint8_t initCalls;
  uint8_t statCalls;
  uint8_t searchCalls;
  uint8_t readCalls;
  uint8_t writeCalls;
  BramFileInfo lastWriteInfo;
  uint16_t lastWriteBytes;
  BramFilename lastReadFilename;
  uint8_t stored[256];
} FakeBram;

static void expect_true(uint8_t value, const char *name) {
  if (!value) {
    printf("FAIL: %s expected true\n", name);
    failures++;
  }
}

static void expect_false(uint8_t value, const char *name) {
  if (value) {
    printf("FAIL: %s expected false\n", name);
    failures++;
  }
}

static void expect_u8(uint8_t actual, uint8_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_u16(uint16_t actual, uint16_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_u32(uint32_t actual, uint32_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %lu got %lu\n", name, (unsigned long)expected,
           (unsigned long)actual);
    failures++;
  }
}

static void expect_char(char actual, char expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected '%c' got '%c'\n", name, expected, actual);
    failures++;
  }
}

static uint8_t fake_init(uint16_t *totalBlocks, BramFormatStatus *status,
                         void *user) {
  FakeBram *fake = (FakeBram *)user;

  if (!fake || !totalBlocks || !status)
    return 0;
  fake->initCalls++;
  *totalBlocks = fake->initBlocks;
  *status = fake->initStatus;
  return 1;
}

static uint8_t fake_stat(uint16_t *freeBlocks, uint16_t *fileCount,
                         void *user) {
  FakeBram *fake = (FakeBram *)user;

  if (!fake || !freeBlocks || !fileCount)
    return 0;
  fake->statCalls++;
  *freeBlocks = fake->statFreeBlocks;
  *fileCount = fake->statFileCount;
  return 1;
}

static uint8_t fake_search(const BramFilename *filename, uint16_t *blocks,
                           uint8_t *mode, void *user) {
  FakeBram *fake = (FakeBram *)user;

  if (!fake || !filename || !blocks || !mode)
    return 0;
  fake->searchCalls++;
  *blocks = fake->searchBlocks;
  *mode = fake->searchMode;
  return 1;
}

static uint8_t fake_read(const BramFilename *filename, uint8_t *buffer,
                         uint16_t *blocks, uint8_t *mode, void *user) {
  FakeBram *fake = (FakeBram *)user;
  uint16_t bytes;

  if (!fake || !filename || !buffer || !blocks || !mode)
    return 0;
  fake->readCalls++;
  fake->lastReadFilename = *filename;
  *blocks = fake->readBlocks;
  *mode = fake->readMode;
  bytes = (uint16_t)BRM_ModeBytes(fake->readBlocks, fake->readMode);
  for (uint16_t i = 0; i < bytes; i++) {
    buffer[i] = fake->stored[i];
  }
  return 1;
}

static uint8_t fake_write(const BramFileInfo *info, const uint8_t *data,
                          void *user) {
  FakeBram *fake = (FakeBram *)user;
  uint16_t bytes;

  if (!fake || !info || !data)
    return 0;
  fake->writeCalls++;
  fake->lastWriteInfo = *info;
  bytes = (uint16_t)BRM_ModeBytes(info->blocks, info->mode);
  fake->lastWriteBytes = bytes;
  for (uint16_t i = 0; i < bytes; i++) {
    fake->stored[i] = data[i];
  }
  return 1;
}

static uint8_t fake_dir(const char *pattern, uint8_t *buffer, uint16_t skip,
                        uint16_t dirBytes, void *user) {
  (void)pattern;
  (void)buffer;
  (void)skip;
  (void)dirBytes;
  (void)user;
  return 0;
}

static BramBiosOps make_ops(FakeBram *fake) {
  BramBiosOps ops;

  ops.init = fake_init;
  ops.stat = fake_stat;
  ops.search = fake_search;
  ops.read = fake_read;
  ops.write = fake_write;
  ops.dir = fake_dir;
  ops.user = fake;
  return ops;
}

static void init_normalizes_basic_bram_filename(void) {
  FakeBram fake = {0};
  BramBiosOps ops = make_ops(&fake);
  BasicBramStorage storage;

  expect_true(BAS_BramStorageInit(&storage, &ops, 0),
              "init default BASIC BRAM storage");
  expect_char(storage.filename.bytes[0], 'B', "default name B");
  expect_char(storage.filename.bytes[1], 'A', "default name A");
  expect_char(storage.filename.bytes[4], 'C', "default name C");
  expect_char(storage.filename.bytes[5], 0, "default name terminator");

  expect_true(BAS_BramStorageInit(&storage, &ops, "basic_1"),
              "init custom BASIC BRAM storage");
  expect_char(storage.filename.bytes[0], 'B', "custom name uppercase");
  expect_char(storage.filename.bytes[5], '_', "custom name underscore");
  expect_false(BAS_BramStorageInit(&storage, &ops, "BAD.NAME"),
               "reject invalid BRAM filename");
  expect_false(BAS_BramStorageInit(&storage, 0, "BASIC"),
               "reject missing BRAM ops");
}

static void probe_updates_internal_volume_for_basic_adapter(void) {
  FakeBram fake = {0};
  BramBiosOps ops;
  BasicBramStorage storage;
  const StorageVolumeInfo *volume;

  fake.initBlocks = 2;
  fake.initStatus = BRM_FORMAT_SEGA;
  fake.statFreeBlocks = 1;
  fake.statFileCount = 2;
  ops = make_ops(&fake);

  expect_true(BAS_BramStorageInit(&storage, &ops, "BASIC"),
              "init before probe");
  expect_true(BAS_BramStorageProbeInternal(&storage), "probe internal BRAM");
  volume = BAS_BramStorageInternalVolume(&storage);
  expect_u8(fake.initCalls, 1, "probe init calls");
  expect_u8(fake.statCalls, 1, "probe stat calls");
  expect_u8(volume->kind, STG_VOLUME_INTERNAL_BRAM, "probe volume kind");
  expect_true(volume->present, "probe volume present");
  expect_true(volume->writable, "probe volume writable");
  expect_u32(volume->totalBytes, 2UL * BRM_INIT_BLOCK_BYTES,
             "probe volume total");
  expect_u32(volume->freeBytes, BRM_INIT_BLOCK_BYTES, "probe volume free");
}

static void write_callback_saves_basic_image_to_internal_bram(void) {
  FakeBram fake = {0};
  BramBiosOps ops = make_ops(&fake);
  BasicBramStorage storage;
  StorageSavePlan plan;
  uint8_t image[65] = {'S', 'B', 'A', 'S', 1, 0, 0, 0};

  plan.target = STG_VOLUME_INTERNAL_BRAM;
  plan.canSave = 1;
  plan.fallbackUsed = 1;
  plan.reserveBytes = STG_INTERNAL_BRAM_RESERVE_BYTES;
  plan.usableFreeBytes = 4096;

  expect_true(BAS_BramStorageInit(&storage, &ops, "BASIC"),
              "init before BRAM write");
  expect_true(BAS_BramStorageWrite(STG_VOLUME_INTERNAL_BRAM, image,
                                   sizeof(image), &plan, &storage),
              "write BASIC image to BRAM");
  expect_u8(fake.writeCalls, 1, "write callback BRAM call count");
  expect_char(fake.lastWriteInfo.filename.bytes[0], 'B',
              "write callback filename");
  expect_u16(fake.lastWriteInfo.blocks, 2, "write callback block count");
  expect_u16(fake.lastWriteBytes, 2U * BRM_NORMAL_FILE_BLOCK_BYTES,
             "write callback padded bytes");

  expect_false(BAS_BramStorageWrite(STG_VOLUME_EXTERNAL_CART, image,
                                    sizeof(image), &plan, &storage),
               "reject external cart target in internal BRAM bridge");
}

static void load_callback_accepts_padded_bram_block_size(void) {
  FakeBram fake = {0};
  BramBiosOps ops;
  BasicBramStorage storage;
  uint8_t image[128];
  uint16_t imageBytes = 0;

  fake.searchBlocks = 1;
  fake.searchMode = BRM_FILE_MODE_NORMAL;
  fake.readBlocks = 1;
  fake.readMode = BRM_FILE_MODE_NORMAL;
  fake.stored[0] = 'S';
  fake.stored[1] = 'B';
  fake.stored[2] = 'A';
  fake.stored[3] = 'S';
  fake.stored[4] = 1;
  fake.stored[5] = 0;
  fake.stored[6] = 0;
  fake.stored[7] = 0;
  ops = make_ops(&fake);

  expect_true(BAS_BramStorageInit(&storage, &ops, "BASIC"),
              "init before BRAM load");
  expect_true(BAS_BramStorageRead(STG_VOLUME_INTERNAL_BRAM, image,
                                  sizeof(image), &imageBytes, &storage),
              "read BASIC image from BRAM");
  expect_u8(fake.searchCalls, 1, "read callback search count");
  expect_u8(fake.readCalls, 1, "read callback BRAM call count");
  expect_u16(imageBytes, BRM_NORMAL_FILE_BLOCK_BYTES,
             "read callback reports padded block bytes");
  expect_u8(image[0], 'S', "read callback image magic");
  expect_char(fake.lastReadFilename.bytes[0], 'B', "read callback filename");

  expect_false(BAS_BramStorageRead(STG_VOLUME_EXTERNAL_CART, image,
                                   sizeof(image), &imageBytes, &storage),
               "reject external cart read in internal BRAM bridge");
}

static void shell_save_and_load_route_through_internal_bram_bridge(void) {
  FakeBram fake = {0};
  BramBiosOps ops;
  BasicBramStorage bramStorage;
  StorageVolumeInfo cart;
  BasicStorageAdapter adapter;
  BasicStorageIO io;
  BasicLine sourceLines[4];
  BasicLine destLines[4];
  uint8_t sourceStorage[96];
  uint8_t destStorage[96];
  uint8_t scratch[192];
  BasicProgram source;
  BasicProgram dest;
  BasicCommandResult result;
  char lineBuffer[48];

  fake.initBlocks = 2;
  fake.initStatus = BRM_FORMAT_SEGA;
  fake.statFreeBlocks = 1;
  fake.searchBlocks = 1;
  fake.searchMode = BRM_FILE_MODE_NORMAL;
  fake.readBlocks = 1;
  fake.readMode = BRM_FILE_MODE_NORMAL;
  ops = make_ops(&fake);

  expect_true(BAS_BramStorageInit(&bramStorage, &ops, "BASIC"),
              "init shell BRAM storage");
  expect_true(BAS_BramStorageProbeInternal(&bramStorage),
              "probe shell BRAM storage");
  STG_InitVolume(&cart, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);
  BAS_StorageInitAdapter(&adapter, &cart,
                         BAS_BramStorageInternalVolume(&bramStorage),
                         BAS_BramStorageWrite, BAS_BramStorageRead,
                         &bramStorage);
  expect_true(BAS_StorageBindIO(&adapter, scratch, sizeof(scratch), &io),
              "bind BASIC BRAM storage IO");

  BAS_InitProgram(&source, sourceLines, 4, sourceStorage,
                  sizeof(sourceStorage));
  BAS_InitProgram(&dest, destLines, 4, destStorage, sizeof(destStorage));
  expect_true(BAS_StoreSourceLine(&source, "10 PRINT \"BRAM\""),
              "store source before BRAM SAVE");
  expect_true(BAS_SubmitConsoleLineWithStorage(
                  &source, "SAVE", 0, 0, lineBuffer, sizeof(lineBuffer), &io,
                  &result),
              "shell SAVE through BRAM bridge");
  expect_u8(fake.writeCalls, 1, "shell BRAM write count");
  expect_u8(result.kind, BAS_CMD_SAVE, "shell BRAM save kind");
  expect_u8(BAS_StorageLastSavePlan(&adapter)->target,
            STG_VOLUME_INTERNAL_BRAM, "shell BRAM save target");

  expect_true(BAS_SubmitConsoleLineWithStorage(
                  &dest, "LOAD", 0, 0, lineBuffer, sizeof(lineBuffer), &io,
                  &result),
              "shell LOAD through BRAM bridge");
  expect_u8(result.kind, BAS_CMD_LOAD, "shell BRAM load kind");
  expect_u8(fake.readCalls, 1, "shell BRAM read count");
  expect_u8(BAS_StorageLastLoadTarget(&adapter), STG_VOLUME_INTERNAL_BRAM,
            "shell BRAM load target");
}

int main(void) {
  init_normalizes_basic_bram_filename();
  probe_updates_internal_volume_for_basic_adapter();
  write_callback_saves_basic_image_to_internal_bram();
  load_callback_accepts_padded_bram_block_size();
  shell_save_and_load_route_through_internal_bram_bridge();

  if (failures) {
    printf("basic bram storage tests failed: %d\n", failures);
    return 1;
  }

  printf("basic bram storage tests passed\n");
  return 0;
}
