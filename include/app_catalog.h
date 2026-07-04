#ifndef SEGAOS_APP_CATALOG_H
#define SEGAOS_APP_CATALOG_H

#include <stdint.h>

#define APP_CATALOG_VERSION 1U
#define APP_CATALOG_HEADER_BYTES 16U
#define APP_CATALOG_ENTRY_BYTES 64U
#define APP_CATALOG_MAX_ENTRIES 32U
#define APP_CATALOG_NAME_BYTES 12U
#define APP_CATALOG_TITLE_BYTES 24U

typedef enum AppCatalogStatus {
  APP_CAT_OK = 0,
  APP_CAT_BAD_ARGUMENT = 1,
  APP_CAT_BAD_MAGIC = 2,
  APP_CAT_BAD_VERSION = 3,
  APP_CAT_BAD_ENTRY_SIZE = 4,
  APP_CAT_TRUNCATED = 5,
  APP_CAT_TOO_MANY_ENTRIES = 6,
  APP_CAT_BAD_ENTRY = 7
} AppCatalogStatus;

typedef enum AppCatalogKind {
  APP_KIND_BUILTIN = 1,
  APP_KIND_MODULE = 2
} AppCatalogKind;

typedef enum AppCatalogCapability {
  APP_CAP_WINDOW = 0x0001,
  APP_CAP_DOCUMENT = 0x0002,
  APP_CAP_STORAGE = 0x0004,
  APP_CAP_TEXT = 0x0008,
  APP_CAP_BASIC = 0x0010,
  APP_CAP_IMAGE = 0x0020
} AppCatalogCapability;

typedef struct AppCatalogView {
  const uint8_t *data;
  uint16_t dataBytes;
  uint16_t entryCount;
} AppCatalogView;

typedef struct AppCatalogEntry {
  uint16_t id;
  char name[APP_CATALOG_NAME_BYTES + 1U];
  char title[APP_CATALOG_TITLE_BYTES + 1U];
  uint16_t kind;
  uint16_t flags;
  uint32_t moduleLba;
  uint32_t moduleBytes;
  uint32_t resourceLba;
  uint32_t resourceBytes;
  uint16_t minWidth;
  uint16_t minHeight;
} AppCatalogEntry;

AppCatalogStatus APP_ParseCatalog(const uint8_t *data, uint16_t dataBytes,
                                  AppCatalogView *out);
uint16_t APP_CatalogEntryCount(const AppCatalogView *view);
AppCatalogStatus APP_GetCatalogEntry(const AppCatalogView *view,
                                     uint16_t index,
                                     AppCatalogEntry *out);
uint8_t APP_FindCatalogEntryByName(const AppCatalogView *view,
                                   const char *name,
                                   AppCatalogEntry *out);
uint8_t APP_IsValidIsoName(const char *name);

#endif
