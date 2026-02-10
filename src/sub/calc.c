/*
 * calc.c - Calculator App Implementation
 *
 * 4-function integer calculator with mouse-driven button grid.
 * Renders inside a WM window using the blitter API.
 */

#include "calc.h"
#include "blitter.h"
#include "sysfont.h"
#include "wm.h"

#include <string.h>

/* ============================================================
 * Static State (one calculator instance)
 * ============================================================ */
static CalcState calcState;

/* Button labels (row-major, top to bottom) */
/* Row 0: C  +/-  (empty)  /     */
/* Row 1: 7   8    9       *     */
/* Row 2: 4   5    6       -     */
/* Row 3: 1   2    3       +     */
/* Row 4: 0   (wide 0)     =     */
static const char *btnLabels[CALC_ROWS][CALC_COLS] = {
    {"C", "+/-", "", "/"}, {"7", "8", "9", "*"}, {"4", "5", "6", "-"},
    {"1", "2", "3", "+"},  {"0", "", "", "="},
};

/* Operator codes */
#define OP_NONE 0
#define OP_ADD 1
#define OP_SUB 2
#define OP_MUL 3
#define OP_DIV 4

/* ============================================================
 * Internal: Number to string (right-aligned)
 * ============================================================ */
static void int_to_str(int32_t val, char *buf, uint8_t bufLen) {
  uint8_t neg = 0;
  uint8_t i = bufLen - 1;
  uint32_t uval;

  buf[bufLen] = '\0';
  memset(buf, ' ', bufLen);

  if (val < 0) {
    neg = 1;
    uval = (uint32_t)(-(val + 1)) + 1; /* Handle INT32_MIN */
  } else {
    uval = (uint32_t)val;
  }

  /* Write digits right-to-left */
  do {
    buf[i--] = '0' + (char)(uval % 10);
    uval /= 10;
  } while (uval > 0 && i > 0);

  if (neg && i > 0) {
    buf[i] = '-';
  }
}

/* ============================================================
 * Internal: Execute pending operation
 * ============================================================ */
static void calc_execute(void) {
  switch (calcState.pendingOp) {
  case OP_ADD:
    calcState.accumulator += calcState.display;
    break;
  case OP_SUB:
    calcState.accumulator -= calcState.display;
    break;
  case OP_MUL:
    calcState.accumulator *= calcState.display;
    break;
  case OP_DIV:
    if (calcState.display != 0) {
      calcState.accumulator /= calcState.display;
    }
    /* Division by zero: keep accumulator unchanged */
    break;
  default:
    calcState.accumulator = calcState.display;
    break;
  }
  calcState.display = calcState.accumulator;
  calcState.pendingOp = OP_NONE;
  calcState.clearOnNext = 1;
}

/* ============================================================
 * Internal: Process button press
 * ============================================================ */
static void calc_press_button(uint8_t row, uint8_t col) {
  /* Digit buttons (rows 1-4, cols 0-2, except row 4 cols 1-2) */
  if (row >= 1 && row <= 4 && col <= 2) {
    /* Row 4 col 1,2 are part of wide "0" button */
    uint8_t digit;
    if (row == 4) {
      digit = 0;
    } else {
      /* Row 1: 7,8,9  Row 2: 4,5,6  Row 3: 1,2,3 */
      digit = (uint8_t)((4 - row) * 3 + col + 1);
      if (digit > 9)
        digit = 9;
    }

    if (calcState.clearOnNext) {
      calcState.display = 0;
      calcState.clearOnNext = 0;
    }

    /* Prevent overflow (10 digit max) */
    if (calcState.display < 999999999 && calcState.display > -999999999) {
      if (calcState.display >= 0) {
        calcState.display = calcState.display * 10 + digit;
      } else {
        calcState.display = calcState.display * 10 - digit;
      }
    }
    return;
  }

  /* Row 0, Col 0: C (Clear) */
  if (row == 0 && col == 0) {
    calcState.display = 0;
    calcState.accumulator = 0;
    calcState.pendingOp = OP_NONE;
    calcState.clearOnNext = 0;
    return;
  }

  /* Row 0, Col 1: +/- (Negate) */
  if (row == 0 && col == 1) {
    calcState.display = -calcState.display;
    return;
  }

  /* Operator buttons (col 3, rows 0-3) */
  if (col == 3 && row <= 3) {
    if (calcState.pendingOp != OP_NONE && !calcState.clearOnNext) {
      calc_execute();
    } else {
      calcState.accumulator = calcState.display;
    }
    switch (row) {
    case 0:
      calcState.pendingOp = OP_DIV;
      break;
    case 1:
      calcState.pendingOp = OP_MUL;
      break;
    case 2:
      calcState.pendingOp = OP_SUB;
      break;
    case 3:
      calcState.pendingOp = OP_ADD;
      break;
    }
    calcState.clearOnNext = 1;
    return;
  }

  /* Row 4, Col 3: = (Equals) */
  if (row == 4 && col == 3) {
    if (calcState.pendingOp != OP_NONE) {
      calc_execute();
    }
    return;
  }
}

