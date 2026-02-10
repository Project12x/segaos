/*
 * wm.c - Window Manager Implementation for Genesis System 1
 *
 * Runs on the Sub CPU (Sega CD 68000 @ 12.5MHz).
 * All rendering targets Word RAM (128KB bank in 1M mode).
 *
 * Memory strategy: Static pool, no malloc/free.
 * Z-order: Doubly-linked list (topWindow -> ... -> bottomWindow).
 */

#include "wm.h"
#include <string.h>

/* ============================================================
 * Global State
 * ============================================================ */
static WindowManager wm;

/* ============================================================
 * Internal Helpers
 * ============================================================ */

/* Rect utility functions */
static inline int16_t rect_width(const Rect *r) { return r->right - r->left; }

static inline int16_t rect_height(const Rect *r) { return r->bottom - r->top; }

static inline Boolean rect_contains_point(const Rect *r, Point pt) {
  return (pt.x >= r->left && pt.x < r->right && pt.y >= r->top &&
          pt.y < r->bottom);
}

static inline Boolean rect_intersects(const Rect *a, const Rect *b) {
  return !(a->right <= b->left || a->left >= b->right || a->bottom <= b->top ||
           a->top >= b->bottom);
}

static void rect_union(const Rect *a, const Rect *b, Rect *out) {
  out->left = (a->left < b->left) ? a->left : b->left;
  out->top = (a->top < b->top) ? a->top : b->top;
  out->right = (a->right > b->right) ? a->right : b->right;
  out->bottom = (a->bottom > b->bottom) ? a->bottom : b->bottom;
}

static void rect_clip_to_screen(Rect *r) {
  if (r->left < 0)
    r->left = 0;
  if (r->top < 0)
    r->top = 0;
  if (r->right > WM_SCREEN_W)
    r->right = WM_SCREEN_W;
  if (r->bottom > WM_SCREEN_H)
    r->bottom = WM_SCREEN_H;
}

/* Compute content and title bar rects from frame */
static void compute_window_rects(Window *win) {
/* Mac System 1.0 style metrics */
#define BORDER_W 1    /* 1px border                       */
#define TITLEBAR_H 18 /* Title bar height                 */
#define CLOSE_SIZE 12 /* Close box size                   */
#define SHADOW_W 1    /* Drop shadow width                */

  /* Title bar */
  win->titleBar.left = win->frame.left + BORDER_W;
  win->titleBar.top = win->frame.top + BORDER_W;
  win->titleBar.right = win->frame.right - BORDER_W;
  win->titleBar.bottom = win->frame.top + BORDER_W + TITLEBAR_H;

  /* Content area (below title bar) */
  win->content.left = win->frame.left + BORDER_W;
  win->content.top = win->titleBar.bottom + 1; /* 1px separator */
  win->content.right = win->frame.right - BORDER_W;
  win->content.bottom = win->frame.bottom - BORDER_W;

  /* Adjust for plain/dialog styles */
  if (win->style == WM_STYLE_PLAIN || win->style == WM_STYLE_SHADOW) {
    /* No title bar for plain windows */
    win->titleBar.top = 0;
    win->titleBar.bottom = 0;
    win->content.top = win->frame.top + BORDER_W;
  }
}

/* Allocate a window from the static pool */
static Window *pool_alloc(void) {
  uint8_t i;
  for (i = 0; i < WM_MAX_WINDOWS; i++) {
    if (!wm.poolUsed[i]) {
      wm.poolUsed[i] = 1;
      wm.windowCount++;
      memset(&wm.pool[i], 0, sizeof(Window));
      wm.pool[i].id = i;
      return &wm.pool[i];
    }
  }
  return (Window *)0; /* Pool exhausted */
}

/* Return a window to the pool */
static void pool_free(Window *win) {
  if (win && win->id < WM_MAX_WINDOWS) {
    wm.poolUsed[win->id] = 0;
    wm.windowCount--;
  }
}

