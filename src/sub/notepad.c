/*
 * notepad.c - Notepad App Implementation
 *
 * Simple text editor that displays a text buffer with a
 * blinking cursor. Receives input from the Virtual Keyboard.
 */

#include "notepad.h"
#include "blitter.h"
#include "sysfont.h"
#include "vkbd.h"
#include "wm.h"

#include <string.h>

/* ============================================================
 * Static State (one notepad instance)
 * ============================================================ */
static NotepadState noteState;

/* ============================================================
 * Internal: Word-wrap aware rendering
 * ============================================================ */

/* Calculate how many characters fit on one line */
static uint8_t chars_per_line(Window *win) {
  int16_t contentW =
      win->content.right - win->content.left - NOTEPAD_MARGIN * 2;
  const Font *f = SysFont_Get();
  if (f && f->glyphs && f->glyphs[0].advance > 0) {
    return (uint8_t)(contentW / f->glyphs[0].advance);
  }
  return (uint8_t)(contentW / 6); /* Fallback */
}

/* ============================================================
 * Window Callbacks
 * ============================================================ */

void Notepad_Draw(Window *win) {
  int16_t cx = win->content.left + NOTEPAD_MARGIN;
  int16_t cy = win->content.top + NOTEPAD_MARGIN;
  int16_t maxY = win->content.bottom - NOTEPAD_MARGIN;
  uint8_t cpl = chars_per_line(win);
  uint16_t i;
  int16_t lineY = cy - noteState.scrollY;
  uint8_t col = 0;
  const Font *f = SysFont_Get();
  uint8_t glyphW =
      (f && f->glyphs && f->glyphs[0].advance > 0) ? f->glyphs[0].advance : 6;

  /* White background for content area */
  Rect contentRect;
  contentRect.left = win->content.left;
  contentRect.top = win->content.top;
  contentRect.right = win->content.right;
  contentRect.bottom = win->content.bottom;
  BLT_FillRect(&contentRect, 0);

  /* Draw text character by character, wrapping at line width */
  for (i = 0; i <= noteState.length; i++) {
    /* Draw cursor at cursor position */
    if (i == noteState.cursorPos && lineY >= cy &&
        lineY + NOTEPAD_LINE_H <= maxY) {
      /* Draw a thin vertical line for cursor */
      int16_t curX = cx + col * glyphW;
      BLT_DrawVLine(curX, lineY, lineY + NOTEPAD_LINE_H - 2, 1);
    }

    if (i >= noteState.length)
      break;

    char ch = noteState.text[i];

    /* Newline: move to next line */
    if (ch == '\n') {
      col = 0;
      lineY += NOTEPAD_LINE_H;
      if (lineY > maxY)
        break; /* Below visible area */
      continue;
    }

    /* Line wrap at edge */
    if (col >= cpl) {
      col = 0;
      lineY += NOTEPAD_LINE_H;
      if (lineY > maxY)
        break;
    }

    /* Draw character if visible */
    if (lineY >= cy && lineY + NOTEPAD_LINE_H <= maxY) {
      char str[2];
      str[0] = ch;
      str[1] = '\0';
      SysFont_DrawString(cx + col * glyphW, lineY, str, 1);
    }

    col++;
  }
}

void Notepad_Click(Window *win, Point where) {
  /* Calculate click position in text */
  int16_t cx = win->content.left + NOTEPAD_MARGIN;
  int16_t cy = win->content.top + NOTEPAD_MARGIN;
  uint8_t cpl = chars_per_line(win);
  const Font *f = SysFont_Get();
  uint8_t glyphW =
      (f && f->glyphs && f->glyphs[0].advance > 0) ? f->glyphs[0].advance : 6;

  /* Determine clicked row and column */
  int16_t relY = where.y - cy + noteState.scrollY;
  int16_t relX = where.x - cx;
  if (relX < 0)
    relX = 0;

  uint16_t clickRow = (uint16_t)(relY / NOTEPAD_LINE_H);
  uint8_t clickCol = (uint8_t)(relX / glyphW);

  /* Walk through text to find the character at that position */
  uint16_t row = 0;
  uint8_t col = 0;
  uint16_t i;

  for (i = 0; i < noteState.length; i++) {
    if (row == clickRow && col >= clickCol) {
      noteState.cursorPos = i;
      WM_InvalidateWindow(win);
      return;
    }

    if (noteState.text[i] == '\n') {
      if (row == clickRow) {
        /* Clicked past end of this line */
        noteState.cursorPos = i;
        WM_InvalidateWindow(win);
        return;
      }
      row++;
      col = 0;
    } else {
      col++;
      if (col >= cpl) {
        if (row == clickRow) {
          noteState.cursorPos = i + 1;
          WM_InvalidateWindow(win);
          return;
        }
        row++;
        col = 0;
      }
    }
  }

  /* Clicked at or past end of text */
  noteState.cursorPos = noteState.length;
  WM_InvalidateWindow(win);
}

/* ============================================================
 * Character Input (from Virtual Keyboard)
 * ============================================================ */

void Notepad_CharInput(char ch) {
  if (!noteState.window)
    return;

  if (ch == '\b') {
    /* Backspace: delete character before cursor */
    if (noteState.cursorPos > 0) {
      uint16_t i;
      noteState.cursorPos--;
      for (i = noteState.cursorPos; i < noteState.length - 1; i++) {
        noteState.text[i] = noteState.text[i + 1];
      }
      noteState.length--;
      noteState.text[noteState.length] = '\0';
    }
  } else if (noteState.length < NOTEPAD_MAX_CHARS) {
    /* Insert character at cursor position */
    uint16_t i;
    for (i = noteState.length; i > noteState.cursorPos; i--) {
      noteState.text[i] = noteState.text[i - 1];
    }
    noteState.text[noteState.cursorPos] = ch;
    noteState.cursorPos++;
    noteState.length++;
    noteState.text[noteState.length] = '\0';
  }

  WM_InvalidateWindow(noteState.window);
}

/* ============================================================
 * Public API
 * ============================================================ */

Window *Notepad_Open(void) {
  Rect bounds;
  Window *win;

  memset(&noteState, 0, sizeof(NotepadState));

  bounds.left = 10;
  bounds.top = 24;
  bounds.right = 240;
  bounds.bottom = 120;

  win = WM_NewWindow(&bounds, "Notepad", WM_STYLE_DOCUMENT,
                     WF_VISIBLE | WF_HAS_CLOSE | WF_HAS_GROW);

  if (win) {
    win->drawProc = Notepad_Draw;
    win->clickProc = Notepad_Click;
    noteState.window = win;

    /* Open virtual keyboard and route to us */
    VKBD_Open();
    VKBD_SetCallback(Notepad_CharInput);
  }

  return win;
}
