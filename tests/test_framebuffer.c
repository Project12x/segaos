#include "framebuffer.h"
#include <stdio.h>
#include <string.h>

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

static void expect_u8(uint8_t actual, uint8_t expected, const char *name) {
  if (actual != expected) {
    printf("FAIL: %s expected %u got %u\n", name, expected, actual);
    failures++;
  }
}

static void write_source_tile(uint8_t *linear, uint16_t tileIndex,
                              uint8_t base) {
  uint16_t tx = tileIndex % FB_TILES_X;
  uint16_t ty = tileIndex / FB_TILES_X;
  uint16_t row;

  for (row = 0; row < 8; row++) {
    uint32_t offset =
        ((uint32_t)((ty * 8) + row) * FB_LINEAR_BPR) + (uint32_t)(tx * 4);
    linear[offset + 0] = (uint8_t)(base + row);
    linear[offset + 1] = (uint8_t)(base + 0x10 + row);
    linear[offset + 2] = (uint8_t)(base + 0x20 + row);
    linear[offset + 3] = (uint8_t)(base + 0x30 + row);
  }
}

static void expect_tile_bytes(const uint8_t *tiles, uint16_t outTileIndex,
                              uint8_t base, const char *name) {
  uint16_t row;
  const uint8_t *tile = tiles + ((uint32_t)outTileIndex * FB_BYTES_PER_TILE);

  for (row = 0; row < 8; row++) {
    expect_u8(tile[(row * 4) + 0], (uint8_t)(base + row), name);
    expect_u8(tile[(row * 4) + 1], (uint8_t)(base + 0x10 + row), name);
    expect_u8(tile[(row * 4) + 2], (uint8_t)(base + 0x20 + row), name);
    expect_u8(tile[(row * 4) + 3], (uint8_t)(base + 0x30 + row), name);
  }
}

static void framebuffer_converts_single_tile_span(void) {
  uint8_t linear[FB_LINEAR_BPR * FB_SCREEN_H];
  uint8_t out[FB_BYTES_PER_TILE];
  uint16_t tileIndex = (uint16_t)((3 * FB_TILES_X) + 2);

  memset(linear, 0, sizeof(linear));
  memset(out, 0, sizeof(out));
  write_source_tile(linear, tileIndex, 0x10);

  expect_true(FB_ConvertTileSpan(linear, tileIndex, 1, out, sizeof(out)),
              "single tile span converts");
  expect_tile_bytes(out, 0, 0x10, "single tile bytes");
}

static void framebuffer_converts_span_across_row_boundary(void) {
  uint8_t linear[FB_LINEAR_BPR * FB_SCREEN_H];
  uint8_t out[FB_BYTES_PER_TILE * 3];

  memset(linear, 0, sizeof(linear));
  memset(out, 0, sizeof(out));
  write_source_tile(linear, 39, 0x20);
  write_source_tile(linear, 40, 0x50);
  write_source_tile(linear, 41, 0x80);

  expect_true(FB_ConvertTileSpan(linear, 39, 3, out, sizeof(out)),
              "row boundary span converts");
  expect_tile_bytes(out, 0, 0x20, "row boundary tile 39");
  expect_tile_bytes(out, 1, 0x50, "row boundary tile 40");
  expect_tile_bytes(out, 2, 0x80, "row boundary tile 41");
}

static void framebuffer_rejects_invalid_spans(void) {
  uint8_t linear[FB_LINEAR_BPR * FB_SCREEN_H];
  uint8_t out[FB_BYTES_PER_TILE];

  memset(linear, 0, sizeof(linear));
  memset(out, 0, sizeof(out));

  expect_false(FB_ConvertTileSpan(linear, FB_TILE_COUNT, 1, out, sizeof(out)),
               "past-end first tile rejected");
  expect_false(FB_ConvertTileSpan(linear, 0, 2, out, sizeof(out)),
               "too-small output rejected");
  expect_false(FB_ConvertTileSpan((const uint8_t *)0, 0, 1, out, sizeof(out)),
               "null source rejected");
  expect_false(FB_ConvertTileSpan(linear, 0, 1, (uint8_t *)0, sizeof(out)),
               "null destination rejected");
}

int main(void) {
  framebuffer_converts_single_tile_span();
  framebuffer_converts_span_across_row_boundary();
  framebuffer_rejects_invalid_spans();

  if (failures) {
    printf("%d framebuffer test(s) failed\n", failures);
    return 1;
  }

  printf("framebuffer tests passed\n");
  return 0;
}