/* Unlink a window from the Z-order list */
static void zorder_unlink(Window *win) {
  if (win->above) {
    win->above->below = win->below;
  } else {
    /* Was the top window */
    wm.topWindow = win->below;
  }

  if (win->below) {
    win->below->above = win->above;
  } else {
    /* Was the bottom window */
    wm.bottomWindow = win->above;
  }

  win->above = (Window *)0;
  win->below = (Window *)0;
}

/* Link a window at the top of the Z-order */
static void zorder_push_top(Window *win) {
  win->above = (Window *)0;
  win->below = wm.topWindow;

  if (wm.topWindow) {
    wm.topWindow->above = win;
  }
  wm.topWindow = win;

  if (!wm.bottomWindow) {
    wm.bottomWindow = win;
  }
}

/* ============================================================
 * Public API - Initialization
 * ============================================================ */

void WM_Init(void) {
  memset(&wm, 0, sizeof(WindowManager));
  wm.desktopPattern = 1; /* Gray pattern by default */
  wm.cursorPos.x = WM_SCREEN_W / 2;
  wm.cursorPos.y = WM_SCREEN_H / 2;
  wm.cursorVisible = 1;
}

/* ============================================================
 * Public API - Window Lifecycle
 * ============================================================ */

Window *WM_NewWindow(Rect *bounds, const char *title, WindowStyle style,
                     uint8_t flags) {
  Window *win = pool_alloc();
  if (!win)
    return (Window *)0;

  /* Set frame from caller's bounds */
  win->frame = *bounds;
  rect_clip_to_screen(&win->frame);

  /* Set properties */
  win->style = (uint8_t)style;
  win->flags = flags;

  /* Copy title */
  if (title) {
    uint8_t i;
    for (i = 0; i < WM_TITLE_MAX && title[i]; i++) {
      win->title[i] = title[i];
    }
    win->title[i] = '\0';
  }

  /* Apply style defaults */
  if (style == WM_STYLE_DOCUMENT) {
    win->flags |= WF_HAS_CLOSE;
  }
  if (style == WM_STYLE_DIALOG || style == WM_STYLE_ALERT) {
    win->flags |= WF_MODAL;
  }

  /* Compute sub-rects */
  compute_window_rects(win);

  /* Add to Z-order (on top) */
  zorder_push_top(win);

  /* Make it the active window */
  if (wm.activeWindow) {
    wm.activeWindow->flags &= ~WF_HILITED;
  }
  wm.activeWindow = win;
  win->flags |= WF_HILITED;

  /* Mark as visible and dirty */
  if (flags & WF_VISIBLE) {
    WM_InvalidateWindow(win);
  }

  return win;
}

void WM_DisposeWindow(Window *win) {
  if (!win)
    return;

  /* Invalidate the area it occupied */
  WM_InvalidateRect(&win->frame);

  /* Unlink from Z-order */
  zorder_unlink(win);

  /* If this was the active window, activate the next one */
  if (wm.activeWindow == win) {
    wm.activeWindow = wm.topWindow;
    if (wm.activeWindow) {
      wm.activeWindow->flags |= WF_HILITED;
    }
  }

  /* Return to pool */
  pool_free(win);
}

/* ============================================================
 * Public API - Z-Order
 * ============================================================ */

void WM_SelectWindow(Window *win) {
  if (!win || win == wm.topWindow)
    return;

  /* Deactivate old front window */
  if (wm.activeWindow) {
    wm.activeWindow->flags &= ~WF_HILITED;
    WM_InvalidateWindow(wm.activeWindow);
  }

  /* Move to front */
  zorder_unlink(win);
  zorder_push_top(win);

  /* Activate */
  wm.activeWindow = win;
  win->flags |= WF_HILITED;
  WM_InvalidateWindow(win);
}

void WM_SendToBack(Window *win) {
  if (!win || win == wm.bottomWindow)
    return;

  zorder_unlink(win);

  /* Link at bottom */
  win->below = (Window *)0;
  win->above = wm.bottomWindow;

  if (wm.bottomWindow) {
    wm.bottomWindow->below = win;
  }
  wm.bottomWindow = win;

  if (!wm.topWindow) {
    wm.topWindow = win;
  }

  WM_InvalidateRect(&win->frame);
}

