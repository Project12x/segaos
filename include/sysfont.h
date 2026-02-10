/*
 * sysfont.h - System Font for Genesis System 1
 *
 * Fixed-width 6x10 bitmap font covering ASCII 32-126 (95 glyphs).
 * Designed in the style of early Macintosh system fonts (Chicago/Geneva).
 *
 * Each glyph is 6 pixels wide, 10 pixels tall.
 * Stored as 1 byte per row (MSB-first, only bits 7-2 used).
 * Total: 95 glyphs * 10 bytes = 950 bytes.
 *
 * This font is compiled into ROM as const data — no file I/O needed.
 */

#ifndef SYSFONT_H
#define SYSFONT_H

#include "blitter.h"

/* The system font instance (defined in sysfont.c) */
extern const Font systemFont;

/* Convenience: draw string using system font */
static inline int16_t SysFont_DrawString(int16_t x, int16_t y, const char *str,
                                         uint8_t color) {
  return BLT_DrawString(x, y, str, &systemFont, color);
}

/* Convenience: measure string width using system font */
static inline int16_t SysFont_StringWidth(const char *str) {
  return BLT_StringWidth(str, &systemFont);
}

/* Get the font for passing to blitter functions */
static inline const Font *SysFont_Get(void) { return &systemFont; }

#endif /* SYSFONT_H */
