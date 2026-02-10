/*
 * vkbd.h - Virtual Keyboard for Genesis System 1
 *
 * On-screen QWERTY keyboard for mouse-driven text input.
 * Renders in its own window. Sends characters to a callback.
 *
 * Layout mirrors a standard US keyboard:
 *   Row 0: 1234567890 (10 keys)
 *   Row 1: QWERTYUIOP (10 keys)
 *   Row 2: ASDFGHJKL  (9 keys)
 *   Row 3: Shift ZXCVBNM Bksp (9 keys)
 *   Row 4: Space bar (wide)
 */

#ifndef VKBD_H
#define VKBD_H

#include "wm.h"
#include <stdint.h>


/* Key dimensions */
#define VKBD_KEY_W 18    /* Standard key width         */
#define VKBD_KEY_H 16    /* Key height                 */
#define VKBD_KEY_PAD 2   /* Padding between keys       */
#define VKBD_ROWS 5      /* Number of key rows         */
#define VKBD_MAX_COLS 12 /* Max keys per row           */
#define VKBD_MARGIN 4    /* Window margin              */
#define VKBD_SPACE_W 120 /* Space bar width            */

/* Keyboard state */
typedef struct {
  uint8_t shifted;  /* Shift is active            */
  uint8_t capsLock; /* Caps lock toggled          */
  uint8_t _pad[2];
  /* Callback: receives typed character */
  void (*charCallback)(char ch);
} VKBDState;

/* Open the virtual keyboard window. Returns window pointer. */
Window *VKBD_Open(void);

/* Set the callback for character output */
void VKBD_SetCallback(void (*callback)(char ch));

/* Draw callback (registered as window's drawProc) */
void VKBD_Draw(Window *win);

/* Click callback (registered as window's clickProc) */
void VKBD_Click(Window *win, Point where);

/* Is the keyboard currently open? */
uint8_t VKBD_IsOpen(void);

#endif /* VKBD_H */
