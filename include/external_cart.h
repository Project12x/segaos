/*
 * external_cart.h - External Backup RAM cartridge probe boundary.
 *
 * This layer intentionally does not encode raw BIOS or bus access. It converts
 * a proven hardware probe result into the storage policy model, so the file
 * manager and BASIC runtime can be built before the final adapter is wired.
 */

#ifndef EXTERNAL_CART_H
#define EXTERNAL_CART_H

#include "storage.h"
#include <stdint.h>

typedef struct {
  uint8_t present;
  uint8_t writable;
  uint32_t totalBytes;
  uint32_t freeBytes;
} ExternalCartProbeResult;

typedef uint8_t (*ExternalCartProbeFn)(ExternalCartProbeResult *result,
                                       void *user);

typedef struct {
  ExternalCartProbeFn probe;
  void *user;
} ExternalCartOps;

uint8_t EXTC_MapProbeResultToVolume(const ExternalCartProbeResult *result,
                                    StorageVolumeInfo *volume);
uint8_t EXTC_ProbeVolume(const ExternalCartOps *ops,
                         StorageVolumeInfo *volume);

#endif /* EXTERNAL_CART_H */
