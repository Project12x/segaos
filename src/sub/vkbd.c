/*
 * vkbd.c - Virtual Keyboard Implementation
 *
 * On-screen QWERTY keyboard with Shift, Caps Lock, Backspace,
 * and a wide space bar. Sends characters via callback.
 */

#include "vkbd.h"
#include "blitter.h"
#include "sysfont.h"
#include "wm.h"

#include <string.h>

/* ============================================================
 * Static State
 * ============================================================ */
static VKBDState vkbdState;
static Window *vkbdWindow = (Window *)0;

/* Key definitions: each row has its own length */
/* Lowercase keys */
static const char row0_lower[] = "1234567890";
static const char row1_lower[] = "qwertyuiop";
static const char row2_lower[] = "asdfghjkl";
static const char row3_lower[] = "zxcvbnm";

/* Uppercase keys */
static const char row0_upper[] = "!@#$%^&*()";
static const char row1_upper[] = "QWERTYUIOP";
static const char row2_upper[] = "ASDFGHJKL";
static const char row3_upper[] = "ZXCVBNM";

/* Row key counts */
static const uint8_t rowLengths[VKBD_ROWS] = {10, 10, 9, 7, 1};

/* Special key codes - use low control codes that fit in signed char */
#define VKBD_KEY_SHIFT 0x01
#define VKBD_KEY_BKSP 0x02
#define VKBD_KEY_CAPS 0x03
#define VKBD_KEY_SPACE ' '

/* ============================================================
 * Internal: Get character for a key position
 * ============================================================ */
static char get_key_char(uint8_t row, uint8_t col) {
  uint8_t useUpper = vkbdState.shifted || vkbdState.capsLock;
  const char *lower, *upper;

  switch (row) {
  case 0:
    lower = row0_lower;
    upper = row0_upper;
    if (col < 10)
      return useUpper ? upper[col] : lower[col];
    break;
  case 1:
    lower = row1_lower;
    upper = row1_upper;
    if (col < 10)
      return useUpper ? upper[col] : lower[col];
    break;
  case 2:
    if (col == 0)
      return VKBD_KEY_CAPS; /* Caps Lock on left */
    if (col <= 9) {
      lower = row2_lower;
      upper = row2_upper;
      return useUpper ? upper[col - 1] : lower[col - 1];
    }
    break;
  case 3:
    if (col == 0)
      return VKBD_KEY_SHIFT;
    if (col <= 7) {
      lower = row3_lower;
      upper = row3_upper;
      return useUpper ? upper[col - 1] : lower[col - 1];
    }
    if (col == 8)
      return VKBD_KEY_BKSP;
    break;
  case 4:
    return VKBD_KEY_SPACE;
  }
  return 0;
}

/* Get display label for a key */
static const char *get_key_label(uint8_t row, uint8_t col, char *buf) {
  char ch = get_key_char(row, col);
  if (ch == VKBD_KEY_SHIFT)
    return "Shft";
  if (ch == VKBD_KEY_BKSP)
    return "Bksp";
  if (ch == VKBD_KEY_CAPS)
    return "Cap";
  if (ch == VKBD_KEY_SPACE)
    return "";
  if (ch == 0)
    return "";
  buf[0] = ch;
  buf[1] = '\0';
  return buf;
}

/* Get number of keys in a row (including special keys) */
static uint8_t get_row_key_count(uint8_t row) {
  switch (row) {
  case 0:
    return 10;
  case 1:
    return 10;
  case 2:
    return 10; /* Caps + 9 letters */
  case 3:
    return 9; /* Shift + 7 letters + Bksp */
  case 4:
    return 1; /* Space */
  }
  return 0;
}

/* ============================================================
 * Window Callbacks
 * ============================================================ */

