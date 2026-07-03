#include "frame_scheduler.h"

static void fs_clear_result(FrameScheduleResult *result) {
  if (!result)
    return;

  result->queued = 0;
  result->complete = 0;
  result->budgetLimited = 0;
  result->overflow = 0;
  result->queuedTiles = 0;
  result->queuedBytes = 0;
  result->remainingTiles = 0;
  result->nextTile = 0;
}

static void fs_clear_queue(DirtyTileQueue *queue) {
  uint8_t i;

  if (!queue)
    return;

  queue->count = 0;
  queue->byteCount = 0;
  queue->budgetExceeded = 0;
  queue->overflow = 0;

  if (!queue->items)
    return;

  for (i = 0; i < queue->capacity; i++) {
    queue->items[i].firstTile = 0;
    queue->items[i].tileCount = 0;
    queue->items[i].byteCount = 0;
  }
}

void FS_ClearTileCursor(FrameTileCursor *cursor) {
  if (!cursor)
    return;

  cursor->firstTile = 0;
  cursor->tileCount = 0;
  cursor->nextTile = 0;
  cursor->active = 0;
  cursor->_pad = 0;
}

void FS_StartTileCursor(FrameTileCursor *cursor, uint16_t firstTile,
                        uint16_t tileCount) {
  if (!cursor)
    return;

  cursor->firstTile = firstTile;
  cursor->tileCount = tileCount;
  cursor->nextTile = firstTile;
  cursor->active = (tileCount != 0) ? 1 : 0;
  cursor->_pad = 0;
}

static uint16_t fs_min_u16(uint16_t a, uint16_t b) {
  return (a < b) ? a : b;
}

uint8_t FS_PlanTileCursorFrame(FrameTileCursor *cursor, DirtyTileQueue *queue,
                               uint16_t bytesPerTile,
                               FrameScheduleResult *result) {
  uint16_t endTile;
  uint16_t remaining;
  uint16_t fitTiles;
  uint16_t queuedTiles;
  uint16_t queuedBytes;

  fs_clear_result(result);
  if (queue)
    fs_clear_queue(queue);

  if (!cursor || !queue || bytesPerTile == 0)
    return 0;

  if (!cursor->active || cursor->tileCount == 0) {
    if (result) {
      result->complete = 1;
      result->nextTile = cursor->nextTile;
    }
    return 1;
  }

  endTile = (uint16_t)(cursor->firstTile + cursor->tileCount);
  if (endTile < cursor->firstTile)
    endTile = 0xffffU;
  if (cursor->nextTile < cursor->firstTile || cursor->nextTile >= endTile) {
    cursor->nextTile = endTile;
    cursor->active = 0;
    if (result) {
      result->complete = 1;
      result->nextTile = cursor->nextTile;
    }
    return 1;
  }

  remaining = (uint16_t)(endTile - cursor->nextTile);
  if (!queue->items || queue->capacity == 0) {
    queue->overflow = 1;
    if (result) {
      result->overflow = 1;
      result->remainingTiles = remaining;
      result->nextTile = cursor->nextTile;
    }
    return 0;
  }

  if (queue->maxBytes != 0) {
    fitTiles = (uint16_t)(queue->maxBytes / bytesPerTile);
  } else {
    fitTiles = (uint16_t)(0xffffU / bytesPerTile);
  }

  if (fitTiles == 0) {
    queue->budgetExceeded = 1;
    if (result) {
      result->budgetLimited = 1;
      result->remainingTiles = remaining;
      result->nextTile = cursor->nextTile;
    }
    return 0;
  }

  queuedTiles = fs_min_u16(remaining, fitTiles);
  queuedBytes = (uint16_t)(queuedTiles * bytesPerTile);

  queue->items[0].firstTile = cursor->nextTile;
  queue->items[0].tileCount = queuedTiles;
  queue->items[0].byteCount = queuedBytes;
  queue->count = 1;
  queue->byteCount = queuedBytes;
  queue->budgetExceeded = (queuedTiles < remaining) ? 1 : 0;
  queue->overflow = 0;

  cursor->nextTile = (uint16_t)(cursor->nextTile + queuedTiles);
  remaining = (uint16_t)(endTile - cursor->nextTile);
  if (remaining == 0) {
    cursor->active = 0;
  }

  if (result) {
    result->queued = 1;
    result->complete = cursor->active ? 0 : 1;
    result->budgetLimited = queue->budgetExceeded;
    result->overflow = 0;
    result->queuedTiles = queuedTiles;
    result->queuedBytes = queuedBytes;
    result->remainingTiles = (uint16_t)remaining;
    result->nextTile = cursor->nextTile;
  }

  return 1;
}
