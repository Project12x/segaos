#include "storage.h"

static uint32_t stg_clamp_free(uint32_t totalBytes, uint32_t freeBytes) {
  if (freeBytes > totalBytes)
    return totalBytes;
  return freeBytes;
}

static void stg_clear_plan(StorageSavePlan *plan) {
  if (!plan)
    return;

  plan->target = STG_VOLUME_NONE;
  plan->canSave = 0;
  plan->fallbackUsed = 0;
  plan->reserveBytes = 0;
  plan->usableFreeBytes = 0;
}

void STG_InitVolume(StorageVolumeInfo *volume, StorageVolumeKind kind,
                    uint8_t present, uint8_t writable, uint32_t totalBytes,
                    uint32_t freeBytes) {
  if (!volume)
    return;

  volume->kind = kind;
  volume->present = present ? 1 : 0;
  volume->writable = writable ? 1 : 0;
  volume->totalBytes = totalBytes;
  volume->freeBytes = stg_clamp_free(totalBytes, freeBytes);

  if (!volume->present || totalBytes == 0) {
    volume->present = 0;
    volume->writable = 0;
    volume->freeBytes = 0;
  }
}

void STG_InitInternalBram(StorageVolumeInfo *volume, uint32_t freeBytes) {
  STG_InitVolume(volume, STG_VOLUME_INTERNAL_BRAM, 1, 1,
                 STG_INTERNAL_BRAM_BYTES, freeBytes);
}

void STG_InitExternalCart(StorageVolumeInfo *volume, uint32_t totalBytes,
                          uint32_t freeBytes) {
  STG_InitVolume(volume, STG_VOLUME_EXTERNAL_CART, totalBytes != 0, 1,
                 totalBytes, freeBytes);
}

uint8_t STG_VolumeWritable(const StorageVolumeInfo *volume) {
  return (uint8_t)(volume && volume->present && volume->writable &&
                   volume->totalBytes > 0);
}

uint32_t STG_InternalFallbackLimit(StorageDocumentKind kind) {
  switch (kind) {
  case STG_DOC_PREFS:
    return STG_INTERNAL_PREF_LIMIT_BYTES;
  case STG_DOC_TEXT:
    return STG_INTERNAL_TEXT_LIMIT_BYTES;
  case STG_DOC_BASIC:
    return STG_INTERNAL_BASIC_LIMIT_BYTES;
  case STG_DOC_DATABASE:
  case STG_DOC_IMAGE:
  default:
    return 0;
  }
}

static uint8_t stg_volume_has_space(const StorageVolumeInfo *volume,
                                    uint16_t reserveBytes,
                                    uint32_t documentBytes,
                                    uint32_t *usableFreeBytes) {
  uint32_t usable;

  if (usableFreeBytes)
    *usableFreeBytes = 0;
  if (!STG_VolumeWritable(volume) || documentBytes == 0)
    return 0;
  if (volume->freeBytes <= reserveBytes)
    return 0;

  usable = volume->freeBytes - reserveBytes;
  if (usableFreeBytes)
    *usableFreeBytes = usable;

  return (uint8_t)(usable >= documentBytes);
}

uint8_t STG_PlanSave(const StorageVolumeInfo *externalCart,
                     const StorageVolumeInfo *internalBram,
                     StorageDocumentKind kind, uint32_t documentBytes,
                     StorageSavePlan *plan) {
  uint32_t usable;
  uint32_t internalLimit;

  stg_clear_plan(plan);
  if (!plan || documentBytes == 0)
    return 0;

  if (stg_volume_has_space(externalCart, STG_EXTERNAL_CART_RESERVE_BYTES,
                           documentBytes, &usable)) {
    plan->target = STG_VOLUME_EXTERNAL_CART;
    plan->canSave = 1;
    plan->fallbackUsed = 0;
    plan->reserveBytes = STG_EXTERNAL_CART_RESERVE_BYTES;
    plan->usableFreeBytes = usable;
    return 1;
  }

  internalLimit = STG_InternalFallbackLimit(kind);
  if (internalLimit == 0 || documentBytes > internalLimit)
    return 0;

  if (stg_volume_has_space(internalBram, STG_INTERNAL_BRAM_RESERVE_BYTES,
                           documentBytes, &usable)) {
    plan->target = STG_VOLUME_INTERNAL_BRAM;
    plan->canSave = 1;
    plan->fallbackUsed = 1;
    plan->reserveBytes = STG_INTERNAL_BRAM_RESERVE_BYTES;
    plan->usableFreeBytes = usable;
    return 1;
  }

  return 0;
}
