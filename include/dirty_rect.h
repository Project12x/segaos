/*
 * dirty_rect.h - Clean-room dirty rectangle helpers for SegaOS redraw.
 *
 * Coordinates use the classic Mac-style Rect already used by SegaOS:
 * top/left inclusive, bottom/right exclusive.
 */

#ifndef DIRTY_RECT_H
#define DIRTY_RECT_H

#include "sega_os.h"
#include <stdint.h>

typedef struct {
  Rect rect;
  uint8_t valid;
  uint8_t _pad;
} DirtyRect;

typedef struct {
  uint16_t x0;
  uint16_t y0;
  uint16_t x1;
  uint16_t y1;
} DirtyTileRange;

typedef struct {
  uint16_t firstTile;
  uint16_t tileCount;
  uint16_t byteCount;
  uint16_t rowSpanCount;
  uint8_t fitsBudget;
  uint8_t _pad;
} DirtyTransferBudget;

typedef struct {
  uint16_t firstTile;
  uint16_t tileCount;
  uint16_t byteCount;
} DirtyTileUpload;

typedef struct {
  DirtyTileUpload *items;
  uint8_t capacity;
  uint8_t count;
  uint16_t maxBytes;
  uint16_t byteCount;
  uint8_t budgetExceeded;
  uint8_t overflow;
} DirtyTileQueue;

typedef struct {
  DirtyRect *items;
  uint8_t capacity;
  uint8_t count;
  Rect bounds;
} DirtyRectList;

typedef struct {
  Rect menu;
  Rect desktop;
  uint8_t hasMenu;
  uint8_t hasDesktop;
} DirtyRootRedraw;

typedef struct {
  Rect clip;
  uint8_t hasWindow;
  uint8_t _pad;
} DirtyWindowRedraw;

Boolean DR_RectIsEmpty(const Rect *r);
Boolean DR_RectIntersect(const Rect *a, const Rect *b, Rect *out);
void DR_RectUnion(const Rect *a, const Rect *b, Rect *out);
Boolean DR_RectClipToBounds(Rect *r, const Rect *bounds);
uint8_t DR_RectSubtract(const Rect *src, const Rect *cut, Rect *out,
                        uint8_t maxOut);
Boolean DR_RectToTileRange(const Rect *r, uint8_t tileW, uint8_t tileH,
                           DirtyTileRange *out);
Boolean DR_TileRangeBudget(const DirtyTileRange *range, uint16_t planeTilesX,
                           uint16_t bytesPerTile, uint16_t maxBytes,
                           DirtyTransferBudget *out);
void DR_InitTileQueue(DirtyTileQueue *queue, DirtyTileUpload *storage,
                      uint8_t capacity, uint16_t maxBytes);
void DR_ClearTileQueue(DirtyTileQueue *queue);
Boolean DR_QueueTileRange(DirtyTileQueue *queue, const DirtyTileRange *range,
                          uint16_t planeTilesX, uint16_t bytesPerTile);
Boolean DR_BuildTileQueueFromDirtyList(const DirtyRectList *list,
                                       DirtyTileQueue *queue, uint8_t tileW,
                                       uint8_t tileH, uint16_t planeTilesX,
                                       uint16_t bytesPerTile);
DirtyTileUpload *DR_GetTileUpload(DirtyTileQueue *queue, uint8_t index);
void DR_PlanRootRedraw(const Rect *dirty, int16_t menuBarHeight,
                       DirtyRootRedraw *out);
void DR_PlanWindowRedraw(const Rect *dirty, const Rect *windowBounds,
                         DirtyWindowRedraw *out);

void DR_InitList(DirtyRectList *list, DirtyRect *storage, uint8_t capacity,
                 const Rect *bounds);
void DR_ClearList(DirtyRectList *list);
Boolean DR_AddRect(DirtyRectList *list, const Rect *r);
uint8_t DR_GetCount(const DirtyRectList *list);
DirtyRect *DR_GetRect(DirtyRectList *list, uint8_t index);

#endif /* DIRTY_RECT_H */
