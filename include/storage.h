/*
 * storage.h - SegaOS storage policy helpers.
 *
 * This is not a hardware BRAM driver. It is the small, testable policy layer
 * that keeps app saves honest while the hardware probe/API is still pending.
 */

#ifndef STORAGE_H
#define STORAGE_H

#include "common.h"
#include "sega_os.h"
#include <stdint.h>

#define STG_INTERNAL_BRAM_BYTES BRAM_INTERNAL_SIZE
#define STG_EXTERNAL_CART_STANDARD_BYTES BRAM_CART_SIZE

#define STG_INTERNAL_BRAM_RESERVE_BYTES 512U
#define STG_EXTERNAL_CART_RESERVE_BYTES 1024U
#define STG_INTERNAL_PREF_LIMIT_BYTES 1024U
#define STG_INTERNAL_TEXT_LIMIT_BYTES 2048U
#define STG_INTERNAL_BASIC_LIMIT_BYTES 2048U

typedef enum {
  STG_VOLUME_NONE = 0,
  STG_VOLUME_INTERNAL_BRAM = 1,
  STG_VOLUME_EXTERNAL_CART = 2
} StorageVolumeKind;

typedef enum {
  STG_DOC_PREFS = 0,
  STG_DOC_TEXT = 1,
  STG_DOC_BASIC = 2,
  STG_DOC_DATABASE = 3,
  STG_DOC_IMAGE = 4
} StorageDocumentKind;

typedef struct {
  StorageVolumeKind kind;
  uint8_t present;
  uint8_t writable;
  uint32_t totalBytes;
  uint32_t freeBytes;
} StorageVolumeInfo;

typedef struct {
  StorageVolumeKind target;
  uint8_t canSave;
  uint8_t fallbackUsed;
  uint16_t reserveBytes;
  uint32_t usableFreeBytes;
} StorageSavePlan;

void STG_InitVolume(StorageVolumeInfo *volume, StorageVolumeKind kind,
                    uint8_t present, uint8_t writable, uint32_t totalBytes,
                    uint32_t freeBytes);
void STG_InitInternalBram(StorageVolumeInfo *volume, uint32_t freeBytes);
void STG_InitExternalCart(StorageVolumeInfo *volume, uint32_t totalBytes,
                          uint32_t freeBytes);
uint8_t STG_VolumeWritable(const StorageVolumeInfo *volume);
uint32_t STG_InternalFallbackLimit(StorageDocumentKind kind);
uint8_t STG_PlanSave(const StorageVolumeInfo *externalCart,
                     const StorageVolumeInfo *internalBram,
                     StorageDocumentKind kind, uint32_t documentBytes,
                     StorageSavePlan *plan);

#endif /* STORAGE_H */
