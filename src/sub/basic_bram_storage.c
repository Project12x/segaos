#include "basic_bram_storage.h"

static void bas_bram_clear_volume(StorageVolumeInfo *volume) {
  STG_InitVolume(volume, STG_VOLUME_INTERNAL_BRAM, 0, 0, 0, 0);
}

static void bas_bram_clear_probe(BramProbeResult *probe) {
  if (!probe)
    return;

  probe->present = 0;
  probe->formatted = 0;
  probe->status = BRM_FORMAT_NO_RAM;
  probe->totalBlocks4K = 0;
  probe->freeBlocks4K = 0;
  probe->fileCount = 0;
  probe->totalBytes = 0;
  probe->freeBytes = 0;
}

static void bas_bram_clear_filename(BramFilename *filename) {
  if (!filename)
    return;

  for (uint8_t i = 0; i < BRM_FILENAME_BYTES; i++) {
    filename->bytes[i] = 0;
  }
}

static void bas_bram_clear_io_buffer(BasicBramStorage *storage) {
  if (!storage)
    return;

  for (uint16_t i = 0; i < STG_INTERNAL_BASIC_LIMIT_BYTES; i++) {
    storage->ioBuffer[i] = 0;
  }
}

static uint16_t bas_bram_padded_write_bytes(uint16_t imageBytes) {
  uint16_t blocks;
  uint32_t padded;

  if (imageBytes == 0 || imageBytes > STG_INTERNAL_BASIC_LIMIT_BYTES)
    return 0;

  blocks = BRM_NormalBlocksForBytes(imageBytes);
  padded = BRM_ModeBytes(blocks, BRM_FILE_MODE_NORMAL);
  if (padded == 0 || padded > STG_INTERNAL_BASIC_LIMIT_BYTES)
    return 0;

  return (uint16_t)padded;
}

uint8_t BAS_BramStorageInit(BasicBramStorage *storage, const BramBiosOps *ops,
                            const char *filename) {
  const char *source = filename ? filename : BAS_BRAM_DEFAULT_FILENAME;

  if (!storage)
    return 0;

  storage->ops = (const BramBiosOps *)0;
  bas_bram_clear_filename(&storage->filename);
  bas_bram_clear_probe(&storage->lastProbe);
  bas_bram_clear_volume(&storage->internalVolume);
  bas_bram_clear_io_buffer(storage);

  if (!ops || !BRM_MakeFilename(source, &storage->filename))
    return 0;

  storage->ops = ops;
  return 1;
}

uint8_t BAS_BramStorageProbeInternal(BasicBramStorage *storage) {
  BramResult result;

  if (!storage || !storage->ops)
    return 0;

  bas_bram_clear_probe(&storage->lastProbe);
  bas_bram_clear_volume(&storage->internalVolume);
  result = BRM_Probe(storage->ops, &storage->lastProbe);
  BRM_InitInternalVolumeFromProbe(&storage->lastProbe,
                                  &storage->internalVolume);
  return (uint8_t)(result == BRM_RESULT_OK);
}

const StorageVolumeInfo *
BAS_BramStorageInternalVolume(const BasicBramStorage *storage) {
  if (!storage)
    return (const StorageVolumeInfo *)0;
  return &storage->internalVolume;
}

uint8_t BAS_BramStorageWrite(StorageVolumeKind target, const uint8_t *image,
                             uint16_t imageBytes,
                             const StorageSavePlan *plan, void *user) {
  BasicBramStorage *storage = (BasicBramStorage *)user;
  uint16_t paddedBytes;

  if (!storage || !storage->ops || target != STG_VOLUME_INTERNAL_BRAM ||
      !image || !plan || !plan->canSave ||
      plan->target != STG_VOLUME_INTERNAL_BRAM)
    return 0;

  paddedBytes = bas_bram_padded_write_bytes(imageBytes);
  if (paddedBytes == 0)
    return 0;

  for (uint16_t i = 0; i < paddedBytes; i++) {
    storage->ioBuffer[i] = (i < imageBytes) ? image[i] : 0;
  }

  return (uint8_t)(BRM_WriteFile(storage->ops, &storage->filename,
                                 storage->ioBuffer,
                                 paddedBytes) == BRM_RESULT_OK);
}

uint8_t BAS_BramStorageRead(StorageVolumeKind target, uint8_t *image,
                            uint16_t imageBufferBytes, uint16_t *imageBytes,
                            void *user) {
  BasicBramStorage *storage = (BasicBramStorage *)user;
  uint16_t bytesRead = 0;

  if (imageBytes)
    *imageBytes = 0;
  if (!storage || !storage->ops || target != STG_VOLUME_INTERNAL_BRAM ||
      !image || imageBufferBytes == 0 || !imageBytes)
    return 0;

  if (BRM_ReadFile(storage->ops, &storage->filename, image, imageBufferBytes,
                   &bytesRead) != BRM_RESULT_OK)
    return 0;

  *imageBytes = bytesRead;
  return 1;
}
