#include "basic.h"
#include "basic_storage.h"
#include "storage.h"
#include <stdio.h>
#include <string.h>

static int failures;

typedef struct {
  uint8_t bytes[256];
  uint16_t bytesUsed;
  uint8_t writeCalls;
  uint8_t readCalls;
  StorageVolumeKind lastWriteTarget;
  StorageVolumeKind lastReadTarget;
  StorageSavePlan lastWritePlan;
} BasicStorageDevice;

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

static uint8_t capture_basic_write(StorageVolumeKind target,
                                   const uint8_t *image, uint16_t imageBytes,
                                   const StorageSavePlan *plan, void *user) {
  BasicStorageDevice *device = (BasicStorageDevice *)user;

  if (!device || !image || !plan || imageBytes > sizeof(device->bytes))
    return 0;

  for (uint16_t i = 0; i < imageBytes; i++) {
    device->bytes[i] = image[i];
  }
  device->bytesUsed = imageBytes;
  device->writeCalls++;
  device->lastWriteTarget = target;
  device->lastWritePlan = *plan;
  return 1;
}

static uint8_t feed_basic_read(StorageVolumeKind target, uint8_t *image,
                               uint16_t imageBufferBytes,
                               uint16_t *imageBytes, void *user) {
  BasicStorageDevice *device = (BasicStorageDevice *)user;

  if (imageBytes)
    *imageBytes = 0;
  if (!device || !image || !imageBytes || device->bytesUsed > imageBufferBytes)
    return 0;

  for (uint16_t i = 0; i < device->bytesUsed; i++) {
    image[i] = device->bytes[i];
  }
  *imageBytes = device->bytesUsed;
  device->readCalls++;
  device->lastReadTarget = target;
  return 1;
}

static void save_prefers_external_cart_for_basic_images(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  BasicStorageAdapter adapter;
  BasicStorageDevice device = {0};
  BasicStorageIO io;
  uint8_t scratch[128];
  uint8_t payload[32] = {'S', 'B', 'A', 'S'};

  STG_InitInternalBram(&internal, 7U * 1024U);
  STG_InitExternalCart(&cart, STG_EXTERNAL_CART_STANDARD_BYTES,
                       96UL * 1024UL);
  BAS_StorageInitAdapter(&adapter, &cart, &internal, capture_basic_write,
                         feed_basic_read, &device);
  expect_true(BAS_StorageBindIO(&adapter, scratch, sizeof(scratch), &io),
              "bind external cart basic storage IO");

  expect_true(io.save(payload, sizeof(payload), io.user),
              "save through storage adapter");
  expect_u8(device.writeCalls, 1, "external save write calls");
  expect_u8(device.lastWriteTarget, STG_VOLUME_EXTERNAL_CART,
            "external save target");
  expect_u8(device.lastWritePlan.fallbackUsed, 0, "external save fallback");
  expect_u16(device.bytesUsed, sizeof(payload), "external save byte count");
}

static void save_falls_back_to_internal_for_tiny_basic_images(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  BasicStorageAdapter adapter;
  BasicStorageDevice device = {0};
  BasicStorageIO io;
  uint8_t scratch[128];
  uint8_t payload[32] = {'S', 'B', 'A', 'S'};

  STG_InitInternalBram(&internal, 7U * 1024U);
  STG_InitVolume(&cart, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);
  BAS_StorageInitAdapter(&adapter, &cart, &internal, capture_basic_write,
                         feed_basic_read, &device);
  expect_true(BAS_StorageBindIO(&adapter, scratch, sizeof(scratch), &io),
              "bind internal fallback basic storage IO");

  expect_true(io.save(payload, sizeof(payload), io.user),
              "save through internal fallback adapter");
  expect_u8(device.writeCalls, 1, "internal save write calls");
  expect_u8(device.lastWriteTarget, STG_VOLUME_INTERNAL_BRAM,
            "internal save target");
  expect_u8(device.lastWritePlan.fallbackUsed, 1, "internal save fallback");
}

static void save_rejects_large_basic_image_without_external_cart(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  BasicStorageAdapter adapter;
  BasicStorageDevice device = {0};
  uint8_t payload[STG_INTERNAL_BASIC_LIMIT_BYTES + 1U];

  STG_InitInternalBram(&internal, 7U * 1024U);
  STG_InitVolume(&cart, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);
  BAS_StorageInitAdapter(&adapter, &cart, &internal, capture_basic_write,
                         feed_basic_read, &device);

  expect_false(BAS_StorageSaveProgramImage(payload, sizeof(payload),
                                           &adapter),
               "reject large basic image without cart");
  expect_u8(device.writeCalls, 0, "large rejected write calls");
  expect_u8(BAS_StorageLastSavePlan(&adapter)->target, STG_VOLUME_NONE,
            "large rejected target");
}

