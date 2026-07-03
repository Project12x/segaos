#include "frame_upload_pump.h"
#include "framebuffer.h"
#include <stdio.h>

typedef struct {
  uint16_t first[8];
  uint16_t tiles[8];
  uint16_t bytes[8];
  uint8_t calls;
  uint8_t failOnCall;
} UploadSpy;

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

static uint8_t upload_spy_callback(const DirtyTileQueue *queue, void *user) {
  UploadSpy *spy = (UploadSpy *)user;
  const DirtyTileUpload *upload;

  if (!spy || !queue || queue->count != 1 || !queue->items)
    return 0;

  spy->calls++;
  if (spy->failOnCall != 0 && spy->calls == spy->failOnCall)
    return 0;

  upload = &queue->items[0];
  if (spy->calls <= 8) {
    uint8_t index = (uint8_t)(spy->calls - 1U);
    spy->first[index] = upload->firstTile;
    spy->tiles[index] = upload->tileCount;
    spy->bytes[index] = upload->byteCount;
  }

  return 1;
}

static void full_frame_becomes_return_ready_after_budgeted_uploads(void) {
  FrameUploadPump pump;
  UploadSpy spy = {0};
  FrameScheduleResult result;

  FUP_Init(&pump, 7524, FB_BYTES_PER_TILE);

  expect_true(FUP_StartFrame(&pump, 0, FB_TILE_COUNT), "start full frame");
  expect_true(FUP_IsUploading(&pump), "pump uploading after start");
  expect_false(FUP_ShouldReturnWordRam(&pump), "no return before upload");

  expect_true(FUP_Tick(&pump, upload_spy_callback, &spy, &result),
              "tick slice 0");
  expect_false(FUP_ShouldReturnWordRam(&pump), "slice 0 not complete");
  expect_u16(result.nextTile, 235, "slice 0 next");

  expect_true(FUP_Tick(&pump, upload_spy_callback, &spy, &result),
              "tick slice 1");
  expect_true(FUP_Tick(&pump, upload_spy_callback, &spy, &result),
              "tick slice 2");
  expect_true(FUP_Tick(&pump, upload_spy_callback, &spy, &result),
              "tick slice 3");
  expect_false(FUP_ShouldReturnWordRam(&pump), "slice 3 not complete");

  expect_true(FUP_Tick(&pump, upload_spy_callback, &spy, &result),
              "tick final slice");
  expect_true(FUP_ShouldReturnWordRam(&pump), "final slice ready to return");
  expect_false(FUP_IsUploading(&pump), "pump not uploading after completion");
  expect_u8(spy.calls, 5, "upload call count");

  expect_u16(spy.first[0], 0, "slice 0 first");
  expect_u16(spy.tiles[0], 235, "slice 0 tiles");
  expect_u16(spy.bytes[0], 7520, "slice 0 bytes");
  expect_u16(spy.first[1], 235, "slice 1 first");
  expect_u16(spy.tiles[1], 235, "slice 1 tiles");
  expect_u16(spy.first[2], 470, "slice 2 first");
  expect_u16(spy.tiles[2], 235, "slice 2 tiles");
  expect_u16(spy.first[3], 705, "slice 3 first");
  expect_u16(spy.tiles[3], 235, "slice 3 tiles");
  expect_u16(spy.first[4], 940, "final first");
  expect_u16(spy.tiles[4], 180, "final tiles");
  expect_u16(spy.bytes[4], 5760, "final bytes");

  expect_true(FUP_MarkWordRamReturned(&pump), "mark returned");
  expect_false(FUP_ShouldReturnWordRam(&pump), "return flag clears");
  expect_false(FUP_IsUploading(&pump), "pump idle after return");
}

static void compact_queue_planner_matches_budgeted_slices(void) {
  FrameUploadPump pump;
  FrameScheduleResult result;

  expect_true(FUP_BeginFrame(&pump, 0, FB_TILE_COUNT, 7524, FB_BYTES_PER_TILE),
              "begin compact full frame");
  expect_true(FUP_PlanNextQueue(&pump, &result), "compact slice 0 plan");
  expect_false(FUP_ShouldReturnWordRam(&pump), "compact slice 0 not complete");
  expect_u8(pump.queue.count, 1, "compact slice 0 queue count");
  expect_u16(pump.upload.firstTile, 0, "compact slice 0 first");
  expect_u16(result.queuedTiles, 235, "compact slice 0 tiles");
  expect_u16(result.nextTile, 235, "compact slice 0 next");

  expect_true(FUP_PlanNextQueue(&pump, &result), "compact slice 1 plan");
  expect_u8(pump.queue.count, 1, "compact slice 1 queue count");
  expect_u16(pump.upload.firstTile, 235, "compact slice 1 first");
  expect_u16(result.queuedTiles, 235, "compact slice 1 tiles");
  expect_u16(result.nextTile, 470, "compact slice 1 next");
}

static void refuses_new_frame_until_return_is_acknowledged(void) {
  FrameUploadPump pump;
  UploadSpy spy = {0};
  FrameScheduleResult result;

  FUP_Init(&pump, 7524, FB_BYTES_PER_TILE);
  expect_true(FUP_StartFrame(&pump, 0, 4), "start small frame");
  expect_false(FUP_StartFrame(&pump, 0, 4), "reject start while uploading");
  expect_true(FUP_Tick(&pump, upload_spy_callback, &spy, &result),
              "finish small frame");
  expect_true(FUP_ShouldReturnWordRam(&pump), "small frame ready");
  expect_false(FUP_StartFrame(&pump, 0, 4),
               "reject start while waiting return");
  expect_true(FUP_MarkWordRamReturned(&pump), "ack return");
  expect_true(FUP_StartFrame(&pump, 10, 2), "start after return");
}

static void failed_upload_enters_error_without_return_ready(void) {
  FrameUploadPump pump;
  UploadSpy spy = {0};
  FrameScheduleResult result;

  spy.failOnCall = 1;
  FUP_Init(&pump, 7524, FB_BYTES_PER_TILE);
  expect_true(FUP_StartFrame(&pump, 0, FB_TILE_COUNT), "start failing frame");

  expect_false(FUP_Tick(&pump, upload_spy_callback, &spy, &result),
               "failed upload tick");
  expect_true(FUP_HasError(&pump), "pump error set");
  expect_false(FUP_ShouldReturnWordRam(&pump), "failed upload not returnable");
  expect_u16(FUP_NextTile(&pump), 0, "failed upload rewinds cursor");
}

int main(void) {
  full_frame_becomes_return_ready_after_budgeted_uploads();
  compact_queue_planner_matches_budgeted_slices();
  refuses_new_frame_until_return_is_acknowledged();
  failed_upload_enters_error_without_return_ready();

  if (failures) {
    printf("frame upload pump tests failed: %d\n", failures);
    return 1;
  }

  printf("frame upload pump tests passed\n");
  return 0;
}
