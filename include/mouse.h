/*
 * mouse.h - Sega Mega Mouse Driver for Genesis System 1
 *
 * The Mega Mouse connects to a standard Genesis controller port
 * and communicates via TH/TR/TL handshake with 9-nibble data packets.
 *
 * This driver runs on the MAIN CPU, since the controller port
 * I/O registers are at $A10003 (port 1) and $A10005 (port 2).
 *
 * Protocol overview (per frame):
 *   1. Write $60 to data port (TH=1, TR=1) - request data
 *   2. Alternate $20/$00 writes, reading nibbles from D3-D0
 *   3. Read 9 nibbles total:
 *      Nibble 0: Mouse ID ($0 = valid mouse)
 *      Nibble 1: Overflow (bit 1=Y overflow, bit 0=X overflow)
 *      Nibble 2: Sign bits (bit 1=Y sign, bit 0=X sign), 1=negative
 *      Nibble 3: Buttons (bit 3=Start, 2=Middle, 1=Right, 0=Left)
 *      Nibble 4: X movement high nibble
 *      Nibble 5: X movement low nibble
 *      Nibble 6: Y movement high nibble
 *      Nibble 7: Y movement low nibble
 *      Nibble 8: Checksum (unused, read to complete transfer)
 *   4. Write $60 to end transfer
 *
 * Movement values are 8-bit signed (assembled from 2 nibbles + sign bit).
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

/* ============================================================
 * Controller Port I/O Registers (Main CPU only)
 * ============================================================ */
#define IO_DATA_1 (*(volatile uint8_t *)0xA10003) /* Port 1 data  */
#define IO_DATA_2 (*(volatile uint8_t *)0xA10005) /* Port 2 data  */
#define IO_CTRL_1 (*(volatile uint8_t *)0xA10009) /* Port 1 ctrl  */
#define IO_CTRL_2 (*(volatile uint8_t *)0xA1000B) /* Port 2 ctrl  */

/* Control register value: TH and TR as outputs, rest as inputs */
#define IO_CTRL_MOUSE 0x60 /* bits 6,5 = output (TH, TR)       */

/* ============================================================
 * Mouse Button Bits (from nibble 3)
 * ============================================================ */
#define MOUSE_BTN_LEFT 0x01
#define MOUSE_BTN_RIGHT 0x02
#define MOUSE_BTN_MIDDLE 0x04
#define MOUSE_BTN_START 0x08

/* ============================================================
 * Mouse State
 * ============================================================ */
typedef struct {
  /* Current absolute position (accumulated from deltas) */
  int16_t x;
  int16_t y;

  /* Raw per-frame delta (signed, from hardware) */
  int16_t dx;
  int16_t dy;

  /* Button state (bitmask of MOUSE_BTN_*) */
  uint8_t buttons;

  /* Previous button state (for edge detection) */
  uint8_t prevButtons;

  /* Status flags */
  uint8_t connected; /* 1 = mouse detected               */
  uint8_t overflow;  /* 1 = movement overflow this frame  */
} MouseState;

/* ============================================================
 * Public API
 * ============================================================ */

/* Initialize mouse driver for specified port (1 or 2) */
void Mouse_Init(uint8_t port);

/* Poll the mouse hardware. Call once per VBlank. */
/* Returns 1 if mouse is connected and data is valid. */
uint8_t Mouse_Poll(void);

/* Get current mouse state */
const MouseState *Mouse_GetState(void);

/* Button edge detection helpers */
/* Returns 1 if button was just pressed this frame */
uint8_t Mouse_ButtonPressed(uint8_t btn);

/* Returns 1 if button was just released this frame */
uint8_t Mouse_ButtonReleased(uint8_t btn);

/* Set mouse position (e.g., center on screen) */
void Mouse_SetPosition(int16_t x, int16_t y);

/* Set movement bounds (screen clipping) */
void Mouse_SetBounds(int16_t minX, int16_t minY, int16_t maxX, int16_t maxY);

#endif /* MOUSE_H */
