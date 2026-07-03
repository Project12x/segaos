#include "bram.h"

uint8_t BRM_BiosCallInit(void *workBuffer, void *stringBuffer,
                         uint16_t *totalBlocks4K, uint16_t *statusCode);
uint8_t BRM_BiosCallStat(void *stringBuffer, uint16_t *freeBlocks4K,
                         uint16_t *fileCount);
uint8_t BRM_BiosCallSearch(const BramFilename *filename, uint16_t *blocks,
                           uint8_t *mode);
uint8_t BRM_BiosCallRead(const BramFilename *filename, uint8_t *buffer,
                         uint16_t *blocks, uint8_t *mode);
uint8_t BRM_BiosCallWrite(const BramFileInfo *info, const uint8_t *data);
uint8_t BRM_BiosCallDir(const char *pattern, uint8_t *dirBuffer,
                        uint16_t skip, uint16_t dirBytes);

static void brm_clear_ops(BramBiosOps *ops) {
  if (!ops)
    return;

  ops->init = (BramBiosInit)0;
  ops->stat = (BramBiosStat)0;
  ops->search = (BramBiosSearch)0;
  ops->read = (BramBiosRead)0;
  ops->write = (BramBiosWrite)0;
  ops->dir = (BramBiosDir)0;
  ops->user = (void *)0;
}

void BRM_ClearBiosContext(BramBiosContext *context) {
  if (!context)
    return;

  for (uint16_t i = 0; i < BRM_WORK_BUFFER_BYTES; i++) {
    context->work[i] = 0;
  }
  for (uint8_t i = 0; i < BRM_STRING_BUFFER_BYTES; i++) {
    context->strings[i] = 0;
  }
}

static uint8_t brm_internal_init(uint16_t *totalBlocks4K,
                                 BramFormatStatus *status, void *user) {
  BramBiosContext *context = (BramBiosContext *)user;
  uint16_t statusCode = BRM_FORMAT_NO_RAM;

  if (!context || !totalBlocks4K || !status)
    return 0;
  if (!BRM_BiosCallInit(context->work, context->strings, totalBlocks4K,
                        &statusCode))
    return 0;

  *status = (BramFormatStatus)statusCode;
  return 1;
}

static uint8_t brm_internal_stat(uint16_t *freeBlocks4K, uint16_t *fileCount,
                                 void *user) {
  BramBiosContext *context = (BramBiosContext *)user;

  if (!context || !freeBlocks4K || !fileCount)
    return 0;
  return BRM_BiosCallStat(context->strings, freeBlocks4K, fileCount);
}

static uint8_t brm_internal_search(const BramFilename *filename,
                                   uint16_t *blocks, uint8_t *mode,
                                   void *user) {
  (void)user;
  if (!filename || !blocks || !mode)
    return 0;
  return BRM_BiosCallSearch(filename, blocks, mode);
}

static uint8_t brm_internal_read(const BramFilename *filename, uint8_t *buffer,
                                 uint16_t *blocks, uint8_t *mode,
                                 void *user) {
  (void)user;
  if (!filename || !buffer || !blocks || !mode)
    return 0;
  return BRM_BiosCallRead(filename, buffer, blocks, mode);
}

static uint8_t brm_internal_write(const BramFileInfo *info,
                                  const uint8_t *data, void *user) {
  (void)user;
  if (!info || !data)
    return 0;
  return BRM_BiosCallWrite(info, data);
}

static uint8_t brm_internal_dir(const char *pattern, uint8_t *dirBuffer,
                                uint16_t skip, uint16_t dirBytes,
                                void *user) {
  (void)user;
  if (!pattern || !dirBuffer || dirBytes == 0)
    return 0;
  return BRM_BiosCallDir(pattern, dirBuffer, skip, dirBytes);
}

uint8_t BRM_InitInternalBiosOps(BramBiosContext *context, BramBiosOps *ops) {
  brm_clear_ops(ops);
  if (!context || !ops)
    return 0;

  BRM_ClearBiosContext(context);
  ops->init = brm_internal_init;
  ops->stat = brm_internal_stat;
  ops->search = brm_internal_search;
  ops->read = brm_internal_read;
  ops->write = brm_internal_write;
  ops->dir = brm_internal_dir;
  ops->user = context;
  return 1;
}
