/*
 * bram.h - Host-testable Sega CD Backup RAM BIOS wrapper contract.
 *
 * This normalizes the Sub BIOS BRAM call semantics behind injectable ops. It
 * does not write raw odd-address BRAM and does not format storage.
 */

#ifndef BRAM_H
#define BRAM_H

#include "storage.h"
#include <stdint.h>

#define BRM_BIOS_VECTOR 0x005f16UL

#define BRM_WORK_BUFFER_BYTES 0x0640U
#define BRM_STRING_BUFFER_BYTES 12U
#define BRM_INIT_BLOCK_BYTES 0x1000UL
#define BRM_NORMAL_FILE_BLOCK_BYTES 0x0040U
#define BRM_PROTECTED_FILE_BLOCK_BYTES 0x0020U
#define BRM_FILENAME_BYTES 11U
#define BRM_FILENAME_BUFFER_BYTES 12U
#define BRM_DIR_ENTRY_BYTES 16U

#define BRM_FILE_MODE_NORMAL 0x00U
#define BRM_FILE_MODE_PROTECTED 0xffU

typedef enum {
  BRM_FORMAT_NO_RAM = 0,
  BRM_FORMAT_UNFORMATTED = 1,
  BRM_FORMAT_OTHER = 2,
  BRM_FORMAT_SEGA = 3
} BramFormatStatus;

typedef enum {
  BRM_RESULT_OK = 0,
  BRM_RESULT_BAD_ARGUMENT = 1,
  BRM_RESULT_NO_RAM = 2,
  BRM_RESULT_UNFORMATTED = 3,
  BRM_RESULT_OTHER_FORMAT = 4,
  BRM_RESULT_NOT_FOUND = 5,
  BRM_RESULT_TOO_SMALL = 6,
  BRM_RESULT_IO_ERROR = 7
} BramResult;

typedef struct {
  char bytes[BRM_FILENAME_BYTES];
} BramFilename;

typedef struct {
  BramFilename filename;
  uint8_t mode;
  uint16_t blocks;
} BramFileInfo;

typedef struct {
  uint8_t present;
  uint8_t formatted;
  BramFormatStatus status;
  uint16_t totalBlocks4K;
  uint16_t freeBlocks4K;
  uint16_t fileCount;
  uint32_t totalBytes;
  uint32_t freeBytes;
} BramProbeResult;

typedef uint8_t (*BramBiosInit)(uint16_t *totalBlocks4K,
                                BramFormatStatus *status, void *user);
typedef uint8_t (*BramBiosStat)(uint16_t *freeBlocks4K, uint16_t *fileCount,
                                void *user);
typedef uint8_t (*BramBiosSearch)(const BramFilename *filename,
                                  uint16_t *blocks, uint8_t *mode,
                                  void *user);
typedef uint8_t (*BramBiosRead)(const BramFilename *filename, uint8_t *buffer,
                                uint16_t *blocks, uint8_t *mode, void *user);
typedef uint8_t (*BramBiosWrite)(const BramFileInfo *info,
                                 const uint8_t *data, void *user);
typedef uint8_t (*BramBiosDir)(const char *pattern, uint8_t *dirBuffer,
                               uint16_t skip, uint16_t dirBytes, void *user);

typedef struct {
  BramBiosInit init;
  BramBiosStat stat;
  BramBiosSearch search;
  BramBiosRead read;
  BramBiosWrite write;
  BramBiosDir dir;
  void *user;
} BramBiosOps;

uint8_t BRM_MakeFilename(const char *source, BramFilename *out);
uint8_t BRM_MakePattern(const char *source, char out[BRM_FILENAME_BUFFER_BYTES]);
uint16_t BRM_NormalBlocksForBytes(uint16_t bytes);
uint32_t BRM_ModeBytes(uint16_t blocks, uint8_t mode);

BramResult BRM_Probe(const BramBiosOps *ops, BramProbeResult *out);
void BRM_InitInternalVolumeFromProbe(const BramProbeResult *probe,
                                     StorageVolumeInfo *volume);
BramResult BRM_WriteFile(const BramBiosOps *ops, const BramFilename *filename,
                         const uint8_t *data, uint16_t dataBytes);
BramResult BRM_ReadFile(const BramBiosOps *ops, const BramFilename *filename,
                        uint8_t *buffer, uint16_t bufferBytes,
                        uint16_t *bytesRead);
BramResult BRM_ReadDirectory(const BramBiosOps *ops, const char *pattern,
                             uint8_t *dirBuffer, uint16_t dirBytes,
                             uint16_t skip);

#endif /* BRAM_H */
