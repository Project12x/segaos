#include "basic_bram_smoke.h"
#include "bram.h"
#include "storage.h"
#include <stdio.h>

static int failures;

typedef struct {
  uint16_t initBlocks;
  BramFormatStatus initStatus;
  uint16_t statFreeBlocks;
  uint16_t statFileCount;
  uint8_t filePresent;
  uint8_t initCalls;
  uint8_t statCalls;
  uint8_t searchCalls;
  uint8_t readCalls;
  uint8_t writeCalls;
  BramFileInfo lastWriteInfo;
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

  if (!fake || !filename || !blocks || !mode || !fake->filePresent)
    return 0;
  fake->searchCalls++;
  *blocks = fake->lastWriteInfo.blocks;
  *mode = fake->lastWriteInfo.mode;
  return 1;
}

static uint8_t fake_read(const BramFilename *filename, uint8_t *buffer,
                         uint16_t *blocks, uint8_t *mode, void *user) {
  FakeBram *fake = (FakeBram *)user;
  uint16_t bytes;

  if (!fake || !filename || !buffer || !blocks || !mode || !fake->filePresent)
    return 0;
  fake->readCalls++;
  *blocks = fake->lastWriteInfo.blocks;
  *mode = fake->lastWriteInfo.mode;
  bytes = (uint16_t)BRM_ModeBytes(*blocks, *mode);
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
  for (uint16_t i = 0; i < bytes; i++) {
    fake->stored[i] = data[i];
  }
  fake->filePresent = 1;
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

static void smoke_saves_and_loads_program_through_bram_bridge(void) {
  FakeBram fake = {0};
  BramBiosOps ops;
  BasicBramSmokeResult result;

  fake.initBlocks = 2;
  fake.initStatus = BRM_FORMAT_SEGA;
  fake.statFreeBlocks = 2;
  fake.statFileCount = 0;
  ops = make_ops(&fake);

  expect_true(BAS_RunBramSmoke(&result, &ops, "BASPROBE"),
              "run BASIC BRAM smoke");
  expect_u16(result.magic, BAS_BRAM_SMOKE_MAGIC, "smoke magic");
  expect_u16(result.status, BAS_BRAM_SMOKE_OK, "smoke status");
  expect_u16(result.formatStatus, BRM_FORMAT_SEGA, "smoke format status");
  expect_u16(result.totalBlocks4K, 2, "smoke total blocks");
  expect_u16(result.freeBlocks4K, 2, "smoke free blocks");
  expect_u8(fake.initCalls, 1, "smoke init calls");
  expect_u8(fake.statCalls, 1, "smoke stat calls");
  expect_u8(fake.writeCalls, 1, "smoke write calls");
  expect_u8(fake.searchCalls, 1, "smoke search calls");
  expect_u8(fake.readCalls, 1, "smoke read calls");
  expect_u16(fake.lastWriteInfo.blocks, 1, "smoke padded block count");
  expect_u16(result.saveOk, 1, "smoke save ok");
  expect_u16(result.loadOk, 1, "smoke load ok");
  expect_u16(result.lineCount, 2, "smoke loaded line count");
  expect_u16(result.firstLineNumber, 10, "smoke first line");
  expect_u16(result.secondLineNumber, 20, "smoke second line");
  expect_u16(result.lastSaveTarget, STG_VOLUME_INTERNAL_BRAM,
             "smoke save target");
  expect_u16(result.lastLoadTarget, STG_VOLUME_INTERNAL_BRAM,
             "smoke load target");
  expect_u16(result.imageHashBefore, result.imageHashAfter,
             "smoke image hashes match");
}

static void smoke_reports_unformatted_bram_without_writing(void) {
  FakeBram fake = {0};
  BramBiosOps ops;
  BasicBramSmokeResult result;

  fake.initBlocks = 2;
  fake.initStatus = BRM_FORMAT_UNFORMATTED;
  ops = make_ops(&fake);

  expect_false(BAS_RunBramSmoke(&result, &ops, "BASPROBE"),
               "reject unformatted BRAM smoke");
  expect_u16(result.magic, BAS_BRAM_SMOKE_MAGIC, "unformatted smoke magic");
  expect_u16(result.status, BAS_BRAM_SMOKE_PROBE_FAILED,
             "unformatted smoke status");
  expect_u16(result.formatStatus, BRM_FORMAT_UNFORMATTED,
             "unformatted smoke format");
  expect_u8(fake.writeCalls, 0, "unformatted smoke write calls");
}

int main(void) {
  smoke_saves_and_loads_program_through_bram_bridge();
  smoke_reports_unformatted_bram_without_writing();

  if (failures) {
    printf("basic bram smoke tests failed: %d\n", failures);
    return 1;
  }

  printf("basic bram smoke tests passed\n");
  return 0;
}