/* ============================================================
 * Window Callbacks
 * ============================================================ */

void Calc_Draw(Window *win) {
  int16_t cx = win->content.left;
  int16_t cy = win->content.top;
  uint8_t r, c;
  char displayBuf[12];

  /* Display area: black background with white text */
  Rect displayRect;
  displayRect.left = cx + CALC_MARGIN;
  displayRect.top = cy + CALC_MARGIN;
  displayRect.right =
      cx + CALC_MARGIN + CALC_COLS * (CALC_BTN_W + CALC_BTN_PAD);
  displayRect.bottom = cy + CALC_MARGIN + CALC_DISPLAY_H;
  BLT_FillRect(&displayRect, 1); /* Black fill */

  /* Draw number right-aligned in display */
  int_to_str(calcState.display, displayBuf, 11);
  SysFont_DrawString(displayRect.right - SysFont_StringWidth(displayBuf) - 4,
                     displayRect.top + 7, displayBuf,
                     0 /* White text on black */
  );

  /* Button grid */
  int16_t gridY = cy + CALC_MARGIN * 2 + CALC_DISPLAY_H;

  for (r = 0; r < CALC_ROWS; r++) {
    for (c = 0; c < CALC_COLS; c++) {
      const char *label = btnLabels[r][c];

      /* Skip empty cells (part of wide "0" button) */
      if (r == 4 && (c == 1 || c == 2))
        continue;
      if (r == 0 && c == 2)
        continue;

      int16_t bx = cx + CALC_MARGIN + c * (CALC_BTN_W + CALC_BTN_PAD);
      int16_t by = gridY + r * (CALC_BTN_H + CALC_BTN_PAD);
      int16_t bw = CALC_BTN_W;

      /* Wide "0" button spans 3 columns */
      if (r == 4 && c == 0) {
        bw = CALC_BTN_W * 3 + CALC_BTN_PAD * 2;
      }

      /* Draw button: white fill with black border */
      Rect btnRect;
      btnRect.left = bx;
      btnRect.top = by;
      btnRect.right = bx + bw;
      btnRect.bottom = by + CALC_BTN_H;
      BLT_FillRect(&btnRect, 0); /* White fill */

      /* Border */
      BLT_DrawHLine(bx, bx + bw - 1, by, 1);
      BLT_DrawHLine(bx, bx + bw - 1, by + CALC_BTN_H - 1, 1);
      BLT_DrawVLine(bx, by, by + CALC_BTN_H - 1, 1);
      BLT_DrawVLine(bx + bw - 1, by, by + CALC_BTN_H - 1, 1);

      /* Center label text */
      if (label && label[0]) {
        int16_t tw = SysFont_StringWidth(label);
        SysFont_DrawString(bx + (bw - tw) / 2, by + 5, label, 1 /* Black text */
        );
      }
    }
  }
}

void Calc_Click(Window *win, Point where) {
  int16_t cx = win->content.left + CALC_MARGIN;
  int16_t gridY = win->content.top + CALC_MARGIN * 2 + CALC_DISPLAY_H;
  uint8_t r, c;

  /* Check if click is in button grid area */
  if (where.y < gridY)
    return;

  /* Determine which button was clicked */
  r = (uint8_t)((where.y - gridY) / (CALC_BTN_H + CALC_BTN_PAD));
  c = (uint8_t)((where.x - cx) / (CALC_BTN_W + CALC_BTN_PAD));

  if (r >= CALC_ROWS || c >= CALC_COLS)
    return;

  /* Wide "0" button: map cols 1,2 to col 0 for row 4 */
  if (r == 4 && c <= 2)
    c = 0;

  /* Skip empty button */
  if (r == 0 && c == 2)
    return;

  calc_press_button(r, c);
  WM_InvalidateWindow(win);
}

/* ============================================================
 * Public API
 * ============================================================ */

Window *Calc_Open(void) {
  Rect bounds;
  Window *win;

  memset(&calcState, 0, sizeof(CalcState));

  bounds.left = 60;
  bounds.top = 40;
  bounds.right = bounds.left + CALC_CONTENT_W + 2;  /* +2 for border */
  bounds.bottom = bounds.top + CALC_CONTENT_H + 22; /* +22 for title */

  win = WM_NewWindow(&bounds, "Calculator", WM_STYLE_DOCUMENT,
                     WF_VISIBLE | WF_HAS_CLOSE);

  if (win) {
    win->drawProc = Calc_Draw;
    win->clickProc = Calc_Click;
  }

  return win;
}
