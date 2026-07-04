#include "app_catalog.h"
#include <stdio.h>

static int failures;

static void expect_true(uint8_t value, const char *name) {
  if (!value) {
    printf("FAIL: %s expected true\n", name);
    failures++;
  }
}

static void expect_false(uint8_t value, const char *name) {
  if (value) {
    printf("FAIL: %s expected false\n", name);
    failures++;
  }
}

static void expect_u16(uint16_t actual, uint16_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void expect_u32(uint32_t actual, uint32_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %lu got %lu\n", name, (unsigned long)expected,
           (unsigned long)actual);
    failures++;
  }
}

static void expect_status(AppCatalogStatus actual, AppCatalogStatus expected,
                          const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected status %u got %u\n", name, (unsigned)expected,
           (unsigned)actual);
    failures++;
  }
}

static void expect_text(const char *actual, const char *expected,
                        const char *name) {
  unsigned int i = 0;
  while (actual[i] || expected[i]) {
    if (actual[i] != expected[i]) {
      printf("FAIL: %s expected '%s' got '%s'\n", name, expected, actual);
      failures++;
      return;
    }
    i++;
  }
}

static void put_be16(uint8_t *data, uint16_t offset, uint16_t value) {
  data[offset] = (uint8_t)(value >> 8);
  data[offset + 1U] = (uint8_t)value;
}

static void put_be32(uint8_t *data, uint16_t offset, uint32_t value) {
  data[offset] = (uint8_t)(value >> 24);
  data[offset + 1U] = (uint8_t)(value >> 16);
  data[offset + 2U] = (uint8_t)(value >> 8);
  data[offset + 3U] = (uint8_t)value;
}

static void put_text(uint8_t *data, uint16_t offset, const char *text,
                     uint16_t maxBytes) {
  uint16_t i;
  for (i = 0; i < maxBytes; i++) {
    data[offset + i] = text[i] ? (uint8_t)text[i] : 0;
    if (!text[i]) {
      i++;
      break;
    }
  }
  for (; i < maxBytes; i++) {
    data[offset + i] = 0;
  }
}

static void init_header(uint8_t *data, uint16_t entryCount) {
  data[0] = 'S';
  data[1] = 'A';
  data[2] = 'C';
  data[3] = '1';
  put_be16(data, 4, APP_CATALOG_VERSION);
  put_be16(data, 6, entryCount);
  put_be16(data, 8, APP_CATALOG_ENTRY_BYTES);
  put_be16(data, 10, 0);
  put_be32(data, 12, 0);
}

static void init_entry(uint8_t *data, uint16_t index, uint16_t id,
                       const char *name, const char *title, uint16_t kind,
                       uint16_t flags, uint32_t moduleLba,
                       uint32_t moduleBytes, uint32_t resourceLba,
                       uint32_t resourceBytes, uint16_t minWidth,
                       uint16_t minHeight) {
  uint16_t base = (uint16_t)(APP_CATALOG_HEADER_BYTES +
                             (index * APP_CATALOG_ENTRY_BYTES));
  put_be16(data, base + 0U, id);
  put_text(data, base + 2U, name, APP_CATALOG_NAME_BYTES);
  put_text(data, base + 14U, title, APP_CATALOG_TITLE_BYTES);
  put_be16(data, base + 38U, kind);
  put_be16(data, base + 40U, flags);
  put_be32(data, base + 42U, moduleLba);
  put_be32(data, base + 46U, moduleBytes);
  put_be32(data, base + 50U, resourceLba);
  put_be32(data, base + 54U, resourceBytes);
  put_be16(data, base + 58U, minWidth);
  put_be16(data, base + 60U, minHeight);
  put_be16(data, base + 62U, 0);
}

static void init_valid_catalog(uint8_t *data) {
  init_header(data, 2);
  init_entry(data, 0, 1, "TEXT.APP", "Text Editor", APP_KIND_MODULE,
             APP_CAP_WINDOW | APP_CAP_DOCUMENT | APP_CAP_STORAGE |
                 APP_CAP_TEXT,
             123, 4096, 200, 512, 160, 96);
  init_entry(data, 1, 2, "BASIC.APP", "BASIC", APP_KIND_BUILTIN,
             APP_CAP_WINDOW | APP_CAP_DOCUMENT | APP_CAP_STORAGE |
                 APP_CAP_BASIC,
             0, 0, 0, 0, 160, 96);
}

