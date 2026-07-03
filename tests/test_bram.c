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
  uint8_t dirCalls;
  BramFileInfo lastWriteInfo;
  uint16_t lastDirSkip;
  uint16_t lastDirBytes;
  char lastDirPattern[BRM_FILENAME_BUFFER_BYTES];
} FakeBramBios;

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
  FakeBramBios *fake = (FakeBramBios *)user;

  if (!fake || !totalBlocks || !status)
    return 0;
  fake->initCalls++;
  *totalBlocks = fake->initBlocks;
  *status = fake->initStatus;
  return 1;
}

static uint8_t fake_stat(uint16_t *freeBlocks, uint16_t *fileCount,
                         void *user) {
  FakeBramBios *fake = (FakeBramBios *)user;

  if (!fake || !freeBlocks || !fileCount)
    return 0;
  fake->statCalls++;
  *freeBlocks = fake->statFreeBlocks;
  *fileCount = fake->statFileCount;
  return 1;
}

static uint8_t fake_search(const BramFilename *filename, uint16_t *blocks,
                           uint8_t *mode, void *user) {
  FakeBramBios *fake = (FakeBramBios *)user;

  (void)filename;
  if (!fake || !blocks || !mode)
    return 0;
  fake->searchCalls++;
  *blocks = fake->searchBlocks;
  *mode = fake->searchMode;
  return 1;
}

static uint8_t fake_read(const BramFilename *filename, uint8_t *buffer,
                         uint16_t *blocks, uint8_t *mode, void *user) {
  FakeBramBios *fake = (FakeBramBios *)user;

  (void)filename;
  if (!fake || !buffer || !blocks || !mode)
    return 0;
  fake->readCalls++;
  buffer[0] = 0x42;
  *blocks = fake->readBlocks;
  *mode = fake->readMode;
  return 1;
}

static uint8_t fake_write(const BramFileInfo *info, const uint8_t *data,
                          void *user) {
  FakeBramBios *fake = (FakeBramBios *)user;

  if (!fake || !info || !data)
    return 0;
  fake->writeCalls++;
  fake->lastWriteInfo = *info;
  return 1;
}

static uint8_t fake_dir(const char *pattern, uint8_t *buffer, uint16_t skip,
                        uint16_t dirBytes, void *user) {
  FakeBramBios *fake = (FakeBramBios *)user;

  if (!fake || !pattern || !buffer || dirBytes == 0)
    return 0;
  fake->dirCalls++;
  fake->lastDirSkip = skip;
  fake->lastDirBytes = dirBytes;
  for (uint8_t i = 0; i < BRM_FILENAME_BUFFER_BYTES; i++) {
    fake->lastDirPattern[i] = pattern[i];
    if (!pattern[i])
      break;
  }
  buffer[0] = 0;
  return 1;
}

