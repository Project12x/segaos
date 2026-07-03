#include "frame_scheduler.h"
#include "framebuffer.h"
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

static void expect_upload(DirtyTileQueue *queue, uint8_t index,
                          uint16_t firstTile, uint16_t tileCount,
                          uint16_t byteCount, const char *name) {
  DirtyTileUpload *upload = DR_GetTileUpload(queue, index);
  if (!upload) {
    printf("FAIL: %s missing upload %u\n", name, index);
    failures++;
    return;
  }

  expect_u16(upload->firstTile, firstTile, name);
  expect_u16(upload->tileCount, tileCount, name);
  expect_u16(upload->byteCount, byteCount, name);
}

static void small_span_finishes_in_one_budgeted_frame(void) {
  FrameTileCursor cursor;
  FrameScheduleResult result;
  DirtyTileUpload upload;
  DirtyTileQueue queue;

  FS_StartTileCursor(&cursor, 82, 4);
  DR_InitTileQueue(&queue, &upload, 1, 7524);

  expect_true(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                     &result),
              "small frame schedule succeeds");
  expect_u8(queue.count, 1, "small queue count");
  expect_upload(&queue, 0, 82, 4, 128, "small queued span");
  expect_u16(result.queuedTiles, 4, "small queued tiles");
  expect_u16(result.queuedBytes, 128, "small queued bytes");
  expect_u16(result.remainingTiles, 0, "small remaining tiles");
  expect_true(result.complete, "small schedule complete");
  expect_false(cursor.active, "small cursor inactive");
  expect_u16(cursor.nextTile, 86, "small cursor next tile");
}

static void full_frame_slices_across_ntsc_vblank_budget(void) {
  FrameTileCursor cursor;
  FrameScheduleResult result;
  DirtyTileUpload uploads[2];
  DirtyTileQueue queue;

  FS_StartTileCursor(&cursor, 0, FB_TILE_COUNT);
  DR_InitTileQueue(&queue, uploads, 2, 7524);

  expect_true(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                     &result),
              "full frame slice 0 schedules");
  expect_u8(queue.count, 1, "slice 0 queue count");
  expect_upload(&queue, 0, 0, 235, 7520, "slice 0 upload");
  expect_true(result.budgetLimited, "slice 0 budget limited");
  expect_false(result.complete, "slice 0 incomplete");
  expect_true(cursor.active, "slice 0 cursor active");
  expect_u16(cursor.nextTile, 235, "slice 0 cursor next");
  expect_u16(result.remainingTiles, 885, "slice 0 remaining");

  expect_true(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                     &result),
              "full frame slice 1 schedules");
  expect_upload(&queue, 0, 235, 235, 7520, "slice 1 upload");
  expect_u16(cursor.nextTile, 470, "slice 1 cursor next");

  expect_true(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                     &result),
              "full frame slice 2 schedules");
  expect_upload(&queue, 0, 470, 235, 7520, "slice 2 upload");
  expect_u16(cursor.nextTile, 705, "slice 2 cursor next");

  expect_true(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                     &result),
              "full frame slice 3 schedules");
  expect_upload(&queue, 0, 705, 235, 7520, "slice 3 upload");
  expect_u16(cursor.nextTile, 940, "slice 3 cursor next");

  expect_true(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                     &result),
              "full frame final slice schedules");
  expect_upload(&queue, 0, 940, 180, 5760, "final slice upload");
  expect_false(result.budgetLimited, "final slice not budget limited");
  expect_true(result.complete, "full frame complete");
  expect_false(cursor.active, "full frame cursor inactive");
  expect_u16(cursor.nextTile, FB_TILE_COUNT, "full frame cursor complete");
  expect_u16(result.remainingTiles, 0, "full frame no remaining");
}

static void inactive_cursor_clears_queue_and_reports_complete(void) {
  FrameTileCursor cursor;
  FrameScheduleResult result;
  DirtyTileUpload upload;
  DirtyTileQueue queue;

  FS_ClearTileCursor(&cursor);
  DR_InitTileQueue(&queue, &upload, 1, 7524);
  upload.firstTile = 99;
  upload.tileCount = 1;
  upload.byteCount = 32;
  queue.count = 1;
  queue.byteCount = 32;

  expect_true(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                     &result),
              "inactive schedule succeeds");
  expect_u8(queue.count, 0, "inactive queue cleared");
  expect_u16(result.queuedTiles, 0, "inactive queued tiles");
  expect_true(result.complete, "inactive complete");
}

static void queue_overflow_does_not_advance_cursor(void) {
  FrameTileCursor cursor;
  FrameScheduleResult result;
  DirtyTileQueue queue;

  FS_StartTileCursor(&cursor, 10, 4);
  DR_InitTileQueue(&queue, (DirtyTileUpload *)0, 0, 7524);

  expect_false(FS_PlanTileCursorFrame(&cursor, &queue, FB_BYTES_PER_TILE,
                                      &result),
               "overflow schedule fails");
  expect_true(result.overflow, "overflow flag set");
  expect_u16(cursor.nextTile, 10, "overflow cursor not advanced");
  expect_true(cursor.active, "overflow cursor still active");
}

int main(void) {
  small_span_finishes_in_one_budgeted_frame();
  full_frame_slices_across_ntsc_vblank_budget();
  inactive_cursor_clears_queue_and_reports_complete();
  queue_overflow_does_not_advance_cursor();

  if (failures) {
    printf("frame scheduler tests failed: %d\n", failures);
    return 1;
  }

  printf("frame scheduler tests passed\n");
  return 0;
}
