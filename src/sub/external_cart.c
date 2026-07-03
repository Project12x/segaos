#include "external_cart.h"

static void extc_init_absent_volume(StorageVolumeInfo *volume) {
  STG_InitVolume(volume, STG_VOLUME_EXTERNAL_CART, 0, 0, 0, 0);
}

uint8_t EXTC_MapProbeResultToVolume(const ExternalCartProbeResult *result,
                                    StorageVolumeInfo *volume) {
  if (!volume)
    return 0;

  if (!result || !result->present || result->totalBytes == 0) {
    extc_init_absent_volume(volume);
    return 0;
  }

  STG_InitVolume(volume, STG_VOLUME_EXTERNAL_CART, 1, result->writable,
                 result->totalBytes, result->freeBytes);
  return volume->present;
}

uint8_t EXTC_ProbeVolume(const ExternalCartOps *ops,
                         StorageVolumeInfo *volume) {
  ExternalCartProbeResult result = {0, 0, 0, 0};

  if (!volume)
    return 0;

  extc_init_absent_volume(volume);
  if (!ops || !ops->probe)
    return 0;
  if (!ops->probe(&result, ops->user))
    return 0;

  return EXTC_MapProbeResultToVolume(&result, volume);
}