/* ============================================================
 * Public API - Visibility
 * ============================================================ */

void WM_ShowWindow(Window *win) {
  if (!win || (win->flags & WF_VISIBLE))
    return;
  win->flags |= WF_VISIBLE;
  WM_InvalidateWindow(win);
}

void WM_HideWindow(Window *win) {
  if (!win || !(win->flags & WF_VISIBLE))
    return;
  win->flags &= ~WF_VISIBLE;
  WM_InvalidateRect(&win->frame);
}

/* ============================================================
 * Public API - Geometry
 * ============================================================ */

void WM_MoveWindow(Window *win, int16_t x, int16_t y) {
  Rect oldFrame;
  int16_t dx, dy;

  if (!win)
    return;

  /* Save old position for dirty marking */
  oldFrame = win->frame;

  /* Calculate delta */
  dx = x - win->frame.left;
  dy = y - win->frame.top;

  /* Move frame */
  win->frame.left += dx;
  win->frame.top += dy;
  win->frame.right += dx;
  win->frame.bottom += dy;

  /* Clip to screen */
  rect_clip_to_screen(&win->frame);

  /* Recompute sub-rects */
  compute_window_rects(win);

  /* Dirty both old and new positions */
  WM_InvalidateRect(&oldFrame);
  WM_InvalidateWindow(win);
}

void WM_SizeWindow(Window *win, int16_t w, int16_t h) {
  Rect oldFrame;

  if (!win)
    return;

  oldFrame = win->frame;

  win->frame.right = win->frame.left + w;
  win->frame.bottom = win->frame.top + h;

  rect_clip_to_screen(&win->frame);
  compute_window_rects(win);

  WM_InvalidateRect(&oldFrame);
  WM_InvalidateWindow(win);
}

void WM_SetTitle(Window *win, const char *title) {
  uint8_t i;

  if (!win || !title)
    return;

  for (i = 0; i < WM_TITLE_MAX && title[i]; i++) {
    win->title[i] = title[i];
  }
  win->title[i] = '\0';

  /* Only the title bar needs redraw */
  WM_InvalidateRect(&win->titleBar);
}

/* ============================================================
 * Public API - Hit Testing
 * ============================================================ */

WindowPart WM_FindWindow(Point pt, Window **outWin) {
  Window *win;

  *outWin = (Window *)0;

  /* Check menu bar first */
  if (pt.y < WM_MENUBAR_H) {
    return WM_PART_MENUBAR;
  }

  /* Walk Z-order front to back */
  for (win = wm.topWindow; win; win = win->below) {
    if (!(win->flags & WF_VISIBLE))
      continue;

    if (rect_contains_point(&win->frame, pt)) {
      *outWin = win;

      /* Check close box (top-left of title bar) */
      if ((win->flags & WF_HAS_CLOSE) &&
          rect_contains_point(&win->titleBar, pt)) {
        Rect closeBox;
        closeBox.left = win->titleBar.left + 4;
        closeBox.top = win->titleBar.top + 3;
        closeBox.right = closeBox.left + CLOSE_SIZE;
        closeBox.bottom = closeBox.top + CLOSE_SIZE;

        if (rect_contains_point(&closeBox, pt)) {
          return WM_PART_CLOSE;
        }
      }

      /* Check grow box (bottom-right corner) */
      if (win->flags & WF_HAS_GROW) {
        Rect growBox;
        growBox.right = win->frame.right - 1;
        growBox.bottom = win->frame.bottom - 1;
        growBox.left = growBox.right - 12;
        growBox.top = growBox.bottom - 12;

        if (rect_contains_point(&growBox, pt)) {
          return WM_PART_GROW;
        }
      }

      /* Title bar = drag region */
      if (rect_contains_point(&win->titleBar, pt)) {
        return WM_PART_DRAG;
      }

      /* Content area */
      if (rect_contains_point(&win->content, pt)) {
        return WM_PART_CONTENT;
      }

      /* In frame but not content (border) */
      return WM_PART_DRAG;
    }
  }

  return WM_PART_DESKTOP;
}