static BramBiosOps make_ops(FakeBramBios *fake) {
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

static void filename_normalization_is_bios_safe(void) {
  BramFilename filename;

  expect_true(BRM_MakeFilename("basic_1", &filename),
              "normalize lowercase filename");
  expect_char(filename.bytes[0], 'B', "filename char 0");
  expect_char(filename.bytes[5], '_', "filename underscore");
  expect_char(filename.bytes[6], '1', "filename digit");
  expect_char(filename.bytes[7], 0, "filename terminator");

  expect_false(BRM_MakeFilename("", &filename), "reject empty filename");
  expect_false(BRM_MakeFilename("TOO-LONG-NAME", &filename),
               "reject too long filename");
  expect_false(BRM_MakeFilename("BAD.NAME", &filename),
               "reject filename punctuation");
}

static void probe_maps_formatted_bram_to_storage_volume(void) {
  FakeBramBios fake = {0};
  BramBiosOps ops;
  BramProbeResult probe;
  StorageVolumeInfo volume;

  fake.initBlocks = 2;
  fake.initStatus = BRM_FORMAT_SEGA;
  fake.statFreeBlocks = 1;
  fake.statFileCount = 3;
  ops = make_ops(&fake);

  expect_u8(BRM_Probe(&ops, &probe), BRM_RESULT_OK, "probe formatted bram");
  expect_u8(fake.initCalls, 1, "probe init calls");
  expect_u8(fake.statCalls, 1, "probe stat calls");
  expect_true(probe.present, "probe present");
  expect_true(probe.formatted, "probe formatted");
  expect_u32(probe.totalBytes, 2UL * BRM_INIT_BLOCK_BYTES,
             "probe total bytes");
  expect_u32(probe.freeBytes, 1UL * BRM_INIT_BLOCK_BYTES,
             "probe free bytes");
  expect_u16(probe.fileCount, 3, "probe file count");

  BRM_InitInternalVolumeFromProbe(&probe, &volume);
  expect_u8(volume.kind, STG_VOLUME_INTERNAL_BRAM, "probe volume kind");
  expect_true(volume.present, "probe volume present");
  expect_true(volume.writable, "probe volume writable");
  expect_u32(volume.totalBytes, 2UL * BRM_INIT_BLOCK_BYTES,
             "probe volume total");
  expect_u32(volume.freeBytes, BRM_INIT_BLOCK_BYTES, "probe volume free");
}

static void probe_reports_unformatted_without_stat(void) {
  FakeBramBios fake = {0};
  BramBiosOps ops;
  BramProbeResult probe;

  fake.initBlocks = 2;
  fake.initStatus = BRM_FORMAT_UNFORMATTED;
  fake.statFreeBlocks = 1;
  ops = make_ops(&fake);

  expect_u8(BRM_Probe(&ops, &probe), BRM_RESULT_UNFORMATTED,
            "probe unformatted bram");
  expect_u8(fake.initCalls, 1, "unformatted init calls");
  expect_u8(fake.statCalls, 0, "unformatted skips stat");
  expect_true(probe.present, "unformatted present");
  expect_false(probe.formatted, "unformatted not formatted");
}

static void write_constructs_normal_file_info_blocks(void) {
  FakeBramBios fake = {0};
  BramBiosOps ops = make_ops(&fake);
  BramFilename filename;
  uint8_t data[65] = {0};

  expect_true(BRM_MakeFilename("BASIC", &filename), "write filename");
  expect_u8(BRM_WriteFile(&ops, &filename, data, sizeof(data)), BRM_RESULT_OK,
            "write file through bram wrapper");

  expect_u8(fake.writeCalls, 1, "write calls");
  expect_char(fake.lastWriteInfo.filename.bytes[0], 'B', "write name char");
  expect_u8(fake.lastWriteInfo.mode, BRM_FILE_MODE_NORMAL, "write mode");
  expect_u16(fake.lastWriteInfo.blocks, 2, "write normal blocks");
  expect_u16(BRM_NormalBlocksForBytes(sizeof(data)), 2,
             "normal block helper");
}

static void read_checks_search_size_before_bios_read(void) {
  FakeBramBios fake = {0};
  BramBiosOps ops;
  BramFilename filename;
  uint8_t buffer[BRM_NORMAL_FILE_BLOCK_BYTES * 2U];
  uint16_t bytesRead = 0;

  fake.searchBlocks = 2;
  fake.searchMode = BRM_FILE_MODE_NORMAL;
  fake.readBlocks = 2;
  fake.readMode = BRM_FILE_MODE_NORMAL;
  ops = make_ops(&fake);
  expect_true(BRM_MakeFilename("BASIC", &filename), "read filename");

  expect_u8(BRM_ReadFile(&ops, &filename, buffer, sizeof(buffer) - 1U,
                         &bytesRead),
            BRM_RESULT_TOO_SMALL, "read rejects small buffer");
  expect_u8(fake.searchCalls, 1, "small read search calls");
  expect_u8(fake.readCalls, 0, "small read skips bios read");
  expect_u16(bytesRead, sizeof(buffer), "small read reports required bytes");

  expect_u8(BRM_ReadFile(&ops, &filename, buffer, sizeof(buffer), &bytesRead),
            BRM_RESULT_OK, "read accepts enough buffer");
  expect_u8(fake.readCalls, 1, "read calls");
  expect_u16(bytesRead, sizeof(buffer), "read byte count");
  expect_u8(buffer[0], 0x42, "read data copied");
}

static void directory_call_passes_pattern_skip_and_size(void) {
  FakeBramBios fake = {0};
  BramBiosOps ops = make_ops(&fake);
  uint8_t dirBuffer[BRM_DIR_ENTRY_BYTES * 2U];

  expect_u8(BRM_ReadDirectory(&ops, "BASIC*", dirBuffer, sizeof(dirBuffer), 4),
            BRM_RESULT_OK, "read bram directory");
  expect_u8(fake.dirCalls, 1, "dir calls");
  expect_char(fake.lastDirPattern[0], 'B', "dir pattern char");
  expect_char(fake.lastDirPattern[5], '*', "dir pattern wildcard");
  expect_u16(fake.lastDirSkip, 4, "dir skip");
  expect_u16(fake.lastDirBytes, sizeof(dirBuffer), "dir bytes");
}

int main(void) {
  filename_normalization_is_bios_safe();
  probe_maps_formatted_bram_to_storage_volume();
  probe_reports_unformatted_without_stat();
  write_constructs_normal_file_info_blocks();
  read_checks_search_size_before_bios_read();
  directory_call_passes_pattern_skip_and_size();

  if (failures) {
    printf("bram tests failed: %d\n", failures);
    return 1;
  }

  printf("bram tests passed\n");
  return 0;
}
