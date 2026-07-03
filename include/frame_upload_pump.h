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
