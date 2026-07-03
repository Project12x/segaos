/*
 * frame_scheduler.h - Budgeted frame-upload planning for SegaOS.
 *
 * This is a Main CPU policy seam. It does not touch VDP registers; it advances
 * a tile cursor into DirtyTileQueue slices that can later be flushed during a
 * measured VBlank/update window.
 */

#ifndef FRAME_SCHEDULER_H
#define FRAME_SCHEDULER_H

#include "dirty_rect.h"
#include <stdint.h>

typedef struct {
  uint16_t firstTile;
  uint16_t tileCount;
  uint16_t nextTile;
  uint8_t active;
  uint8_t _pad;
} FrameTileCursor;

typedef struct {
  uint8_t queued;
  uint8_t complete;
  uint8_t budgetLimited;
  uint8_t overflow;
  uint16_t queuedTiles;
  uint16_t queuedBytes;
  uint16_t remainingTiles;
  uint16_t nextTile;
} FrameScheduleResult;

void FS_ClearTileCursor(FrameTileCursor *cursor);
void FS_StartTileCursor(FrameTileCursor *cursor, uint16_t firstTile,
                        uint16_t tileCount);
uint8_t FS_PlanTileCursorFrame(FrameTileCursor *cursor, DirtyTileQueue *queue,
                               uint16_t bytesPerTile,
                               FrameScheduleResult *result);

#endif /* FRAME_SCHEDULER_H */