/* ============================================================
 * Public API - Dirty Rect Management
 * ============================================================ */

void WM_InvalidateRect(Rect *r) {
  Rect clipped;
  uint8_t i;

  if (!r)
    return;

  clipped = *r;
  rect_clip_to_screen(&clipped);

  /* Check if this rect is empty */
  if (clipped.left >= clipped.right || clipped.top >= clipped.bottom) {
    return;
  }

  /* Try to merge with an existing dirty rect */
  for (i = 0; i < wm.dirtyCount; i++) {
    if (wm.dirtyRects[i].valid &&
        rect_intersects(&wm.dirtyRects[i].rect, &clipped)) {
      rect_union(&wm.dirtyRects[i].rect, &clipped, &wm.dirtyRects[i].rect);
      return;
    }
  }

  /* Add as new dirty rect */
  if (wm.dirtyCount < WM_MAX_DIRTY_RECTS) {
    wm.dirtyRects[wm.dirtyCount].rect = clipped;
    wm.dirtyRects[wm.dirtyCount].valid = 1;
    wm.dirtyCount++;
  } else {
    /* Overflow: merge with first rect (worst case) */
    rect_union(&wm.dirtyRects[0].rect, &clipped, &wm.dirtyRects[0].rect);
  }
}

void WM_InvalidateWindow(Window *win) {
  if (win) {
    WM_InvalidateRect(&win->frame);
  }
}

void WM_ValidateRect(Rect *r) {
  /* For now, validation is a no-op. Dirty rects are cleared in EndUpdate. */
  (void)r;
}

/* ============================================================
 * Public API - Render Cycle
 * ============================================================ */

uint8_t WM_BeginUpdate(void) {
  /* Returns the number of dirty rects to process */
  return wm.dirtyCount;
}

DirtyRect *WM_GetDirtyRect(uint8_t index) {
  if (index < wm.dirtyCount) {
    return &wm.dirtyRects[index];
  }
  return (DirtyRect *)0;
}

void WM_EndUpdate(void) {
  /* Clear all dirty rects */
  uint8_t i;
  for (i = 0; i < WM_MAX_DIRTY_RECTS; i++) {
    wm.dirtyRects[i].valid = 0;
  }
  wm.dirtyCount = 0;
  wm.menuBarDirty = 0;
}

/* ============================================================
 * Public API - Desktop
 * ============================================================ */

void WM_DrawDesktop(void) {
  /* TODO: Fill screen with desktop pattern via blitter */
  /* Pattern 0 = white, 1 = 50% gray (checkerboard), 2 = 25% checker */
  (void)wm.desktopPattern;
}

void WM_SetDesktopPattern(uint8_t pattern) {
  if (pattern != wm.desktopPattern) {
    wm.desktopPattern = pattern;
    /* Invalidate entire desktop area (below menu bar) */
    Rect desktop;
    desktop.left = 0;
    desktop.top = WM_MENUBAR_H;
    desktop.right = WM_SCREEN_W;
    desktop.bottom = WM_SCREEN_H;
    WM_InvalidateRect(&desktop);
  }
}

/* ============================================================
 * Public API - Lookup
 * ============================================================ */

Window *WM_GetWindowById(uint8_t id) {
  if (id < WM_MAX_WINDOWS && wm.poolUsed[id]) {
    return &wm.pool[id];
  }
  return (Window *)0;
}

/* ============================================================
 * Public API - Debug
 * ============================================================ */

uint8_t WM_GetWindowCount(void) { return wm.windowCount; }

/* ============================================================
 * Public API - HitTest Wrapper
 * ============================================================ */

HitTestResult WM_HitTest(Point pt) {
  HitTestResult result;
  Window *win = (Window *)0;

  result.part = WM_FindWindow(pt, &win);
  result.window = win;

  return result;
}

/* ============================================================
 * Public API - Traversal
 * ============================================================ */

Window *WM_GetTopWindow(void) { return wm.topWindow; }

Window *WM_GetBottomWindow(void) { return wm.bottomWindow; }

Window *WM_GetActiveWindow(void) { return wm.activeWindow; }
