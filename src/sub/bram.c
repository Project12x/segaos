#include "bram.h"

static char brm_upper(char ch) {
  if (ch >= 'a' && ch <= 'z')
    return (char)(ch - ('a' - 'A'));
  return ch;
}

static uint8_t brm_filename_char_allowed(char ch) {
  return (uint8_t)((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                   ch == '_');
}

static void brm_clear_probe(BramProbeResult *out) {
  if (!out)
    return;

  out->present = 0;
  out->formatted = 0;
  out->status = BRM_FORMAT_NO_RAM;
  out->totalBlocks4K = 0;
  out->freeBlocks4K = 0;
  out->fileCount = 0;
  out->totalBytes = 0;
  out->freeBytes = 0;
}

uint8_t BRM_MakeFilename(const char *source, BramFilename *out) {
  uint8_t len = 0;

  if (!source || !out || !source[0])
    return 0;

  for (uint8_t i = 0; i < BRM_FILENAME_BYTES; i++) {
    out->bytes[i] = 0;
  }

  while (source[len]) {
    char ch;

    if (len + 1U >= BRM_FILENAME_BYTES)
      return 0;

    ch = brm_upper(source[len]);
    if (!brm_filename_char_allowed(ch))
      return 0;

    out->bytes[len] = ch;
    len++;
  }

  return (uint8_t)(len > 0);
}

uint8_t BRM_MakePattern(const char *source,
                        char out[BRM_FILENAME_BUFFER_BYTES]) {
  uint8_t len = 0;
  uint8_t wildcardSeen = 0;

  if (!source || !out || !source[0])
    return 0;

  for (uint8_t i = 0; i < BRM_FILENAME_BUFFER_BYTES; i++) {
    out[i] = 0;
  }

  while (source[len]) {
    char ch;

    if (len + 1U >= BRM_FILENAME_BUFFER_BYTES)
      return 0;

    ch = brm_upper(source[len]);
    if (ch == '*') {
      if (wildcardSeen || source[len + 1U] != 0)
        return 0;
      wildcardSeen = 1;
    } else if (!brm_filename_char_allowed(ch)) {
      return 0;
    }

    out[len] = ch;
    len++;
  }

  return (uint8_t)(len > 0);
}

uint16_t BRM_NormalBlocksForBytes(uint16_t bytes) {
  if (bytes == 0)
    return 0;
  return (uint16_t)((bytes + (BRM_NORMAL_FILE_BLOCK_BYTES - 1U)) /
                    BRM_NORMAL_FILE_BLOCK_BYTES);
}

uint32_t BRM_ModeBytes(uint16_t blocks, uint8_t mode) {
  if (mode == BRM_FILE_MODE_NORMAL)
    return (uint32_t)blocks * BRM_NORMAL_FILE_BLOCK_BYTES;
  if (mode == BRM_FILE_MODE_PROTECTED)
    return (uint32_t)blocks * BRM_PROTECTED_FILE_BLOCK_BYTES;
  return 0;
}

BramResult BRM_Probe(const BramBiosOps *ops, BramProbeResult *out) {
  uint16_t totalBlocks = 0;
  uint16_t freeBlocks = 0;
  uint16_t fileCount = 0;
  BramFormatStatus status = BRM_FORMAT_NO_RAM;

  brm_clear_probe(out);
  if (!ops || !out || !ops->init)
    return BRM_RESULT_BAD_ARGUMENT;

  if (!ops->init(&totalBlocks, &status, ops->user))
    return BRM_RESULT_IO_ERROR;

  out->status = status;
  out->totalBlocks4K = totalBlocks;
  out->totalBytes = (uint32_t)totalBlocks * BRM_INIT_BLOCK_BYTES;

  if (status == BRM_FORMAT_NO_RAM || totalBlocks == 0)
    return BRM_RESULT_NO_RAM;

  out->present = 1;
  if (status == BRM_FORMAT_UNFORMATTED)
    return BRM_RESULT_UNFORMATTED;
  if (status == BRM_FORMAT_OTHER)
    return BRM_RESULT_OTHER_FORMAT;
  if (status != BRM_FORMAT_SEGA)
    return BRM_RESULT_IO_ERROR;

  out->formatted = 1;
  if (!ops->stat)
    return BRM_RESULT_BAD_ARGUMENT;
  if (!ops->stat(&freeBlocks, &fileCount, ops->user))
    return BRM_RESULT_IO_ERROR;

  if (freeBlocks > totalBlocks)
    freeBlocks = totalBlocks;
  out->freeBlocks4K = freeBlocks;
  out->freeBytes = (uint32_t)freeBlocks * BRM_INIT_BLOCK_BYTES;
  out->fileCount = fileCount;
  return BRM_RESULT_OK;
}

void BRM_InitInternalVolumeFromProbe(const BramProbeResult *probe,
                                     StorageVolumeInfo *volume) {
  if (!probe || !probe->present || !probe->formatted) {
    STG_InitVolume(volume, STG_VOLUME_INTERNAL_BRAM, 0, 0, 0, 0);
    return;
  }

  STG_InitVolume(volume, STG_VOLUME_INTERNAL_BRAM, 1, 1, probe->totalBytes,
                 probe->freeBytes);
}

BramResult BRM_WriteFile(const BramBiosOps *ops, const BramFilename *filename,
                         const uint8_t *data, uint16_t dataBytes) {
  BramFileInfo info;

  if (!ops || !ops->write || !filename || !data || dataBytes == 0)
    return BRM_RESULT_BAD_ARGUMENT;

  info.filename = *filename;
  info.mode = BRM_FILE_MODE_NORMAL;
  info.blocks = BRM_NormalBlocksForBytes(dataBytes);
  if (info.blocks == 0)
    return BRM_RESULT_BAD_ARGUMENT;

  if (!ops->write(&info, data, ops->user))
    return BRM_RESULT_IO_ERROR;
  return BRM_RESULT_OK;
}

BramResult BRM_ReadFile(const BramBiosOps *ops, const BramFilename *filename,
                        uint8_t *buffer, uint16_t bufferBytes,
                        uint16_t *bytesRead) {
  uint16_t blocks = 0;
  uint8_t mode = BRM_FILE_MODE_NORMAL;
  uint32_t requiredBytes;

  if (bytesRead)
    *bytesRead = 0;
  if (!ops || !ops->search || !ops->read || !filename || !buffer ||
      !bytesRead)
    return BRM_RESULT_BAD_ARGUMENT;

  if (!ops->search(filename, &blocks, &mode, ops->user))
    return BRM_RESULT_NOT_FOUND;

  requiredBytes = BRM_ModeBytes(blocks, mode);
  if (requiredBytes == 0 || requiredBytes > 0xffffUL)
    return BRM_RESULT_IO_ERROR;
  *bytesRead = (uint16_t)requiredBytes;
  if (bufferBytes < *bytesRead)
    return BRM_RESULT_TOO_SMALL;

  blocks = 0;
  mode = BRM_FILE_MODE_NORMAL;
  if (!ops->read(filename, buffer, &blocks, &mode, ops->user))
    return BRM_RESULT_IO_ERROR;

  requiredBytes = BRM_ModeBytes(blocks, mode);
  if (requiredBytes == 0 || requiredBytes > bufferBytes ||
      requiredBytes > 0xffffUL)
    return BRM_RESULT_IO_ERROR;
  *bytesRead = (uint16_t)requiredBytes;
  return BRM_RESULT_OK;
}

BramResult BRM_ReadDirectory(const BramBiosOps *ops, const char *pattern,
                             uint8_t *dirBuffer, uint16_t dirBytes,
                             uint16_t skip) {
  char normalized[BRM_FILENAME_BUFFER_BYTES];

  if (!ops || !ops->dir || !dirBuffer || dirBytes == 0)
    return BRM_RESULT_BAD_ARGUMENT;
  if (!BRM_MakePattern(pattern, normalized))
    return BRM_RESULT_BAD_ARGUMENT;

  if (!ops->dir(normalized, dirBuffer, skip, dirBytes, ops->user))
    return BRM_RESULT_IO_ERROR;
  return BRM_RESULT_OK;
}
