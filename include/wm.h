/*
 * wm.h - Window Manager for Genesis System 1
 *
 * Mac System 1.0 / GEOS style window management.
 * Runs on the Sub CPU (Sega CD 68000 @ 12.5MHz).
 *
 * Design constraints:
 *   - 320x224 resolution, 1-bit (B&W) rendering
 *   - Max ~16 windows (RAM budget: ~2KB for window records)
 *   - Dirty rectangle tracking for partial screen updates
 *   - Cooperative: only one window moves/resizes at a time
 */

#ifndef WM_H
#define WM_H

#include "sega_os.h"

/* ============================================================
 * Configuration
 * ============================================================ */
#define WM_MAX_WINDOWS 16
#define WM_MAX_DIRTY_RECTS 32 /* Max dirty rects per frame    */
#define WM_TITLE_MAX 31       /* Max title length (+ null)    */
#define WM_SCREEN_W 320
#define WM_SCREEN_H 224
#define WM_MENUBAR_H 20 /* Menu bar height in pixels    */

/* ============================================================
 * Window Parts (hit-test results)
 * Mirrors classic Mac "FindWindow" part codes
 * ============================================================ */
typedef enum {
  WM_PART_NONE = 0,    /* Not in any window                */
  WM_PART_MENUBAR = 1, /* In the menu bar                  */
  WM_PART_DESKTOP = 2, /* On the desktop (behind windows)  */
  WM_PART_DRAG = 3,    /* In the title/drag bar            */
  WM_PART_CONTENT = 4, /* In the content area              */
  WM_PART_CLOSE = 5,   /* On the close box                 */
  WM_PART_GROW = 6,    /* On the grow/resize box           */
  WM_PART_GOAWAY = 7   /* On the go-away box (same as close) */
} WindowPart;

/* Aliases used by Sub CPU event handler */
#define WM_HIT_NONE WM_PART_NONE
#define WM_HIT_MENUBAR WM_PART_MENUBAR
#define WM_HIT_DESKTOP WM_PART_DESKTOP
#define WM_HIT_DRAG WM_PART_DRAG
#define WM_HIT_CONTENT WM_PART_CONTENT
#define WM_HIT_CLOSE WM_PART_CLOSE
#define WM_HIT_GROW WM_PART_GROW

/* Hit test result (returned by WM_HitTest) */
typedef struct {
  WindowPart part;
  struct Window *window; /* Window hit (NULL for desktop/menubar) */
} HitTestResult;

/* ============================================================
 * Window Style / Definition ID
 * ============================================================ */
typedef enum {
  WM_STYLE_DOCUMENT = 0, /* Standard document window         */
                         /* Title bar, close box, drag       */
  WM_STYLE_DIALOG = 1,   /* Modal dialog (no close box)      */
  WM_STYLE_PLAIN = 2,    /* Plain rectangle (no title bar)   */
  WM_STYLE_SHADOW = 3,   /* Plain with drop shadow           */
  WM_STYLE_ALERT = 4     /* Alert box (bold border)          */
} WindowStyle;

/* ============================================================
 * Window Flags (bitfield)
 * ============================================================ */
#define WF_VISIBLE 0x01   /* Window is drawn on screen        */
#define WF_HILITED 0x02   /* Title bar is highlighted (active)*/
#define WF_HAS_CLOSE 0x04 /* Has a close box                  */
#define WF_HAS_GROW 0x08  /* Has a grow/resize box            */
#define WF_MODAL 0x10     /* Modal (blocks input to others)   */
#define WF_DIRTY 0x20     /* Needs full redraw                */

/* ============================================================
 * WindowRecord - The core window data structure
 *
 * Each window occupies ~128 bytes. At 16 max = 2KB total.
 * This is allocated from a static pool (no malloc needed).
 * ============================================================ */
