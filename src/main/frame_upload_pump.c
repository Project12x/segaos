#include "frame_upload_pump.h"

static void fup_bind_queue(FrameUploadPump *pump) {
  pump->queue.items = &pump->upload;
  pump->queue.capacity = 1;
  pump->queue.count = 0;
  pump->queue.maxBytes = pump->budgetBytes;
  pump->queue.byteCount = 0;
  pump->queue.budgetExceeded = 0;
  pump->queue.overflow = 0;
}

void FUP_Init(FrameUploadPump *pump, uint16_t budgetBytes,
              uint16_t bytesPerTile) {
  if (!pump)
    return;

  FS_ClearTileCursor(&pump->cursor);
  pump->upload.firstTile = 0;
  pump->upload.tileCount = 0;
  pump->upload.byteCount = 0;
  pump->budgetBytes = budgetBytes;
  pump->bytesPerTile = bytesPerTile;
  pump->state = FUP_STATE_IDLE;
  pump->lastError = FUP_ERROR_NONE;
  fup_bind_queue(pump);
}

uint8_t FUP_StartFrame(FrameUploadPump *pump, uint16_t firstTile,
                       uint16_t tileCount) {
  if (!pump || pump->state != FUP_STATE_IDLE || tileCount == 0 ||
      pump->bytesPerTile == 0) {
    return 0;
  }

  FS_StartTileCursor(&pump->cursor, firstTile, tileCount);
  fup_bind_queue(pump);
  pump->state = FUP_STATE_UPLOADING;
  pump->lastError = FUP_ERROR_NONE;
  return 1;
}

uint8_t FUP_Tick(FrameUploadPump *pump, FrameUploadPumpCallback upload,
                 void *user, FrameScheduleResult *result) {
  FrameTileCursor previousCursor;
  FrameScheduleResult localResult;
  FrameScheduleResult *outResult;

  if (!pump || !upload)
    return 0;
  if (pump->state == FUP_STATE_IDLE ||
      pump->state == FUP_STATE_READY_TO_RETURN)
    return 1;
  if (pump->state != FUP_STATE_UPLOADING)
    return 0;

  previousCursor = pump->cursor;
  outResult = result ? result : &localResult;
  if (!FS_PlanTileCursorFrame(&pump->cursor, &pump->queue, pump->bytesPerTile,
                              outResult)) {
    pump->cursor = previousCursor;
    pump->state = FUP_STATE_ERROR;
    pump->lastError = FUP_ERROR_PLAN_FAILED;
    return 0;
  }

  if (pump->queue.count != 0 && !upload(&pump->queue, user)) {
    pump->cursor = previousCursor;
    pump->state = FUP_STATE_ERROR;
    pump->lastError = FUP_ERROR_UPLOAD_FAILED;
    return 0;
  }

  if (outResult->complete) {
    pump->state = FUP_STATE_READY_TO_RETURN;
  }

  return 1;
}

uint8_t FUP_ShouldReturnWordRam(const FrameUploadPump *pump) {
  return (pump && pump->state == FUP_STATE_READY_TO_RETURN) ? 1 : 0;
}

uint8_t FUP_MarkWordRamReturned(FrameUploadPump *pump) {
  if (!pump || pump->state != FUP_STATE_READY_TO_RETURN)
    return 0;

  FS_ClearTileCursor(&pump->cursor);
  fup_bind_queue(pump);
  pump->state = FUP_STATE_IDLE;
  pump->lastError = FUP_ERROR_NONE;
  return 1;
}

uint8_t FUP_IsUploading(const FrameUploadPump *pump) {
  return (pump && pump->state == FUP_STATE_UPLOADING) ? 1 : 0;
}

uint8_t FUP_HasError(const FrameUploadPump *pump) {
  return (pump && pump->state == FUP_STATE_ERROR) ? 1 : 0;
}

uint16_t FUP_NextTile(const FrameUploadPump *pump) {
  return pump ? pump->cursor.nextTile : 0;
}
