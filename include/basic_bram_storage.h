/*
 * basic_bram_storage.h - BASIC storage callbacks backed by internal BRAM.
 *
 * This binds BASIC's generic SAVE/LOAD byte callbacks to the BRAM wrapper
 * contract. It does not call the Sega CD BIOS vector directly.
 */

#ifndef BASIC_BRAM_STORAGE_H
#define BASIC_BRAM_STORAGE_H

#include "basic_storage.h"
#include "bram.h"
#include "storage.h"
#include <stdint.h>

#define BAS_BRAM_DEFAULT_FILENAME "BASIC"

typedef struct {
  const BramBiosOps *ops;
  BramFilename filename;
  BramProbeResult lastProbe;
  StorageVolumeInfo internalVolume;
  uint8_t ioBuffer[STG_INTERNAL_BASIC_LIMIT_BYTES];
} BasicBramStorage;

uint8_t BAS_BramStorageInit(BasicBramStorage *storage,
                            const BramBiosOps *ops, const char *filename);
uint8_t BAS_BramStorageProbeInternal(BasicBramStorage *storage);
const StorageVolumeInfo *
BAS_BramStorageInternalVolume(const BasicBramStorage *storage);
uint8_t BAS_BramStorageWrite(StorageVolumeKind target, const uint8_t *image,
                             uint16_t imageBytes,
                             const StorageSavePlan *plan, void *user);
uint8_t BAS_BramStorageRead(StorageVolumeKind target, uint8_t *image,
                            uint16_t imageBufferBytes, uint16_t *imageBytes,
                            void *user);

#endif /* BASIC_BRAM_STORAGE_H */
