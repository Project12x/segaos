/*
 * paint.h - Paint Application for Genesis System 1
 *
 * Simple drawing tool with multiple tools:
 *   - Pencil (freehand drawing)
 *   - Eraser (freehand erase, 4x4 brush)
 *   - Line (two-click: anchor + endpoint)
 *   - Rectangle outline (two-click)
 *   - Filled rectangle (two-click)
 *
 * Canvas is stored as a 1-bit bitmap for simplicity.
 * Drawing color is mapped to BLT_BLACK for rendering.
 * In Paint mode, the blitter can be switched to 4-bit
 * for a full 16-color palette.
 */

#ifndef PAINT_H
#define PAINT_H

#include "wm.h"

/* ============================================================
 * Canvas Configuration
 *
 * Canvas is always stored as 1-bit internally for
 * compactness. Rendering expands to current blit mode.
 * ============================================================ */
#define PAINT_CANVAS_W 240                /* Canvas width in pixels       */
#define PAINT_CANVAS_H 150                /* Canvas height in pixels      */
#define PAINT_STRIDE (PAINT_CANVAS_W / 8) /* 30 bytes/row */
#define PAINT_BUF_SIZE (PAINT_STRIDE * PAINT_CANVAS_H) /* 4500 */

/* UI Layout */
#define PAINT_TOOLBAR_W 20  /* Toolbar strip width          */
#define PAINT_TOOL_BTN_H 16 /* Tool button height           */
#define PAINT_TOOL_PAD 2    /* Padding between buttons      */

/* ============================================================
 * Drawing Tools
 * ============================================================ */
typedef enum {
  PAINT_TOOL_PENCIL = 0,
  PAINT_TOOL_ERASER,
  PAINT_TOOL_LINE,
  PAINT_TOOL_RECT,
  PAINT_TOOL_FILL_RECT,
  PAINT_TOOL_CLEAR, /* Not a real tool - action button */
  PAINT_TOOL_COUNT
} PaintTool;

/* ============================================================
 * Paint State
 * ============================================================ */
typedef struct {
  uint8_t canvas[PAINT_BUF_SIZE]; /* 1-bit canvas bitmap */
  PaintTool currentTool;
  uint8_t drawColor; /* Palette index for drawing          */
  uint8_t anchorSet; /* 1 = first click placed (line/rect) */
  int16_t anchorX;   /* First click X (canvas coords)      */
  int16_t anchorY;   /* First click Y (canvas coords)      */
  int16_t lastX;     /* Last pencil/eraser position        */
  int16_t lastY;
  uint8_t hasLast; /* 1 = lastX/lastY are valid (drag)   */
} PaintState;

/* ============================================================
 * Public API
 * ============================================================ */

/* Open the paint window (creates window, inits canvas) */
void Paint_Open(void);

/* Window callbacks (registered with WM) */
void Paint_Draw(Window *win);
void Paint_Click(Window *win, Point where);
void Paint_Drag(Window *win, Point where);

#endif /* PAINT_H */
