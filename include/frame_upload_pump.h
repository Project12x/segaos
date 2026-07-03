/*
 * frame_upload_pump.h - Main-side frame upload loop policy.
 *
 * The pump owns a scheduler cursor for one rendered Word RAM frame. Each tick
 * plans one budgeted DirtyTileQueue and sends it through a caller-provided
 * upload function. When the cursor completes, Main may return Word RAM to Sub.
 */

#ifndef FRAME_UPLOAD_PUMP_H
#define FRAME_UPLOAD_PUMP_H

#include "frame_scheduler.h"
#include <stdint.h>

#define FUP_STATE_IDLE 0U
#define FUP_STATE_UPLOADING 1U
#define FUP_STATE_READY_TO_RETURN 2U
#define FUP_STATE_ERROR 3U

#define FUP_ERROR_NONE 0U
#define FUP_ERROR_PLAN_FAILED 1U
#define FUP_ERROR_UPLOAD_FAILED 2U

typedef uint8_t (*FrameUploadPumpCallback)(const DirtyTileQueue *queue,
                                           void *user);

typedef struct {
  FrameTileCursor cursor;
  DirtyTileUpload upload;
  DirtyTileQueue queue;
  uint16_t budgetBytes;
  uint16_t bytesPerTile;
  uint8_t state;
  uint8_t lastError;
} FrameUploadPump;

/* Compact planner path for IP-constrained target code. The caller performs
 * the upload after each planned queue. */
static inline void fup_bind_single_queue(FrameUploadPump *pump) {
  pump->queue.items = &pump->upload;
  pump->queue.capacity = 1;
  pump->queue.count = 0;
  pump->queue.maxBytes = pump->budgetBytes;
  pump->queue.byteCount = 0;
  pump->queue.budgetExceeded = 0;
  pump->queue.overflow = 0;
}

static inline uint8_t FUP_BeginFrame(FrameUploadPump *pump, uint16_t firstTile,
                                     uint16_t tileCount,
                                     uint16_t budgetBytes,
                                     uint16_t bytesPerTile) {
  if (!pump || tileCount == 0 || bytesPerTile == 0)
    return 0;

  pump->cursor.firstTile = firstTile;
  pump->cursor.tileCount = tileCount;
  pump->cursor.nextTile = firstTile;
  pump->cursor.active = 1;
  pump->cursor._pad = 0;
  pump->upload.firstTile = 0;
  pump->upload.tileCount = 0;
  pump->upload.byteCount = 0;
  pump->budgetBytes = budgetBytes;
  pump->bytesPerTile = bytesPerTile;
  pump->state = FUP_STATE_UPLOADING;
  pump->lastError = FUP_ERROR_NONE;
  fup_bind_single_queue(pump);
  return 1;
}

static inline uint8_t FUP_PlanNextQueue(FrameUploadPump *pump,
                                        FrameScheduleResult *result) {
  if (!pump || !result || pump->state != FUP_STATE_UPLOADING)
    return 0;

  if (!FS_PlanTileCursorFrame(&pump->cursor, &pump->queue, pump->bytesPerTile,
                              result)) {
    pump->state = FUP_STATE_ERROR;
    pump->lastError = FUP_ERROR_PLAN_FAILED;
    return 0;
  }

  if (result->complete)
    pump->state = FUP_STATE_READY_TO_RETURN;

  return 1;
}

static inline uint8_t FUP_PlanNextQueueCompact(FrameUploadPump *pump) {
  if (!pump || pump->state != FUP_STATE_UPLOADING)
    return 0;

  if (!FS_PlanTileCursorFrame(&pump->cursor, &pump->queue, pump->bytesPerTile,
                              0)) {
    pump->state = FUP_STATE_ERROR;
    pump->lastError = FUP_ERROR_PLAN_FAILED;
    return 0;
  }

  if (!pump->cursor.active)
    pump->state = FUP_STATE_READY_TO_RETURN;

  return 1;
}

void FUP_Init(FrameUploadPump *pump, uint16_t budgetBytes,
              uint16_t bytesPerTile);
uint8_t FUP_StartFrame(FrameUploadPump *pump, uint16_t firstTile,
                       uint16_t tileCount);
uint8_t FUP_Tick(FrameUploadPump *pump, FrameUploadPumpCallback upload,
                 void *user, FrameScheduleResult *result);
uint8_t FUP_ShouldReturnWordRam(const FrameUploadPump *pump);
uint8_t FUP_MarkWordRamReturned(FrameUploadPump *pump);
uint8_t FUP_IsUploading(const FrameUploadPump *pump);
uint8_t FUP_HasError(const FrameUploadPump *pump);
uint16_t FUP_NextTile(const FrameUploadPump *pump);

#endif /* FRAME_UPLOAD_PUMP_H */
