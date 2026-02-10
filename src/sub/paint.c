/*
 * paint.c - Paint Application Implementation
 *
 * Drawing tool with pencil, eraser, line,
 * rectangle, and filled rectangle tools.
 * Canvas is a persistent 1-bit bitmap blitted to screen on draw.
 * Canvas pixel ops use 1/0 (set/clear) internally.
 * BLT_BlitBitmap1 expands 1-bit canvas to active palette color.
 */

#include "paint.h"
#include "blitter.h"
#include "sysfont.h"
#include <string.h>

/* ============================================================
 * Static State
 * ============================================================ */
static PaintState paintState;
static Window *paintWindow = (Window *)0;

/* Tool labels (3 chars each for toolbar buttons) */
static const char *toolLabels[PAINT_TOOL_COUNT] = {"Pen", "Era", "Lin",
                                                   "Rct", "Fil", "Clr"};

/* ============================================================
 * Canvas Pixel Operations
 * ============================================================ */

static void canvas_set_pixel(int16_t x, int16_t y, uint8_t color) {
  uint16_t byteIdx;
  uint8_t bit;

  if (x < 0 || x >= PAINT_CANVAS_W || y < 0 || y >= PAINT_CANVAS_H)
    return;

  byteIdx = (uint16_t)y * PAINT_STRIDE + (uint16_t)(x >> 3);
  bit = 0x80 >> (x & 7);

  if (color)
    paintState.canvas[byteIdx] |= bit;
  else
    paintState.canvas[byteIdx] &= ~bit;
}

/* Bresenham line on the canvas buffer */
static void canvas_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             uint8_t color) {
  int16_t dx, dy, sx, sy, err, e2;

  dx = x1 - x0;
  if (dx < 0)
    dx = -dx;
  dy = y1 - y0;
  if (dy < 0)
    dy = -dy;

  sx = (x0 < x1) ? 1 : -1;
  sy = (y0 < y1) ? 1 : -1;
  err = dx - dy;

  for (;;) {
    canvas_set_pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = err * 2;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

/* Rectangle outline on canvas */
static void canvas_draw_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             uint8_t color) {
  int16_t tmp, x, y;

  /* Normalize so x0 <= x1, y0 <= y1 */
  if (x0 > x1) {
    tmp = x0;
    x0 = x1;
    x1 = tmp;
  }
  if (y0 > y1) {
    tmp = y0;
    y0 = y1;
    y1 = tmp;
  }

  /* Top and bottom edges */
  for (x = x0; x <= x1; x++) {
    canvas_set_pixel(x, y0, color);
    canvas_set_pixel(x, y1, color);
  }
  /* Left and right edges */
  for (y = y0 + 1; y < y1; y++) {
    canvas_set_pixel(x0, y, color);
    canvas_set_pixel(x1, y, color);
  }
}

/* Filled rectangle on canvas */
static void canvas_fill_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                             uint8_t color) {
  int16_t tmp, x, y;

  if (x0 > x1) {
    tmp = x0;
    x0 = x1;
    x1 = tmp;
  }
  if (y0 > y1) {
    tmp = y0;
    y0 = y1;
    y1 = tmp;
  }

  for (y = y0; y <= y1; y++) {
    for (x = x0; x <= x1; x++) {
      canvas_set_pixel(x, y, color);
    }
  }
}

/* Eraser brush (4x4 block of white pixels) */
static void canvas_erase_at(int16_t cx, int16_t cy) {
  int16_t x, y;
  for (y = cy - 1; y <= cy + 2; y++) {
    for (x = cx - 1; x <= cx + 2; x++) {
      canvas_set_pixel(x, y, 0); /* White */
    }
  }
}

/* Eraser line (draws 4x4 eraser along a Bresenham path) */
static void canvas_erase_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
  int16_t dx, dy, sx, sy, err, e2;

  dx = x1 - x0;
  if (dx < 0)
    dx = -dx;
  dy = y1 - y0;
  if (dy < 0)
    dy = -dy;

  sx = (x0 < x1) ? 1 : -1;
  sy = (y0 < y1) ? 1 : -1;
  err = dx - dy;

  for (;;) {
    canvas_erase_at(x0, y0);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = err * 2;
    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}

/* ============================================================
 * Coordinate Conversion
 * ============================================================ */

/* Convert screen coordinates to canvas coordinates.
 * Returns 1 if point is inside canvas, 0 if outside. */
