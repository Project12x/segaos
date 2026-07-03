#include "external_cart.h"
#include "storage.h"
#include <stdio.h>

static int failures;

typedef struct {
  uint8_t called;
  uint8_t ok;
  ExternalCartProbeResult result;
} FakeCartProbe;

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

static void expect_u32(uint32_t actual, uint32_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %lu got %lu\n", name, (unsigned long)expected,
           (unsigned long)actual);
    failures++;
  }
}

static uint8_t fake_probe(ExternalCartProbeResult *result, void *user) {
  FakeCartProbe *fake = (FakeCartProbe *)user;

  fake->called++;
  if (result)
    *result = fake->result;
  return fake->ok;
}

static void writable_probe_maps_to_external_volume(void) {
  FakeCartProbe fake;
  ExternalCartOps ops;
  StorageVolumeInfo volume;

  fake.called = 0;
  fake.ok = 1;
  fake.result.present = 1;
  fake.result.writable = 1;
  fake.result.totalBytes = 256UL * 1024UL;
  fake.result.freeBytes = 128UL * 1024UL;
  ops.probe = fake_probe;
  ops.user = &fake;

  expect_true(EXTC_ProbeVolume(&ops, &volume), "probe succeeds");
  expect_u8(fake.called, 1, "probe call count");
  expect_u8(volume.kind, STG_VOLUME_EXTERNAL_CART, "external volume kind");
  expect_true(volume.present, "external volume present");
  expect_true(volume.writable, "external volume writable");
  expect_u32(volume.totalBytes, 256UL * 1024UL, "external total bytes");
  expect_u32(volume.freeBytes, 128UL * 1024UL, "external free bytes");
}

static void missing_or_failed_probe_maps_to_absent_volume(void) {
  FakeCartProbe fake;
  ExternalCartOps ops;
  StorageVolumeInfo volume;

  expect_false(EXTC_ProbeVolume(0, &volume), "null ops probe fails");
  expect_u8(volume.kind, STG_VOLUME_EXTERNAL_CART, "null ops volume kind");
  expect_false(volume.present, "null ops volume absent");
  expect_false(volume.writable, "null ops volume unwritable");
  expect_u32(volume.totalBytes, 0, "null ops total bytes");
  expect_u32(volume.freeBytes, 0, "null ops free bytes");

  fake.called = 0;
  fake.ok = 0;
  fake.result.present = 1;
  fake.result.writable = 1;
  fake.result.totalBytes = STG_EXTERNAL_CART_STANDARD_BYTES;
  fake.result.freeBytes = STG_EXTERNAL_CART_STANDARD_BYTES;
  ops.probe = fake_probe;
  ops.user = &fake;

  expect_false(EXTC_ProbeVolume(&ops, &volume), "failed probe returns false");
  expect_u8(fake.called, 1, "failed probe call count");
  expect_false(volume.present, "failed probe volume absent");
  expect_false(volume.writable, "failed probe volume unwritable");
}

static void impossible_free_count_is_clamped_to_total(void) {
  ExternalCartProbeResult result;
  StorageVolumeInfo volume;

  result.present = 1;
  result.writable = 1;
  result.totalBytes = STG_EXTERNAL_CART_STANDARD_BYTES;
  result.freeBytes = STG_EXTERNAL_CART_STANDARD_BYTES + 4096U;

  expect_true(EXTC_MapProbeResultToVolume(&result, &volume),
              "mapped clamped result is present");
  expect_u32(volume.freeBytes, STG_EXTERNAL_CART_STANDARD_BYTES,
             "external free bytes clamped");
}

static void readonly_cart_is_present_but_not_writable(void) {
  ExternalCartProbeResult result;
  StorageVolumeInfo volume;

  result.present = 1;
  result.writable = 0;
  result.totalBytes = STG_EXTERNAL_CART_STANDARD_BYTES;
  result.freeBytes = STG_EXTERNAL_CART_STANDARD_BYTES / 2U;

  expect_true(EXTC_MapProbeResultToVolume(&result, &volume),
              "readonly cart is detected");
  expect_true(volume.present, "readonly cart present");
  expect_false(volume.writable, "readonly cart writable flag");
  expect_false(STG_VolumeWritable(&volume), "readonly cart policy writable");
}

static void probed_cart_enables_large_document_save_plan(void) {
  ExternalCartProbeResult result;
  StorageVolumeInfo cart;
  StorageVolumeInfo internal;
  StorageSavePlan plan;

  result.present = 1;
  result.writable = 1;
  result.totalBytes = 256UL * 1024UL;
  result.freeBytes = 192UL * 1024UL;
  STG_InitInternalBram(&internal, 6U * 1024U);

  expect_true(EXTC_MapProbeResultToVolume(&result, &cart),
              "cart probe maps before planning");
  expect_true(STG_PlanSave(&cart, &internal, STG_DOC_IMAGE, 64UL * 1024UL,
                           &plan),
              "large image save can use cart");
  expect_u8(plan.target, STG_VOLUME_EXTERNAL_CART, "large image target");
  expect_false(plan.fallbackUsed, "large image not internal fallback");
}

int main(void) {
  writable_probe_maps_to_external_volume();
  missing_or_failed_probe_maps_to_absent_volume();
  impossible_free_count_is_clamped_to_total();
  readonly_cart_is_present_but_not_writable();
  probed_cart_enables_large_document_save_plan();

  if (failures) {
    printf("external cart probe tests failed: %d\n", failures);
    return 1;
  }

  printf("external cart probe tests passed\n");
  return 0;
}