void VKBD_Draw(Window *win) {
  int16_t cx = win->content.left + VKBD_MARGIN;
  int16_t cy = win->content.top + VKBD_MARGIN;
  uint8_t r, c;
  char labelBuf[2];

  for (r = 0; r < VKBD_ROWS; r++) {
    uint8_t keyCount = get_row_key_count(r);
    /* Center each row horizontally */
    int16_t rowWidth = keyCount * (VKBD_KEY_W + VKBD_KEY_PAD) - VKBD_KEY_PAD;
    int16_t startX = cx;

    /* Row 2,3 are indented slightly like a real keyboard */
    if (r == 2)
      startX += VKBD_KEY_W / 4;
    if (r == 3)
      startX += VKBD_KEY_W / 2;

    for (c = 0; c < keyCount; c++) {
      int16_t kx = startX + c * (VKBD_KEY_W + VKBD_KEY_PAD);
      int16_t ky = cy + r * (VKBD_KEY_H + VKBD_KEY_PAD);
      int16_t kw = VKBD_KEY_W;

      /* Space bar is extra wide */
      if (r == 4 && c == 0) {
        kw = VKBD_SPACE_W;
        kx = cx + (10 * (VKBD_KEY_W + VKBD_KEY_PAD) - VKBD_SPACE_W) / 2;
      }

      /* Backspace wider */
      if (r == 3 && c == 8) {
        kw = VKBD_KEY_W + 6;
      }

      /* Draw key */
      Rect keyRect;
      keyRect.left = kx;
      keyRect.top = ky;
      keyRect.right = kx + kw;
      keyRect.bottom = ky + VKBD_KEY_H;

      /* Highlight active modifier keys */
      uint8_t inverted = 0;
      char ch = get_key_char(r, c);
      if (ch == VKBD_KEY_SHIFT && vkbdState.shifted)
        inverted = 1;
      if (ch == VKBD_KEY_CAPS && vkbdState.capsLock)
        inverted = 1;

      BLT_FillRect(&keyRect, inverted ? 1 : 0);

      /* Border */
      BLT_DrawHLine(kx, kx + kw - 1, ky, 1);
      BLT_DrawHLine(kx, kx + kw - 1, ky + VKBD_KEY_H - 1, 1);
      BLT_DrawVLine(kx, ky, ky + VKBD_KEY_H - 1, 1);
      BLT_DrawVLine(kx + kw - 1, ky, ky + VKBD_KEY_H - 1, 1);

      /* Label */
      const char *label = get_key_label(r, c, labelBuf);
      if (label[0]) {
        int16_t tw = SysFont_StringWidth(label);
        SysFont_DrawString(kx + (kw - tw) / 2, ky + 3, label, inverted ? 0 : 1);
      }
    }
  }
}

void VKBD_Click(Window *win, Point where) {
  int16_t cx = win->content.left + VKBD_MARGIN;
  int16_t cy = win->content.top + VKBD_MARGIN;
  uint8_t r, c;

  /* Determine which row */
  r = (uint8_t)((where.y - cy) / (VKBD_KEY_H + VKBD_KEY_PAD));
  if (r >= VKBD_ROWS)
    return;

  /* Determine column with row offset */
  int16_t startX = cx;
  if (r == 2)
    startX += VKBD_KEY_W / 4;
  if (r == 3)
    startX += VKBD_KEY_W / 2;

  /* Space bar: special handling */
  if (r == 4) {
    int16_t spaceX = cx + (10 * (VKBD_KEY_W + VKBD_KEY_PAD) - VKBD_SPACE_W) / 2;
    if (where.x >= spaceX && where.x < spaceX + VKBD_SPACE_W) {
      if (vkbdState.charCallback) {
        vkbdState.charCallback(' ');
      }
    }
    return;
  }

  c = (uint8_t)((where.x - startX) / (VKBD_KEY_W + VKBD_KEY_PAD));

  uint8_t keyCount = get_row_key_count(r);
  if (c >= keyCount)
    return;

  char ch = get_key_char(r, c);
  if (ch == 0)
    return;

  if (ch == VKBD_KEY_SHIFT) {
    vkbdState.shifted = !vkbdState.shifted;
    WM_InvalidateWindow(win);
    return;
  }

  if (ch == VKBD_KEY_CAPS) {
    vkbdState.capsLock = !vkbdState.capsLock;
    WM_InvalidateWindow(win);
    return;
  }

  if (ch == VKBD_KEY_BKSP) {
    if (vkbdState.charCallback) {
      vkbdState.charCallback('\b');
    }
    /* Auto-release shift after backspace */
    if (vkbdState.shifted) {
      vkbdState.shifted = 0;
      WM_InvalidateWindow(win);
    }
    return;
  }

  /* Normal character */
  if (vkbdState.charCallback) {
    vkbdState.charCallback(ch);
  }

  /* Auto-release shift (not caps lock) after typing */
  if (vkbdState.shifted) {
    vkbdState.shifted = 0;
    WM_InvalidateWindow(win);
  }
}

/* ============================================================
 * Public API
 * ============================================================ */

Window *VKBD_Open(void) {
  Rect bounds;

  if (vkbdWindow)
    return vkbdWindow;

  memset(&vkbdState, 0, sizeof(VKBDState));

  /* Calculate window size from key grid */
  int16_t contentW = 10 * (VKBD_KEY_W + VKBD_KEY_PAD) + VKBD_MARGIN * 2;
  int16_t contentH = VKBD_ROWS * (VKBD_KEY_H + VKBD_KEY_PAD) + VKBD_MARGIN * 2;

  bounds.left = 20;
  bounds.top = 120; /* Bottom half of screen */
  bounds.right = bounds.left + contentW + 2;
  bounds.bottom = bounds.top + contentH + 22;

  vkbdWindow = WM_NewWindow(&bounds, "Keyboard", WM_STYLE_DOCUMENT,
                            WF_VISIBLE | WF_HAS_CLOSE);

  if (vkbdWindow) {
    vkbdWindow->drawProc = VKBD_Draw;
    vkbdWindow->clickProc = VKBD_Click;
  }

  return vkbdWindow;
}

void VKBD_SetCallback(void (*callback)(char ch)) {
  vkbdState.charCallback = callback;
}

uint8_t VKBD_IsOpen(void) { return vkbdWindow != (Window *)0; }
