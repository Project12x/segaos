#include "framebuffer.h"
#include "dirty_rect.h"
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

static void expect_u16(uint16_t actual, uint16_t expected, const char *name) {
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

typedef struct {
  uint8_t count;
  uint16_t firstTile[4];
  uint16_t tileCount[4];
  uint16_t vramAddr[4];
  uint16_t wordCount[4];
  uint8_t firstByte[4];
  uint8_t lastTileFirstByte[4];
} UploadLog;

static uint8_t record_upload(const uint8_t *tileData, uint16_t firstTile,
                             uint16_t tileCount, uint16_t vramAddr,
                             uint16_t wordCount, void *user) {
  UploadLog *log = (UploadLog *)user;
  uint8_t index;

  if (!log || log->count >= 4 || !tileData || tileCount == 0)
    return 0;

  index = log->count;
  log->firstTile[index] = firstTile;
  log->tileCount[index] = tileCount;
  log->vramAddr[index] = vramAddr;
  log->wordCount[index] = wordCount;
  log->firstByte[index] = tileData[0];
  log->lastTileFirstByte[index] =
      tileData[((uint32_t)(tileCount - 1) * FB_BYTES_PER_TILE)];
  log->count++;
  return 1;
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

static void framebuffer_flushes_dirty_queue_in_strip_sized_chunks(void) {
  uint8_t linear[FB_LINEAR_BPR * FB_SCREEN_H];
  uint8_t scratch[FB_STRIP_TILES * FB_BYTES_PER_TILE];
  DirtyTileUpload uploads[4];
  DirtyTileQueue queue;
  DirtyTileRange fullFrame = {0, 0, FB_TILES_X, FB_TILES_Y};
  UploadLog log;

  memset(linear, 0, sizeof(linear));
  memset(scratch, 0, sizeof(scratch));
  memset(&log, 0, sizeof(log));
  write_source_tile(linear, 0, 0x10);
  write_source_tile(linear, 159, 0x40);
  write_source_tile(linear, 160, 0x70);
  write_source_tile(linear, 234, 0xa0);

  DR_InitTileQueue(&queue, uploads, 4, 7524);
  expect_false(DR_QueueTileRange(&queue, &fullFrame, FB_TILES_X,
                                 FB_BYTES_PER_TILE),
               "full frame queue exceeds vblank budget");
  expect_u16(queue.count, 1, "full frame queue entry count");

  expect_true(FB_FlushTileQueueWithCallback(linear, &queue, scratch,
                                            sizeof(scratch), record_upload,
                                            &log),
              "dirty queue flush callback succeeds");
  expect_u8(log.count, 2, "dirty queue upload chunk count");
  expect_u16(log.firstTile[0], 0, "chunk 0 first tile");
  expect_u16(log.tileCount[0], 160, "chunk 0 tile count");
  expect_u16(log.vramAddr[0], 0, "chunk 0 vram addr");
  expect_u16(log.wordCount[0], 2560, "chunk 0 word count");
  expect_u8(log.firstByte[0], 0x10, "chunk 0 first byte");
  expect_u8(log.lastTileFirstByte[0], 0x40, "chunk 0 last tile byte");
  expect_u16(log.firstTile[1], 160, "chunk 1 first tile");
  expect_u16(log.tileCount[1], 75, "chunk 1 tile count");
  expect_u16(log.vramAddr[1], 5120, "chunk 1 vram addr");
  expect_u16(log.wordCount[1], 1200, "chunk 1 word count");
  expect_u8(log.firstByte[1], 0x70, "chunk 1 first byte");
  expect_u8(log.lastTileFirstByte[1], 0xa0, "chunk 1 last tile byte");
}

static void framebuffer_rejects_queue_flush_without_scratch_space(void) {
  uint8_t linear[FB_LINEAR_BPR * FB_SCREEN_H];
  uint8_t scratch[FB_BYTES_PER_TILE - 1];
  DirtyTileUpload upload;
  DirtyTileQueue queue;
  DirtyTileRange range = {0, 0, 1, 1};
  UploadLog log;

  memset(linear, 0, sizeof(linear));
  memset(scratch, 0, sizeof(scratch));
  memset(&log, 0, sizeof(log));
  DR_InitTileQueue(&queue, &upload, 1, 7524);
  expect_true(DR_QueueTileRange(&queue, &range, FB_TILES_X,
                                FB_BYTES_PER_TILE),
              "small queue builds");

  expect_false(FB_FlushTileQueueWithCallback(linear, &queue, scratch,
                                             sizeof(scratch), record_upload,
                                             &log),
               "undersized scratch rejected");
  expect_u8(log.count, 0, "no uploads with undersized scratch");
}

int main(void) {
  framebuffer_converts_single_tile_span();
  framebuffer_converts_span_across_row_boundary();
  framebuffer_rejects_invalid_spans();
  framebuffer_flushes_dirty_queue_in_strip_sized_chunks();
  framebuffer_rejects_queue_flush_without_scratch_space();

  if (failures) {
    printf("%d framebuffer test(s) failed\n", failures);
    return 1;
  }

  printf("framebuffer tests passed\n");
  return 0;
}
