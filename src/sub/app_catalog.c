#include "app_catalog.h"

static uint16_t app_read_be16(const uint8_t *data, uint16_t offset) {
  return (uint16_t)(((uint16_t)data[offset] << 8) |
                    (uint16_t)data[offset + 1U]);
}

static uint32_t app_read_be32(const uint8_t *data, uint16_t offset) {
  return ((uint32_t)data[offset] << 24) |
         ((uint32_t)data[offset + 1U] << 16) |
         ((uint32_t)data[offset + 2U] << 8) |
         (uint32_t)data[offset + 3U];
}

static void app_clear_view(AppCatalogView *view) {
  if (view) {
    view->data = (const uint8_t *)0;
    view->dataBytes = 0;
    view->entryCount = 0;
  }
}

static void app_clear_entry(AppCatalogEntry *entry) {
  uint16_t i;

  if (!entry) {
    return;
  }

  entry->id = 0;
  for (i = 0; i <= APP_CATALOG_NAME_BYTES; i++) {
    entry->name[i] = 0;
  }
  for (i = 0; i <= APP_CATALOG_TITLE_BYTES; i++) {
    entry->title[i] = 0;
  }
  entry->kind = 0;
  entry->flags = 0;
  entry->moduleLba = 0;
  entry->moduleBytes = 0;
  entry->resourceLba = 0;
  entry->resourceBytes = 0;
  entry->minWidth = 0;
  entry->minHeight = 0;
}

static uint8_t app_is_iso_char(char ch) {
  if (ch >= 'A' && ch <= 'Z') {
    return 1;
  }
  if (ch >= '0' && ch <= '9') {
    return 1;
  }
  return (uint8_t)(ch == '_');
}

uint8_t APP_IsValidIsoName(const char *name) {
  uint8_t baseLen = 0;
  uint8_t extLen = 0;
  uint8_t sawDot = 0;
  uint8_t i = 0;

  if (!name || !name[0]) {
    return 0;
  }

  while (name[i]) {
    char ch = name[i];
    if (i >= APP_CATALOG_NAME_BYTES) {
      return 0;
    }

    if (ch == '.') {
      if (sawDot || baseLen == 0) {
        return 0;
      }
      sawDot = 1;
    } else if (!app_is_iso_char(ch)) {
      return 0;
    } else if (sawDot) {
      extLen++;
      if (extLen > 3) {
        return 0;
      }
    } else {
      baseLen++;
      if (baseLen > 8) {
        return 0;
      }
    }
    i++;
  }

  if (sawDot && extLen == 0) {
    return 0;
  }

  return (uint8_t)(baseLen > 0);
}

static uint8_t app_fixed_text_valid(const uint8_t *data, uint16_t maxBytes,
                                    uint8_t requireText) {
  uint16_t i;
  uint8_t sawText = 0;

  for (i = 0; i < maxBytes; i++) {
    uint8_t ch = data[i];
    if (ch == 0) {
      return (uint8_t)(!requireText || sawText);
    }
    if (ch < 32U || ch > 126U) {
      return 0;
    }
    sawText = 1;
  }

  return (uint8_t)(!requireText || sawText);
}

static void app_copy_fixed_text(char *dest, const uint8_t *src,
                                uint16_t maxBytes) {
  uint16_t i;

  for (i = 0; i < maxBytes; i++) {
    dest[i] = (char)src[i];
    if (!src[i]) {
      return;
    }
  }
  dest[maxBytes] = 0;
}

static uint8_t app_cstr_equal(const char *a, const char *b) {
  uint16_t i = 0;

  if (!a || !b) {
    return 0;
  }

  while (a[i] || b[i]) {
    if (a[i] != b[i]) {
      return 0;
    }
    i++;
  }

  return 1;
}

