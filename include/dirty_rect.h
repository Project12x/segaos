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

Boolean DR_RectIsEmpty(const Rect *r);
Boolean DR_RectIntersect(const Rect *a, const Rect *b, Rect *out);
void DR_RectUnion(const Rect *a, const Rect *b, Rect *out);
Boolean DR_RectClipToBounds(Rect *r, const Rect *bounds);
uint8_t DR_RectSubtract(const Rect *src, const Rect *cut, Rect *out,
                        uint8_t maxOut);
Boolean DR_RectToTileRange(const Rect *r, uint8_t tileW, uint8_t tileH,
                           DirtyTileRange *out);
void DR_PlanRootRedraw(const Rect *dirty, int16_t menuBarHeight,
                       DirtyRootRedraw *out);

void DR_InitList(DirtyRectList *list, DirtyRect *storage, uint8_t capacity,
                 const Rect *bounds);
void DR_ClearList(DirtyRectList *list);
Boolean DR_AddRect(DirtyRectList *list, const Rect *r);
uint8_t DR_GetCount(const DirtyRectList *list);
DirtyRect *DR_GetRect(DirtyRectList *list, uint8_t index);

#endif /* DIRTY_RECT_H */
