#include "basic_storage.h"

static void bas_storage_clear_plan(StorageSavePlan *plan) {
  if (!plan)
    return;

  plan->target = STG_VOLUME_NONE;
  plan->canSave = 0;
  plan->fallbackUsed = 0;
  plan->reserveBytes = 0;
  plan->usableFreeBytes = 0;
}

static void bas_storage_clear_io(BasicStorageIO *io) {
  if (!io)
    return;

  io->save = (BasicProgramSaveSink)0;
  io->load = (BasicProgramLoadSource)0;
  io->user = (void *)0;
  io->imageBuffer = (uint8_t *)0;
  io->imageBufferBytes = 0;
}

static uint8_t bas_storage_volume_present(const StorageVolumeInfo *volume) {
  return (uint8_t)(volume && volume->present && volume->totalBytes > 0);
}

void BAS_StorageInitAdapter(BasicStorageAdapter *adapter,
                            const StorageVolumeInfo *externalCart,
                            const StorageVolumeInfo *internalBram,
                            BasicStorageWrite write, BasicStorageRead read,
                            void *user) {
  if (!adapter)
    return;

  adapter->externalCart = externalCart;
  adapter->internalBram = internalBram;
  adapter->write = write;
  adapter->read = read;
  adapter->user = user;
  bas_storage_clear_plan(&adapter->lastSavePlan);
  adapter->lastLoadTarget = STG_VOLUME_NONE;
}

uint8_t BAS_StorageBindIO(BasicStorageAdapter *adapter, uint8_t *imageBuffer,
                          uint16_t imageBufferBytes, BasicStorageIO *out) {
  bas_storage_clear_io(out);
  if (!adapter || !out || !imageBuffer || imageBufferBytes == 0)
    return 0;

  out->save = BAS_StorageSaveProgramImage;
  out->load = BAS_StorageLoadProgramImage;
  out->user = adapter;
  out->imageBuffer = imageBuffer;
  out->imageBufferBytes = imageBufferBytes;
  return 1;
}

uint8_t BAS_StorageSaveProgramImage(const uint8_t *image, uint16_t imageBytes,
                                    void *user) {
  BasicStorageAdapter *adapter = (BasicStorageAdapter *)user;

  if (!adapter)
    return 0;
  bas_storage_clear_plan(&adapter->lastSavePlan);
  if (!image || imageBytes == 0 || !adapter->write)
    return 0;

  if (!STG_PlanSave(adapter->externalCart, adapter->internalBram,
                    STG_DOC_BASIC, imageBytes, &adapter->lastSavePlan))
    return 0;

  return adapter->write(adapter->lastSavePlan.target, image, imageBytes,
                        &adapter->lastSavePlan, adapter->user);
}

uint8_t BAS_StorageLoadProgramImage(uint8_t *image,
                                    uint16_t imageBufferBytes,
                                    uint16_t *imageBytes, void *user) {
  BasicStorageAdapter *adapter = (BasicStorageAdapter *)user;
  StorageVolumeKind target = STG_VOLUME_NONE;

  if (imageBytes)
    *imageBytes = 0;
  if (!adapter)
    return 0;
  adapter->lastLoadTarget = STG_VOLUME_NONE;
  if (!image || imageBufferBytes == 0 || !imageBytes || !adapter->read)
    return 0;

  if (bas_storage_volume_present(adapter->externalCart)) {
    target = STG_VOLUME_EXTERNAL_CART;
  } else if (bas_storage_volume_present(adapter->internalBram)) {
    target = STG_VOLUME_INTERNAL_BRAM;
  } else {
    return 0;
  }

  if (!adapter->read(target, image, imageBufferBytes, imageBytes,
                     adapter->user))
    return 0;

  adapter->lastLoadTarget = target;
  return 1;
}

const StorageSavePlan *
BAS_StorageLastSavePlan(const BasicStorageAdapter *adapter) {
  if (!adapter)
    return (const StorageSavePlan *)0;
  return &adapter->lastSavePlan;
}

StorageVolumeKind BAS_StorageLastLoadTarget(const BasicStorageAdapter *adapter) {
  if (!adapter)
    return STG_VOLUME_NONE;
  return adapter->lastLoadTarget;
}
