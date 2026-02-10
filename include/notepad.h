/*
 * notepad.h - Notepad App for Genesis System 1
 *
 * Simple text editor with a text buffer and cursor.
 * Uses the Virtual Keyboard for input.
 */

#ifndef NOTEPAD_H
#define NOTEPAD_H

#include "wm.h"
#include <stdint.h>


/* Text buffer limits */
#define NOTEPAD_MAX_CHARS 512 /* Maximum characters in buffer */
#define NOTEPAD_LINE_H 12     /* Line height in pixels        */
#define NOTEPAD_MARGIN 4      /* Content margin               */

/* Notepad state */
typedef struct {
  char text[NOTEPAD_MAX_CHARS + 1];
  uint16_t length;    /* Current text length          */
  uint16_t cursorPos; /* Cursor position in buffer    */
  int16_t scrollY;    /* Vertical scroll offset       */
  Window *window;     /* Our window                   */
} NotepadState;

/* Open a notepad window. Returns the window pointer. */
Window *Notepad_Open(void);

/* Draw callback */
void Notepad_Draw(Window *win);

/* Click callback */
void Notepad_Click(Window *win, Point where);

/* Handle a character from the virtual keyboard */
void Notepad_CharInput(char ch);

#endif /* NOTEPAD_H */
