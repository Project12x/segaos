/*
 * basic_storage.h - BASIC storage adapter for SegaOS policy-backed saves.
 *
 * This bridges the generic BASIC `SAVE`/`LOAD` byte callbacks to the storage
 * policy layer. It still is not a BRAM hardware driver.
 */

#ifndef BASIC_STORAGE_H
#define BASIC_STORAGE_H

#include "basic.h"
#include "storage.h"
#include <stdint.h>

typedef uint8_t (*BasicStorageWrite)(StorageVolumeKind target,
                                     const uint8_t *image,
                                     uint16_t imageBytes,
                                     const StorageSavePlan *plan, void *user);
typedef uint8_t (*BasicStorageRead)(StorageVolumeKind target, uint8_t *image,
                                    uint16_t imageBufferBytes,
                                    uint16_t *imageBytes, void *user);

typedef struct {
  const StorageVolumeInfo *externalCart;
  const StorageVolumeInfo *internalBram;
  BasicStorageWrite write;
  BasicStorageRead read;
  void *user;
  StorageSavePlan lastSavePlan;
  StorageVolumeKind lastLoadTarget;
} BasicStorageAdapter;

void BAS_StorageInitAdapter(BasicStorageAdapter *adapter,
                            const StorageVolumeInfo *externalCart,
                            const StorageVolumeInfo *internalBram,
                            BasicStorageWrite write, BasicStorageRead read,
                            void *user);
uint8_t BAS_StorageBindIO(BasicStorageAdapter *adapter, uint8_t *imageBuffer,
                          uint16_t imageBufferBytes, BasicStorageIO *out);
uint8_t BAS_StorageSaveProgramImage(const uint8_t *image, uint16_t imageBytes,
                                    void *user);
uint8_t BAS_StorageLoadProgramImage(uint8_t *image,
                                    uint16_t imageBufferBytes,
                                    uint16_t *imageBytes, void *user);
const StorageSavePlan *
BAS_StorageLastSavePlan(const BasicStorageAdapter *adapter);
StorageVolumeKind BAS_StorageLastLoadTarget(const BasicStorageAdapter *adapter);

#endif /* BASIC_STORAGE_H */