typedef struct Window {
  /* Identity */
  uint8_t id;    /* Unique window ID (0-15)      */
  uint8_t style; /* WindowStyle enum             */
  uint8_t flags; /* WF_* bitfield                */
  uint8_t _pad0;

  /* Geometry (screen coordinates) */
  Rect frame;    /* Outer frame (includes border)*/
  Rect content;  /* Inner content area           */
  Rect titleBar; /* Title bar rect               */

  /* Title */
  char title[WM_TITLE_MAX + 1];

  /* Z-Order (doubly-linked list) */
  struct Window *above; /* Window above (closer to user)*/
  struct Window *below; /* Window below (further back)  */

  /* Application callback */
  uint32_t refCon; /* App-defined reference value  */
  void (*drawProc)(struct Window *win);
  /* Content draw callback        */
  void (*clickProc)(struct Window *win, Point where);
  /* Content click callback       */
  void (*dragProc)(struct Window *win, Point where);
  /* Content drag callback        */

  /* Dirty tracking */
  uint8_t dirtyCount; /* Number of dirty sub-rects    */
  uint8_t _pad1;
  Rect dirtyRects[4]; /* Per-window dirty rects       */
} Window;

/* ============================================================
 * DirtyRect - Global dirty rectangle for VDP transfer
 * ============================================================ */
typedef struct {
  Rect rect;
  uint8_t valid; /* 1 = needs transfer           */
  uint8_t _pad;
} DirtyRect;

/* ============================================================
 * WindowManager - Global state
 *
 * Single instance, lives in Sub CPU BSS.
 * ============================================================ */
typedef struct {
  /* Window pool (static allocation) */
  Window pool[WM_MAX_WINDOWS];
  uint8_t poolUsed[WM_MAX_WINDOWS]; /* 0=free, 1=in-use  */
  uint8_t windowCount;

  /* Z-order (linked list) */
  Window *topWindow;    /* Frontmost window             */
  Window *bottomWindow; /* Backmost window              */

  /* Active window (receives keyboard input) */
  Window *activeWindow;

  /* Global dirty rect accumulator */
  DirtyRect dirtyRects[WM_MAX_DIRTY_RECTS];
  uint8_t dirtyCount;

  /* Desktop state */
  uint8_t desktopPattern; /* 0=white, 1=gray, 2=checker  */
  uint8_t menuBarDirty;   /* Menu bar needs redraw        */

  /* Cursor */
  Point cursorPos;
  uint8_t cursorVisible;
} WindowManager;

/* ============================================================
 * Public API
 * ============================================================ */

/* Initialization */
void WM_Init(void);

/* Window lifecycle */
Window *WM_NewWindow(Rect *bounds, const char *title, WindowStyle style,
                     uint8_t flags);
void WM_DisposeWindow(Window *win);

/* Z-order */
void WM_SelectWindow(Window *win); /* Bring to front      */
void WM_SendToBack(Window *win);

/* Visibility */
void WM_ShowWindow(Window *win);
void WM_HideWindow(Window *win);

/* Geometry */
void WM_MoveWindow(Window *win, int16_t x, int16_t y);
void WM_SizeWindow(Window *win, int16_t w, int16_t h);
void WM_SetTitle(Window *win, const char *title);

/* Hit testing */
WindowPart WM_FindWindow(Point pt, Window **outWin);
HitTestResult WM_HitTest(Point pt); /* Convenience wrapper */

/* Dirty rect management */
void WM_InvalidateRect(Rect *r);          /* Mark screen region dirty */
void WM_InvalidateWindow(Window *win);    /* Mark entire window dirty */
void WM_ValidateRect(Rect *r);            /* Mark region as clean     */
#define WM_AddDirtyRect WM_InvalidateRect /* Alias for Sub CPU code */

/* Render cycle (called each frame by Sub CPU main loop) */
/* Returns number of dirty rects that need VDP transfer */
uint8_t WM_BeginUpdate(void);
DirtyRect *WM_GetDirtyRect(uint8_t index);
void WM_EndUpdate(void);

/* Desktop */
void WM_DrawDesktop(void);
void WM_SetDesktopPattern(uint8_t pattern);

/* Lookup & Traversal */
Window *WM_GetWindowById(uint8_t id);
Window *WM_GetTopWindow(void);    /* Frontmost window         */
Window *WM_GetBottomWindow(void); /* Backmost window          */
Window *WM_GetActiveWindow(void); /* Currently active window  */

/* Debug */
uint8_t WM_GetWindowCount(void);

#endif /* WM_H */
