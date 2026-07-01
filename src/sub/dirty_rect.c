#include "dirty_rect.h"

static int16_t dr_max16(int16_t a, int16_t b) { return (a > b) ? a : b; }
static int16_t dr_min16(int16_t a, int16_t b) { return (a < b) ? a : b; }

Boolean DR_RectIsEmpty(const Rect *r) {
  if (!r)
    return 1;
  return (r->left >= r->right || r->top >= r->bottom) ? 1 : 0;
}

Boolean DR_RectIntersect(const Rect *a, const Rect *b, Rect *out) {
  Rect r;

  if (!a || !b)
    return 0;

  r.top = dr_max16(a->top, b->top);
  r.left = dr_max16(a->left, b->left);
  r.bottom = dr_min16(a->bottom, b->bottom);
  r.right = dr_min16(a->right, b->right);

  if (DR_RectIsEmpty(&r))
    return 0;

  if (out)
    *out = r;
  return 1;
}

void DR_RectUnion(const Rect *a, const Rect *b, Rect *out) {
  if (!out)
    return;
  if (!a || DR_RectIsEmpty(a)) {
    if (b)
      *out = *b;
    return;
  }
  if (!b || DR_RectIsEmpty(b)) {
    *out = *a;
    return;
  }

  out->top = dr_min16(a->top, b->top);
  out->left = dr_min16(a->left, b->left);
  out->bottom = dr_max16(a->bottom, b->bottom);
  out->right = dr_max16(a->right, b->right);
}

Boolean DR_RectClipToBounds(Rect *r, const Rect *bounds) {
  Rect clipped;

  if (!r || !bounds)
    return 0;
  if (!DR_RectIntersect(r, bounds, &clipped))
    return 0;

  *r = clipped;
  return 1;
}

uint8_t DR_RectSubtract(const Rect *src, const Rect *cut, Rect *out,
                        uint8_t maxOut) {
  Rect hit;
  uint8_t count = 0;

  if (!src || !out || maxOut == 0 || DR_RectIsEmpty(src))
    return 0;

  if (!cut || !DR_RectIntersect(src, cut, &hit)) {
    out[0] = *src;
    return 1;
  }

  if (src->top < hit.top && count < maxOut) {
    out[count].top = src->top;
    out[count].left = src->left;
    out[count].bottom = hit.top;
    out[count].right = src->right;
    count++;
  }
  if (hit.bottom < src->bottom && count < maxOut) {
    out[count].top = hit.bottom;
    out[count].left = src->left;
    out[count].bottom = src->bottom;
    out[count].right = src->right;
    count++;
  }
  if (src->left < hit.left && count < maxOut) {
    out[count].top = hit.top;
    out[count].left = src->left;
    out[count].bottom = hit.bottom;
    out[count].right = hit.left;
    count++;
  }
  if (hit.right < src->right && count < maxOut) {
    out[count].top = hit.top;
    out[count].left = hit.right;
    out[count].bottom = hit.bottom;
    out[count].right = src->right;
    count++;
  }

  return count;
}

Boolean DR_RectToTileRange(const Rect *r, uint8_t tileW, uint8_t tileH,
                           DirtyTileRange *out) {
  if (!r || !out || tileW == 0 || tileH == 0 || DR_RectIsEmpty(r))
    return 0;
  if (r->left < 0 || r->top < 0 || r->right < 0 || r->bottom < 0)
    return 0;

  out->x0 = (uint16_t)(r->left / tileW);
  out->y0 = (uint16_t)(r->top / tileH);
  out->x1 = (uint16_t)((r->right + tileW - 1) / tileW);
  out->y1 = (uint16_t)((r->bottom + tileH - 1) / tileH);
  return 1;
}

static void dr_clear_budget(DirtyTransferBudget *out) {
  if (!out)
    return;

  out->firstTile = 0;
  out->tileCount = 0;
  out->byteCount = 0;
  out->rowSpanCount = 0;
  out->fitsBudget = 0;
  out->_pad = 0;
}

Boolean DR_TileRangeBudget(const DirtyTileRange *range, uint16_t planeTilesX,
                           uint16_t bytesPerTile, uint16_t maxBytes,
                           DirtyTransferBudget *out) {
  uint32_t width;
  uint32_t height;
  uint32_t firstTile;
  uint32_t tileCount;
  uint32_t byteCount;
  uint32_t rowSpanCount;

  dr_clear_budget(out);

  if (!range || !out || planeTilesX == 0 || bytesPerTile == 0)
    return 0;
  if (range->x0 >= range->x1 || range->y0 >= range->y1)
    return 0;
  if (range->x1 > planeTilesX)
    return 0;

  width = (uint32_t)(range->x1 - range->x0);
  height = (uint32_t)(range->y1 - range->y0);
  firstTile = ((uint32_t)range->y0 * planeTilesX) + range->x0;
  tileCount = width * height;
  byteCount = tileCount * bytesPerTile;
  rowSpanCount = (range->x0 == 0 && width == planeTilesX) ? 1U : height;

  if (firstTile > 0xffffU || tileCount > 0xffffU || byteCount > 0xffffU ||
      rowSpanCount > 0xffffU) {
    return 0;
  }

  out->firstTile = (uint16_t)firstTile;
  out->tileCount = (uint16_t)tileCount;
  out->byteCount = (uint16_t)byteCount;
  out->rowSpanCount = (uint16_t)rowSpanCount;
  out->fitsBudget = (byteCount <= maxBytes) ? 1 : 0;
  return 1;
}

