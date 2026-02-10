/*
 * calc.h - Calculator App for Genesis System 1
 *
 * Simple 4-function calculator with a button grid.
 * Designed for mouse-only interaction.
 *
 * Layout (inside a WM_STYLE_DOCUMENT window):
 *   - 7-segment style display at top (shows current number)
 *   - 4x5 button grid: digits 0-9, +, -, *, /, =, C, +/-
 *   - Each button is 32x20 pixels
 */

#ifndef CALC_H
#define CALC_H

#include "wm.h"
#include <stdint.h>


/* Button grid dimensions */
#define CALC_COLS 4
#define CALC_ROWS 5
#define CALC_BTN_W 32
#define CALC_BTN_H 20
#define CALC_BTN_PAD 2    /* Padding between buttons    */
#define CALC_DISPLAY_H 24 /* Display area height        */
#define CALC_MARGIN 4     /* Margin inside window       */

/* Total content size */
#define CALC_CONTENT_W                                                         \
  (CALC_COLS * (CALC_BTN_W + CALC_BTN_PAD) + CALC_MARGIN * 2)
#define CALC_CONTENT_H                                                         \
  (CALC_DISPLAY_H + CALC_ROWS * (CALC_BTN_H + CALC_BTN_PAD) + CALC_MARGIN * 3)

/* Calculator state */
typedef struct {
  int32_t display;     /* Current displayed value    */
  int32_t accumulator; /* Stored operand             */
  uint8_t pendingOp;   /* Pending operator (0=none)  */
  uint8_t clearOnNext; /* Clear display on next digit*/
  uint8_t hasDecimal;  /* Unused (integer only)      */
  uint8_t _pad;
} CalcState;

/* Open a calculator window. Returns the window pointer. */
Window *Calc_Open(void);

/* Draw callback (registered as window's drawProc) */
void Calc_Draw(Window *win);

/* Click callback (registered as window's clickProc) */
void Calc_Click(Window *win, Point where);

#endif /* CALC_H */