static uint8_t screen_to_canvas(Window *win, Point screen, int16_t *cx,
                                int16_t *cy) {
  int16_t canvasLeft = win->content.left + PAINT_TOOLBAR_W;
  int16_t canvasTop = win->content.top;

  *cx = screen.x - canvasLeft;
  *cy = screen.y - canvasTop;

  return (*cx >= 0 && *cx < PAINT_CANVAS_W && *cy >= 0 && *cy < PAINT_CANVAS_H);
}

/* Check if a screen point is in the toolbar area */
static int8_t toolbar_hit(Window *win, Point screen) {
  int16_t lx = screen.x - win->content.left;
  int16_t ly = screen.y - win->content.top;
  int8_t btnIdx;

  if (lx < 0 || lx >= PAINT_TOOLBAR_W || ly < 0)
    return -1;

  btnIdx = (int8_t)(ly / (PAINT_TOOL_BTN_H + PAINT_TOOL_PAD));
  if (btnIdx >= PAINT_TOOL_COUNT)
    return -1;

  return btnIdx;
}

/* ============================================================
 * Window Callbacks
 * ============================================================ */

void Paint_Draw(Window *win) {
  int16_t tx = win->content.left;
  int16_t ty = win->content.top;
  int16_t canvasLeft = tx + PAINT_TOOLBAR_W;
  uint8_t i;
  Rect btnRect, canvasRect, borderRect;

  /* --- Toolbar --- */
  /* Background */
  btnRect.left = tx;
  btnRect.top = ty;
  btnRect.right = tx + PAINT_TOOLBAR_W - 1;
  btnRect.bottom = ty + PAINT_TOOL_COUNT * (PAINT_TOOL_BTN_H + PAINT_TOOL_PAD);
  BLT_FillRect(&btnRect, BLT_GetWhite());

  /* Tool buttons */
  for (i = 0; i < PAINT_TOOL_COUNT; i++) {
    int16_t by = ty + i * (PAINT_TOOL_BTN_H + PAINT_TOOL_PAD);
    uint8_t selected =
        (i == (uint8_t)paintState.currentTool && i < PAINT_TOOL_CLEAR);

    btnRect.left = tx + 1;
    btnRect.top = by;
    btnRect.right = tx + PAINT_TOOLBAR_W - 2;
    btnRect.bottom = by + PAINT_TOOL_BTN_H;

    /* Invert selected tool */
    BLT_FillRect(&btnRect, selected ? BLT_BLACK : BLT_GetWhite());

    /* Border */
    BLT_DrawRect(&btnRect, BLT_BLACK);

    /* Label */
    SysFont_DrawString(tx + 2, by + 3, toolLabels[i],
                       selected ? BLT_GetWhite() : BLT_BLACK);
  }

  /* Toolbar / canvas separator */
  BLT_DrawVLine(canvasLeft - 1, ty, PAINT_CANVAS_H, BLT_BLACK);

  /* --- Canvas --- */
  /* White background */
  canvasRect.left = canvasLeft;
  canvasRect.top = ty;
  canvasRect.right = canvasLeft + PAINT_CANVAS_W;
  canvasRect.bottom = ty + PAINT_CANVAS_H;
  BLT_FillRect(&canvasRect, BLT_GetWhite());

  /* Blit 1-bit canvas: set bits draw as black */
  BLT_BlitBitmap1(canvasLeft, ty, paintState.canvas, PAINT_CANVAS_W,
                  PAINT_CANVAS_H, BLT_BLACK);

  /* Canvas border */
  borderRect.left = canvasLeft - 1;
  borderRect.top = ty - 1;
  borderRect.right = canvasLeft + PAINT_CANVAS_W;
  borderRect.bottom = ty + PAINT_CANVAS_H;
  BLT_DrawRect(&borderRect, BLT_BLACK);

  /* Anchor marker for two-click tools */
  if (paintState.anchorSet) {
    int16_t ax = canvasLeft + paintState.anchorX;
    int16_t ay = ty + paintState.anchorY;
    /* Draw small crosshair at anchor */
    BLT_SetPixel(ax, ay, BLT_BLACK);
    BLT_SetPixel(ax - 2, ay, BLT_BLACK);
    BLT_SetPixel(ax + 2, ay, BLT_BLACK);
    BLT_SetPixel(ax, ay - 2, BLT_BLACK);
    BLT_SetPixel(ax, ay + 2, BLT_BLACK);
  }
}