static void shell_save_uses_storage_policy_adapter(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  BasicStorageAdapter adapter;
  BasicStorageDevice device = {0};
  BasicStorageIO io;
  BasicLine lines[4];
  uint8_t storage[96];
  uint8_t scratch[192];
  BasicProgram program;
  BasicCommandResult result;
  char lineBuffer[48];

  STG_InitInternalBram(&internal, 7U * 1024U);
  STG_InitExternalCart(&cart, STG_EXTERNAL_CART_STANDARD_BYTES,
                       96UL * 1024UL);
  BAS_StorageInitAdapter(&adapter, &cart, &internal, capture_basic_write,
                         feed_basic_read, &device);
  expect_true(BAS_StorageBindIO(&adapter, scratch, sizeof(scratch), &io),
              "bind shell save storage IO");

  BAS_InitProgram(&program, lines, 4, storage, sizeof(storage));
  expect_true(BAS_StoreSourceLine(&program, "10 PRINT \"CART\""),
              "store program before adapter SAVE");

  expect_true(BAS_SubmitConsoleLineWithStorage(
                  &program, "SAVE", 0, 0, lineBuffer, sizeof(lineBuffer), &io,
                  &result),
              "shell SAVE through storage adapter");
  expect_u8(result.kind, BAS_CMD_SAVE, "adapter save command kind");
  expect_u16(result.bytesTransferred, BAS_ProgramImageSize(&program),
             "adapter save image bytes");
  expect_u8(device.lastWriteTarget, STG_VOLUME_EXTERNAL_CART,
            "adapter shell save target");
}

static void shell_load_uses_external_cart_before_internal(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  BasicStorageAdapter adapter;
  BasicStorageDevice device = {0};
  BasicStorageIO io;
  BasicLine sourceLines[4];
  BasicLine destLines[4];
  uint8_t sourceStorage[96];
  uint8_t destStorage[96];
  uint8_t scratch[192];
  BasicProgram source;
  BasicProgram dest;
  BasicCommandResult result;
  uint16_t written = 0;
  char lineBuffer[48];

  STG_InitInternalBram(&internal, 7U * 1024U);
  STG_InitExternalCart(&cart, STG_EXTERNAL_CART_STANDARD_BYTES,
                       96UL * 1024UL);
  BAS_StorageInitAdapter(&adapter, &cart, &internal, capture_basic_write,
                         feed_basic_read, &device);
  expect_true(BAS_StorageBindIO(&adapter, scratch, sizeof(scratch), &io),
              "bind shell load storage IO");

  BAS_InitProgram(&source, sourceLines, 4, sourceStorage,
                  sizeof(sourceStorage));
  BAS_InitProgram(&dest, destLines, 4, destStorage, sizeof(destStorage));
  expect_true(BAS_StoreSourceLine(&source, "10 PRINT \"LOAD\""),
              "store source before adapter LOAD");
  expect_true(BAS_ExportProgramImage(&source, device.bytes,
                                     sizeof(device.bytes), &written),
              "prepare adapter load image");
  device.bytesUsed = written;

  expect_true(BAS_SubmitConsoleLineWithStorage(
                  &dest, "LOAD", 0, 0, lineBuffer, sizeof(lineBuffer), &io,
                  &result),
              "shell LOAD through storage adapter");
  expect_u8(result.kind, BAS_CMD_LOAD, "adapter load command kind");
  expect_u16(result.bytesTransferred, written, "adapter load image bytes");
  expect_u8(device.lastReadTarget, STG_VOLUME_EXTERNAL_CART,
            "adapter shell load target");
  expect_u8(BAS_StorageLastLoadTarget(&adapter), STG_VOLUME_EXTERNAL_CART,
            "adapter remembered load target");
}

int main(void) {
  save_prefers_external_cart_for_basic_images();
  save_falls_back_to_internal_for_tiny_basic_images();
  save_rejects_large_basic_image_without_external_cart();
  shell_save_uses_storage_policy_adapter();
  shell_load_uses_external_cart_before_internal();

  if (failures) {
    printf("basic storage tests failed: %d\n", failures);
    return 1;
  }

  printf("basic storage tests passed\n");
  return 0;
}
