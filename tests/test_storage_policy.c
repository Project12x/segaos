#include "storage.h"
#include <stdio.h>

static int failures;

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

static void external_cart_is_preferred_for_documents(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  StorageSavePlan plan;

  STG_InitInternalBram(&internal, 6U * 1024U);
  STG_InitExternalCart(&cart, STG_EXTERNAL_CART_STANDARD_BYTES,
                       96UL * 1024UL);

  expect_true(STG_PlanSave(&cart, &internal, STG_DOC_BASIC, 4096U, &plan),
              "basic save can be planned");
  expect_u8(plan.target, STG_VOLUME_EXTERNAL_CART, "basic save target");
  expect_false(plan.fallbackUsed, "cart save is not fallback");
  expect_u16(plan.reserveBytes, STG_EXTERNAL_CART_RESERVE_BYTES,
             "cart reserve");
  expect_u32(plan.usableFreeBytes, (96UL * 1024UL) - 1024UL,
             "cart usable free");
}

static void internal_bram_is_tiny_document_fallback(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  StorageSavePlan plan;

  STG_InitInternalBram(&internal, 5U * 1024U);
  STG_InitVolume(&cart, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);

  expect_true(STG_PlanSave(&cart, &internal, STG_DOC_TEXT, 1024U, &plan),
              "tiny text save falls back to internal");
  expect_u8(plan.target, STG_VOLUME_INTERNAL_BRAM, "tiny text fallback target");
  expect_true(plan.fallbackUsed, "internal text save is fallback");
  expect_u16(plan.reserveBytes, STG_INTERNAL_BRAM_RESERVE_BYTES,
             "internal reserve");
}

static void internal_bram_rejects_large_documents(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  StorageSavePlan plan;

  STG_InitInternalBram(&internal, 7U * 1024U);
  STG_InitVolume(&cart, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);

  expect_false(STG_PlanSave(&cart, &internal, STG_DOC_BASIC, 4096U, &plan),
               "large basic save rejected without cart");
  expect_u8(plan.target, STG_VOLUME_NONE, "large rejected target");
}

static void cart_requires_reserved_free_space(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  StorageSavePlan plan;

  STG_InitInternalBram(&internal, 0);
  STG_InitExternalCart(&cart, STG_EXTERNAL_CART_STANDARD_BYTES, 4096U);

  expect_false(STG_PlanSave(&cart, &internal, STG_DOC_TEXT, 4096U, &plan),
               "cart save rejected without reserve");
  expect_u8(plan.target, STG_VOLUME_NONE, "reserve rejected target");
}

static void image_documents_require_external_cart(void) {
  StorageVolumeInfo internal;
  StorageVolumeInfo cart;
  StorageSavePlan plan;

  STG_InitInternalBram(&internal, 7U * 1024U);
  STG_InitVolume(&cart, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);

  expect_false(STG_PlanSave(&cart, &internal, STG_DOC_IMAGE, 512U, &plan),
               "image save rejected without cart");
  expect_u8(plan.target, STG_VOLUME_NONE, "image rejected target");
}

int main(void) {
  external_cart_is_preferred_for_documents();
  internal_bram_is_tiny_document_fallback();
  internal_bram_rejects_large_documents();
  cart_requires_reserved_free_space();
  image_documents_require_external_cart();

  if (failures) {
    printf("storage policy tests failed: %d\n", failures);
    return 1;
  }

  printf("storage policy tests passed\n");
  return 0;
}
