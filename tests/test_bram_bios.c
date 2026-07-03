#include "bram.h"
#include <stdio.h>

static int failures;

typedef struct {
  void *initWork;
  void *initStrings;
  void *statStrings;
  const BramFilename *searchFilename;
  const BramFilename *readFilename;
  uint8_t *readBuffer;
  const BramFileInfo *writeInfo;
  const uint8_t *writeData;
  const char *dirPattern;
  uint8_t *dirBuffer;
  uint16_t dirSkip;
  uint16_t dirBytes;
  uint8_t initCalls;
  uint8_t statCalls;
  uint8_t searchCalls;
  uint8_t readCalls;
  uint8_t writeCalls;
  uint8_t dirCalls;
} FakeRawBios;

static FakeRawBios fakeRaw;

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

static void expect_ptr(const void *actual, const void *expected,
                       const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %p got %p\n", name, expected, actual);
    failures++;
  }
}

uint8_t BRM_BiosCallInit(void *workBuffer, void *stringBuffer,
                         uint16_t *totalBlocks4K, uint16_t *statusCode) {
  fakeRaw.initCalls++;
  fakeRaw.initWork = workBuffer;
  fakeRaw.initStrings = stringBuffer;
  *totalBlocks4K = 2;
  *statusCode = BRM_FORMAT_SEGA;
  return 1;
}

uint8_t BRM_BiosCallStat(void *stringBuffer, uint16_t *freeBlocks4K,
                         uint16_t *fileCount) {
  fakeRaw.statCalls++;
  fakeRaw.statStrings = stringBuffer;
  *freeBlocks4K = 1;
  *fileCount = 4;
  return 1;
}

uint8_t BRM_BiosCallSearch(const BramFilename *filename, uint16_t *blocks,
                           uint8_t *mode) {
  fakeRaw.searchCalls++;
  fakeRaw.searchFilename = filename;
  *blocks = 3;
  *mode = BRM_FILE_MODE_NORMAL;
  return 1;
}

uint8_t BRM_BiosCallRead(const BramFilename *filename, uint8_t *buffer,
                         uint16_t *blocks, uint8_t *mode) {
  fakeRaw.readCalls++;
  fakeRaw.readFilename = filename;
  fakeRaw.readBuffer = buffer;
  buffer[0] = 0x51;
  *blocks = 3;
  *mode = BRM_FILE_MODE_NORMAL;
  return 1;
}

uint8_t BRM_BiosCallWrite(const BramFileInfo *info, const uint8_t *data) {
  fakeRaw.writeCalls++;
  fakeRaw.writeInfo = info;
  fakeRaw.writeData = data;
  return 1;
}

uint8_t BRM_BiosCallDir(const char *pattern, uint8_t *dirBuffer,
                        uint16_t skip, uint16_t dirBytes) {
  fakeRaw.dirCalls++;
  fakeRaw.dirPattern = pattern;
  fakeRaw.dirBuffer = dirBuffer;
  fakeRaw.dirSkip = skip;
  fakeRaw.dirBytes = dirBytes;
  return 1;
}

static void internal_ops_bind_context_and_callbacks(void) {
  BramBiosContext context;
  BramBiosOps ops;

  context.work[0] = 0x7a;
  context.strings[0] = 0x6b;
  expect_true(BRM_InitInternalBiosOps(&context, &ops),
              "bind internal bios ops");
  expect_ptr(ops.user, &context, "ops user context");
  expect_u8(context.work[0], 0, "context work cleared");
  expect_u8(context.strings[0], 0, "context strings cleared");
  expect_true(ops.init != 0, "ops init callback");
  expect_true(ops.stat != 0, "ops stat callback");
  expect_true(ops.search != 0, "ops search callback");
  expect_true(ops.read != 0, "ops read callback");
  expect_true(ops.write != 0, "ops write callback");
  expect_true(ops.dir != 0, "ops dir callback");

  expect_false(BRM_InitInternalBiosOps((BramBiosContext *)0, &ops),
               "reject missing context");
  expect_false(BRM_InitInternalBiosOps(&context, (BramBiosOps *)0),
               "reject missing ops");
}

static void internal_ops_route_probe_calls_to_raw_bios(void) {
  BramBiosContext context;
  BramBiosOps ops;
  BramProbeResult probe;

  fakeRaw = (FakeRawBios){0};
  expect_true(BRM_InitInternalBiosOps(&context, &ops), "bind for probe");
  expect_u8(BRM_Probe(&ops, &probe), BRM_RESULT_OK, "probe through bios ops");

  expect_u8(fakeRaw.initCalls, 1, "raw init call count");
  expect_u8(fakeRaw.statCalls, 1, "raw stat call count");
  expect_ptr(fakeRaw.initWork, context.work, "raw init work buffer");
  expect_ptr(fakeRaw.initStrings, context.strings, "raw init string buffer");
  expect_ptr(fakeRaw.statStrings, context.strings, "raw stat string buffer");
  expect_u16(probe.totalBlocks4K, 2, "probe total blocks");
  expect_u16(probe.freeBlocks4K, 1, "probe free blocks");
  expect_u16(probe.fileCount, 4, "probe file count");
}

static void internal_ops_route_file_and_directory_calls(void) {
  BramBiosContext context;
  BramBiosOps ops;
  BramFilename filename;
  uint8_t data[BRM_NORMAL_FILE_BLOCK_BYTES * 3U];
  uint8_t dir[BRM_DIR_ENTRY_BYTES * 2U];
  uint16_t bytesRead = 0;

  fakeRaw = (FakeRawBios){0};
  expect_true(BRM_InitInternalBiosOps(&context, &ops), "bind for file ops");
  expect_true(BRM_MakeFilename("BASIC", &filename), "make filename");
  expect_u8(BRM_ReadFile(&ops, &filename, data, sizeof(data), &bytesRead),
            BRM_RESULT_OK, "read through raw bios");
  expect_u8(fakeRaw.searchCalls, 1, "raw search call count");
  expect_u8(fakeRaw.readCalls, 1, "raw read call count");
  expect_ptr(fakeRaw.searchFilename, &filename, "raw search filename");
  expect_ptr(fakeRaw.readFilename, &filename, "raw read filename");
  expect_ptr(fakeRaw.readBuffer, data, "raw read buffer");
  expect_u16(bytesRead, sizeof(data), "read byte count");
  expect_u8(data[0], 0x51, "read byte copied");

  expect_u8(BRM_WriteFile(&ops, &filename, data, 65), BRM_RESULT_OK,
            "write through raw bios");
  expect_u8(fakeRaw.writeCalls, 1, "raw write call count");
  expect_ptr(fakeRaw.writeData, data, "raw write data");
  expect_u16(fakeRaw.writeInfo->blocks, 2, "raw write blocks");

  expect_u8(BRM_ReadDirectory(&ops, "BASIC*", dir, sizeof(dir), 5),
            BRM_RESULT_OK, "directory through raw bios");
  expect_u8(fakeRaw.dirCalls, 1, "raw dir call count");
  expect_ptr(fakeRaw.dirBuffer, dir, "raw dir buffer");
  expect_u16(fakeRaw.dirSkip, 5, "raw dir skip");
  expect_u16(fakeRaw.dirBytes, sizeof(dir), "raw dir bytes");
}

int main(void) {
  internal_ops_bind_context_and_callbacks();
  internal_ops_route_probe_calls_to_raw_bios();
  internal_ops_route_file_and_directory_calls();

  if (failures) {
    printf("bram bios tests failed: %d\n", failures);
    return 1;
  }

  printf("bram bios tests passed\n");
  return 0;
}