static void parses_cd_app_catalog_entries(void) {
  uint8_t data[APP_CATALOG_HEADER_BYTES + (2U * APP_CATALOG_ENTRY_BYTES)] = {0};
  AppCatalogView view;
  AppCatalogEntry entry;

  init_valid_catalog(data);

  expect_status(APP_ParseCatalog(data, sizeof(data), &view), APP_CAT_OK,
                "parse valid catalog");
  expect_u16(APP_CatalogEntryCount(&view), 2, "entry count");
  expect_status(APP_GetCatalogEntry(&view, 0, &entry), APP_CAT_OK,
                "get first entry");
  expect_u16(entry.id, 1, "text app id");
  expect_text(entry.name, "TEXT.APP", "text app name");
  expect_text(entry.title, "Text Editor", "text app title");
  expect_u16(entry.kind, APP_KIND_MODULE, "text app kind");
  expect_u16(entry.flags, APP_CAP_WINDOW | APP_CAP_DOCUMENT |
                              APP_CAP_STORAGE | APP_CAP_TEXT,
             "text app caps");
  expect_u32(entry.moduleLba, 123, "text module lba");
  expect_u32(entry.moduleBytes, 4096, "text module bytes");
  expect_u32(entry.resourceLba, 200, "text resource lba");
  expect_u32(entry.resourceBytes, 512, "text resource bytes");
  expect_u16(entry.minWidth, 160, "text min width");
  expect_u16(entry.minHeight, 96, "text min height");
}

static void finds_entry_by_iso_name(void) {
  uint8_t data[APP_CATALOG_HEADER_BYTES + (2U * APP_CATALOG_ENTRY_BYTES)] = {0};
  AppCatalogView view;
  AppCatalogEntry entry;

  init_valid_catalog(data);
  expect_status(APP_ParseCatalog(data, sizeof(data), &view), APP_CAT_OK,
                "parse valid catalog for lookup");
  expect_true(APP_FindCatalogEntryByName(&view, "BASIC.APP", &entry),
              "find basic app");
  expect_u16(entry.id, 2, "basic app id");
  expect_u16(entry.kind, APP_KIND_BUILTIN, "basic built-in rung kind");
  expect_false(APP_FindCatalogEntryByName(&view, "MISSING.APP", &entry),
               "missing app not found");
}

static void rejects_invalid_catalog_shapes(void) {
  uint8_t data[APP_CATALOG_HEADER_BYTES + APP_CATALOG_ENTRY_BYTES] = {0};
  AppCatalogView view;

  init_header(data, 1);
  init_entry(data, 0, 1, "TEXT.APP", "Text Editor", APP_KIND_MODULE,
             APP_CAP_WINDOW, 10, 256, 0, 0, 80, 64);

  data[0] = 'X';
  expect_status(APP_ParseCatalog(data, sizeof(data), &view), APP_CAT_BAD_MAGIC,
                "bad magic");
  data[0] = 'S';

  put_be16(data, 4, 2);
  expect_status(APP_ParseCatalog(data, sizeof(data), &view),
                APP_CAT_BAD_VERSION, "bad version");
  put_be16(data, 4, APP_CATALOG_VERSION);

  put_be16(data, 8, 60);
  expect_status(APP_ParseCatalog(data, sizeof(data), &view),
                APP_CAT_BAD_ENTRY_SIZE, "bad entry size");
  put_be16(data, 8, APP_CATALOG_ENTRY_BYTES);

  expect_status(APP_ParseCatalog(data, sizeof(data) - 1U, &view),
                APP_CAT_TRUNCATED, "truncated catalog");

  put_be16(data, 6, APP_CATALOG_MAX_ENTRIES + 1U);
  expect_status(APP_ParseCatalog(data, sizeof(data), &view),
                APP_CAT_TOO_MANY_ENTRIES, "too many entries");
}

static void rejects_invalid_app_entries(void) {
  uint8_t data[APP_CATALOG_HEADER_BYTES + APP_CATALOG_ENTRY_BYTES] = {0};
  AppCatalogView view;

  init_header(data, 1);
  init_entry(data, 0, 1, "bad.app", "Text Editor", APP_KIND_MODULE,
             APP_CAP_WINDOW, 10, 256, 0, 0, 80, 64);
  expect_status(APP_ParseCatalog(data, sizeof(data), &view), APP_CAT_BAD_ENTRY,
                "lowercase name");

  init_entry(data, 0, 1, "TEXT.APP", "Text Editor", APP_KIND_MODULE,
             APP_CAP_WINDOW, 10, 0, 0, 0, 80, 64);
  expect_status(APP_ParseCatalog(data, sizeof(data), &view), APP_CAT_BAD_ENTRY,
                "module with no bytes");

  init_entry(data, 0, 1, "TEXT.APP", "Text Editor", 99, APP_CAP_WINDOW, 10,
             256, 0, 0, 80, 64);
  expect_status(APP_ParseCatalog(data, sizeof(data), &view), APP_CAT_BAD_ENTRY,
                "unknown app kind");
}

int main(void) {
  parses_cd_app_catalog_entries();
  finds_entry_by_iso_name();
  rejects_invalid_catalog_shapes();
  rejects_invalid_app_entries();

  if (failures) {
    printf("app catalog tests failed: %d\n", failures);
    return 1;
  }

  printf("app catalog tests passed\n");
  return 0;
}