void DR_PlanRootRedraw(const Rect *dirty, int16_t menuBarHeight,
                       DirtyRootRedraw *out) {
  Rect part;

  if (!out)
    return;

  out->hasMenu = 0;
  out->hasDesktop = 0;
  out->menu.top = 0;
  out->menu.left = 0;
  out->menu.bottom = 0;
  out->menu.right = 0;
  out->desktop = out->menu;

  if (!dirty || DR_RectIsEmpty(dirty))
    return;

  if (menuBarHeight < 0)
    menuBarHeight = 0;

  if (dirty->top < menuBarHeight) {
    part = *dirty;
    if (part.bottom > menuBarHeight)
      part.bottom = menuBarHeight;
    if (!DR_RectIsEmpty(&part)) {
      out->menu = part;
      out->hasMenu = 1;
    }
  }

  if (dirty->bottom > menuBarHeight) {
    part = *dirty;
    if (part.top < menuBarHeight)
      part.top = menuBarHeight;
    if (!DR_RectIsEmpty(&part)) {
      out->desktop = part;
      out->hasDesktop = 1;
    }
  }
}

void DR_PlanWindowRedraw(const Rect *dirty, const Rect *windowBounds,
                         DirtyWindowRedraw *out) {
  if (!out)
    return;

  out->hasWindow = 0;
  out->clip.top = 0;
  out->clip.left = 0;
  out->clip.bottom = 0;
  out->clip.right = 0;

  if (!dirty || !windowBounds || DR_RectIsEmpty(dirty) ||
      DR_RectIsEmpty(windowBounds)) {
    return;
  }

  if (DR_RectIntersect(dirty, windowBounds, &out->clip)) {
    out->hasWindow = 1;
  }
}

static Boolean dr_rect_can_merge(const Rect *a, const Rect *b) {
  Boolean horizontalOverlap;
  Boolean verticalOverlap;
  Boolean horizontalTouch;
  Boolean verticalTouch;

  if (!a || !b)
    return 0;

  if (DR_RectIntersect(a, b, (Rect *)0))
    return 1;

  horizontalOverlap = (a->left < b->right && b->left < a->right) ? 1 : 0;
  verticalOverlap = (a->top < b->bottom && b->top < a->bottom) ? 1 : 0;
  horizontalTouch = (a->right == b->left || b->right == a->left) ? 1 : 0;
  verticalTouch = (a->bottom == b->top || b->bottom == a->top) ? 1 : 0;

  return ((verticalOverlap && horizontalTouch) ||
          (horizontalOverlap && verticalTouch))
             ? 1
             : 0;
}

static void dr_remove_at(DirtyRectList *list, uint8_t index) {
  uint8_t i;

  if (!list || index >= list->count)
    return;

  for (i = index; i + 1 < list->count; i++) {
    list->items[i] = list->items[i + 1];
  }
  if (list->count > 0) {
    list->count--;
    list->items[list->count].valid = 0;
  }
}

static void dr_collapse_to_union(DirtyRectList *list, const Rect *r) {
  Rect merged;
  uint8_t i;

  if (!list || !list->items || list->capacity == 0 || !r)
    return;

  merged = *r;
  for (i = 0; i < list->count; i++) {
    if (list->items[i].valid) {
      DR_RectUnion(&merged, &list->items[i].rect, &merged);
    }
  }

  DR_ClearList(list);
  list->items[0].rect = merged;
  list->items[0].valid = 1;
  list->count = 1;
}

void DR_InitList(DirtyRectList *list, DirtyRect *storage, uint8_t capacity,
                 const Rect *bounds) {
  if (!list)
    return;

  list->items = storage;
  list->capacity = capacity;
  list->count = 0;
  if (bounds) {
    list->bounds = *bounds;
  } else {
    list->bounds.top = 0;
    list->bounds.left = 0;
    list->bounds.bottom = 0;
    list->bounds.right = 0;
  }
  DR_ClearList(list);
}

void DR_ClearList(DirtyRectList *list) {
  uint8_t i;

  if (!list || !list->items)
    return;

  for (i = 0; i < list->capacity; i++) {
    list->items[i].valid = 0;
  }
  list->count = 0;
}

Boolean DR_AddRect(DirtyRectList *list, const Rect *r) {
  Rect clipped;
  uint8_t i;

  if (!list || !list->items || list->capacity == 0 || !r)
    return 0;

  clipped = *r;
  if (!DR_RectClipToBounds(&clipped, &list->bounds))
    return 0;

  for (i = 0; i < list->count; i++) {
    if (list->items[i].valid &&
        dr_rect_can_merge(&list->items[i].rect, &clipped)) {
      uint8_t j;
      DR_RectUnion(&list->items[i].rect, &clipped, &list->items[i].rect);

      j = 0;
      while (j < list->count) {
        if (j != i && list->items[j].valid &&
            dr_rect_can_merge(&list->items[i].rect, &list->items[j].rect)) {
          DR_RectUnion(&list->items[i].rect, &list->items[j].rect,
                       &list->items[i].rect);
          dr_remove_at(list, j);
          if (j < i)
            i--;
        } else {
          j++;
        }
      }
      return 1;
    }
  }

  if (list->count < list->capacity) {
    list->items[list->count].rect = clipped;
    list->items[list->count].valid = 1;
    list->count++;
    return 1;
  }

  dr_collapse_to_union(list, &clipped);
  return 1;
}

uint8_t DR_GetCount(const DirtyRectList *list) {
  if (!list)
    return 0;
  return list->count;
}

DirtyRect *DR_GetRect(DirtyRectList *list, uint8_t index) {
  if (!list || !list->items || index >= list->count)
    return (DirtyRect *)0;
  return &list->items[index];
}