void Paint_Click(Window *win, Point where) {
  int16_t cx, cy;
  int8_t toolBtn;

  /* Check toolbar hit first */
  toolBtn = toolbar_hit(win, where);
  if (toolBtn >= 0) {
    if (toolBtn == PAINT_TOOL_CLEAR) {
      /* Clear canvas action */
      memset(paintState.canvas, 0, PAINT_BUF_SIZE);
      paintState.anchorSet = 0;
    } else {
      paintState.currentTool = (PaintTool)toolBtn;
      paintState.anchorSet = 0; /* Reset anchor on tool change */
    }
    paintState.hasLast = 0;
    WM_InvalidateWindow(win);
    return;
  }

  /* Check canvas hit */
  if (!screen_to_canvas(win, where, &cx, &cy))
    return;

  switch (paintState.currentTool) {
  case PAINT_TOOL_PENCIL:
    canvas_set_pixel(cx, cy, 1);
    paintState.lastX = cx;
    paintState.lastY = cy;
    paintState.hasLast = 1;
    break;

  case PAINT_TOOL_ERASER:
    canvas_erase_at(cx, cy);
    paintState.lastX = cx;
    paintState.lastY = cy;
    paintState.hasLast = 1;
    break;

  case PAINT_TOOL_LINE:
    if (!paintState.anchorSet) {
      paintState.anchorX = cx;
      paintState.anchorY = cy;
      paintState.anchorSet = 1;
    } else {
      canvas_draw_line(paintState.anchorX, paintState.anchorY, cx, cy, 1);
      paintState.anchorSet = 0;
    }
    break;

  case PAINT_TOOL_RECT:
    if (!paintState.anchorSet) {
      paintState.anchorX = cx;
      paintState.anchorY = cy;
      paintState.anchorSet = 1;
    } else {
      canvas_draw_rect(paintState.anchorX, paintState.anchorY, cx, cy, 1);
      paintState.anchorSet = 0;
    }
    break;

  case PAINT_TOOL_FILL_RECT:
    if (!paintState.anchorSet) {
      paintState.anchorX = cx;
      paintState.anchorY = cy;
      paintState.anchorSet = 1;
    } else {
      canvas_fill_rect(paintState.anchorX, paintState.anchorY, cx, cy, 1);
      paintState.anchorSet = 0;
    }
    break;

  default:
    break;
  }

  WM_InvalidateWindow(win);
}

void Paint_Drag(Window *win, Point where) {
  int16_t cx, cy;

  if (!screen_to_canvas(win, where, &cx, &cy))
    return;

  switch (paintState.currentTool) {
  case PAINT_TOOL_PENCIL:
    if (paintState.hasLast) {
      canvas_draw_line(paintState.lastX, paintState.lastY, cx, cy, 1);
    } else {
      canvas_set_pixel(cx, cy, 1);
    }
    paintState.lastX = cx;
    paintState.lastY = cy;
    paintState.hasLast = 1;
    WM_InvalidateWindow(win);
    break;

  case PAINT_TOOL_ERASER:
    if (paintState.hasLast) {
      canvas_erase_line(paintState.lastX, paintState.lastY, cx, cy);
    } else {
      canvas_erase_at(cx, cy);
    }
    paintState.lastX = cx;
    paintState.lastY = cy;
    paintState.hasLast = 1;
    WM_InvalidateWindow(win);
    break;

  default:
    /* Line/rect tools don't use drag */
    break;
  }
}

/* ============================================================
 * Public API
 * ============================================================ */

void Paint_Open(void) {
  Rect bounds;
  int16_t winW, winH;

  if (paintWindow)
    return;

  /* Clear state and canvas */
  memset(&paintState, 0, sizeof(PaintState));
  paintState.currentTool = PAINT_TOOL_PENCIL;
  paintState.drawColor = BLT_BLACK;

  /* Window sized to fit toolbar + canvas + chrome */
  winW = PAINT_TOOLBAR_W + PAINT_CANVAS_W + 4;
  winH = PAINT_CANVAS_H + 24; /* +24 for title bar */

  bounds.left = 15;
  bounds.top = 25;
  bounds.right = bounds.left + winW;
  bounds.bottom = bounds.top + winH;

  paintWindow = WM_NewWindow(&bounds, "Paint", WM_STYLE_DOCUMENT,
                             WF_VISIBLE | WF_HAS_CLOSE);

  if (paintWindow) {
    paintWindow->drawProc = Paint_Draw;
    paintWindow->clickProc = Paint_Click;
    paintWindow->dragProc = Paint_Drag;
  }
}
