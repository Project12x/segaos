/*
 * input.h - Input Event System for Genesis System 1
 *
 * Bridges Main CPU input (mouse, joypad) to Sub CPU (Window Manager).
 *
 * Architecture:
 *   Main CPU: Polls hardware each VBlank, packs events into CMD registers.
 *   Sub CPU:  Reads CMD registers, dispatches to Window Manager.
 *
 * Mouse event encoding in CMD registers:
 *   CMD[0] ($A12010): X position (absolute, 0-319)
 *   CMD[1] ($A12012): Y position (absolute, 0-223)
 *   CMD[2] ($A12014): Buttons (low byte) | Event type (high byte)
 *   CMD[3] ($A12016): Delta X (signed, for drag tracking)
 *
 * The Main CPU sends CMD_MOUSE_EVENT via the comm flag, then the
 * Sub CPU reads the packed data from the CMD registers.
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>

/* ============================================================
 * Event Types (packed into high byte of CMD[2])
 * ============================================================ */
#define INPUT_EVT_NONE 0x00
#define INPUT_EVT_MOUSE_MOVE 0x01 /* Mouse moved                  */
#define INPUT_EVT_MOUSE_DOWN 0x02 /* Button pressed               */
#define INPUT_EVT_MOUSE_UP 0x03   /* Button released              */
#define INPUT_EVT_MOUSE_DRAG 0x04 /* Mouse moved with button held */

/* ============================================================
 * Input Event (decoded on Sub CPU side)
 * ============================================================ */
typedef struct {
  uint8_t type;    /* INPUT_EVT_* */
  uint8_t buttons; /* MOUSE_BTN_* bitmask */
  int16_t x;       /* Absolute screen X */
  int16_t y;       /* Absolute screen Y */
  int16_t dx;      /* Delta X (for drag) */
  int16_t dy;      /* Delta Y (for drag) */
} InputEvent;

/* ============================================================
 * Main CPU Side - Packing (runs in VBlank handler)
 * ============================================================ */
#ifdef MAIN_CPU

#include "common.h"
#include "ga_regs.h"
#include "mouse.h"


/* Call from Main CPU VBlank: polls mouse, sends event to Sub CPU.
 * Only sends an event if something changed (move, button press/release).
 */
static inline void Input_SendMouseEvent(void) {
  const MouseState *ms = Mouse_GetState();
  uint8_t evtType = INPUT_EVT_NONE;

  if (!ms->connected)
    return;

  /* Determine event type */
  if (ms->buttons != ms->prevButtons) {
    /* Button state changed */
    uint8_t newlyPressed = ms->buttons & ~ms->prevButtons;
    if (newlyPressed) {
      evtType = INPUT_EVT_MOUSE_DOWN;
    } else {
      evtType = INPUT_EVT_MOUSE_UP;
    }
  } else if (ms->dx != 0 || ms->dy != 0) {
    /* Mouse moved */
    if (ms->buttons) {
      evtType = INPUT_EVT_MOUSE_DRAG;
    } else {
      evtType = INPUT_EVT_MOUSE_MOVE;
    }
  }

  if (evtType == INPUT_EVT_NONE)
    return;

  /* Pack into CMD registers */
  main_send_param(0, (uint16_t)ms->x);        /* CMD[0]: X pos    */
  main_send_param(1, (uint16_t)ms->y);        /* CMD[1]: Y pos    */
  main_send_param(2, ((uint16_t)evtType << 8) /* CMD[2]: type|btn */
                         | (uint16_t)ms->buttons);
  main_send_param(3, (uint16_t)ms->dx); /* CMD[3]: delta X  */

  /* Send the command */
  GA_MAIN_SET_FLAG(CMD_MOUSE_EVENT);
}

#endif /* MAIN_CPU */

/* ============================================================
 * Sub CPU Side - Unpacking (runs in command loop)
 * ============================================================ */
#ifdef SUB_CPU

#include "common.h"
#include "ga_regs.h"


/* Decode a mouse event from CMD registers into an InputEvent struct. */
static inline void Input_DecodeMouseEvent(InputEvent *evt) {
  uint16_t posX = sub_read_param(0);
  uint16_t posY = sub_read_param(1);
  uint16_t packed = sub_read_param(2);
  uint16_t deltaX = sub_read_param(3);

  evt->x = (int16_t)posX;
  evt->y = (int16_t)posY;
  evt->type = (uint8_t)(packed >> 8);
  evt->buttons = (uint8_t)(packed & 0xFF);
  evt->dx = (int16_t)deltaX;
  evt->dy = 0; /* Could add CMD[4] for delta Y if needed */
}

#endif /* SUB_CPU */

#endif /* INPUT_H */