static AppCatalogStatus app_decode_entry(const uint8_t *base,
                                         AppCatalogEntry *out) {
  AppCatalogEntry entry;

  app_clear_entry(&entry);
  entry.id = app_read_be16(base, 0);
  app_copy_fixed_text(entry.name, base + 2U, APP_CATALOG_NAME_BYTES);
  app_copy_fixed_text(entry.title, base + 14U, APP_CATALOG_TITLE_BYTES);
  entry.kind = app_read_be16(base, 38);
  entry.flags = app_read_be16(base, 40);
  entry.moduleLba = app_read_be32(base, 42);
  entry.moduleBytes = app_read_be32(base, 46);
  entry.resourceLba = app_read_be32(base, 50);
  entry.resourceBytes = app_read_be32(base, 54);
  entry.minWidth = app_read_be16(base, 58);
  entry.minHeight = app_read_be16(base, 60);

  if (entry.id == 0 || !APP_IsValidIsoName(entry.name) ||
      !app_fixed_text_valid(base + 14U, APP_CATALOG_TITLE_BYTES, 1)) {
    return APP_CAT_BAD_ENTRY;
  }

  if (entry.kind != APP_KIND_BUILTIN && entry.kind != APP_KIND_MODULE) {
    return APP_CAT_BAD_ENTRY;
  }

  if (entry.kind == APP_KIND_MODULE && entry.moduleBytes == 0) {
    return APP_CAT_BAD_ENTRY;
  }

  if (out) {
    *out = entry;
  }

  return APP_CAT_OK;
}

AppCatalogStatus APP_ParseCatalog(const uint8_t *data, uint16_t dataBytes,
                                  AppCatalogView *out) {
  uint16_t version;
  uint16_t count;
  uint16_t entryBytes;
  uint32_t requiredBytes;
  uint16_t i;

  app_clear_view(out);

  if (!data || !out) {
    return APP_CAT_BAD_ARGUMENT;
  }

  if (dataBytes < APP_CATALOG_HEADER_BYTES) {
    return APP_CAT_TRUNCATED;
  }

  if (data[0] != 'S' || data[1] != 'A' || data[2] != 'C' || data[3] != '1') {
    return APP_CAT_BAD_MAGIC;
  }

  version = app_read_be16(data, 4);
  if (version != APP_CATALOG_VERSION) {
    return APP_CAT_BAD_VERSION;
  }

  count = app_read_be16(data, 6);
  entryBytes = app_read_be16(data, 8);
  if (entryBytes != APP_CATALOG_ENTRY_BYTES) {
    return APP_CAT_BAD_ENTRY_SIZE;
  }

  if (count > APP_CATALOG_MAX_ENTRIES) {
    return APP_CAT_TOO_MANY_ENTRIES;
  }

  requiredBytes = APP_CATALOG_HEADER_BYTES +
                  ((uint32_t)count * (uint32_t)APP_CATALOG_ENTRY_BYTES);
  if ((uint32_t)dataBytes < requiredBytes) {
    return APP_CAT_TRUNCATED;
  }

  for (i = 0; i < count; i++) {
    const uint8_t *entryBase =
        data + APP_CATALOG_HEADER_BYTES + (i * APP_CATALOG_ENTRY_BYTES);
    AppCatalogStatus status = app_decode_entry(entryBase,
                                               (AppCatalogEntry *)0);
    if (status != APP_CAT_OK) {
      return status;
    }
  }

  out->data = data;
  out->dataBytes = dataBytes;
  out->entryCount = count;
  return APP_CAT_OK;
}

uint16_t APP_CatalogEntryCount(const AppCatalogView *view) {
  if (!view) {
    return 0;
  }
  return view->entryCount;
}

AppCatalogStatus APP_GetCatalogEntry(const AppCatalogView *view,
                                     uint16_t index,
                                     AppCatalogEntry *out) {
  const uint8_t *entryBase;

  if (!view || !view->data || !out) {
    return APP_CAT_BAD_ARGUMENT;
  }

  app_clear_entry(out);

  if (index >= view->entryCount) {
    return APP_CAT_BAD_ARGUMENT;
  }

  entryBase = view->data + APP_CATALOG_HEADER_BYTES +
              (index * APP_CATALOG_ENTRY_BYTES);
  return app_decode_entry(entryBase, out);
}

uint8_t APP_FindCatalogEntryByName(const AppCatalogView *view,
                                   const char *name,
                                   AppCatalogEntry *out) {
  uint16_t i;

  if (!view || !name || !out || !APP_IsValidIsoName(name)) {
    return 0;
  }

  for (i = 0; i < view->entryCount; i++) {
    AppCatalogEntry entry;
    if (APP_GetCatalogEntry(view, i, &entry) != APP_CAT_OK) {
      return 0;
    }
    if (app_cstr_equal(entry.name, name)) {
      *out = entry;
      return 1;
    }
  }

  app_clear_entry(out);
  return 0;
}
