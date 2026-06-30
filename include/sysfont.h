/*
 * sysfont.h - SegaOS system font
 *
 * Fixed-width 8x8 bitmap font covering ASCII 32-126 (95 glyphs).
 * The glyph bitmap data is format-converted from SGDK v2.11's MIT-licensed
 * font_default.png. Attribution and license details are recorded in
 * src/sub/sysfont.c and third_party/sgdk_font/.
 *
 * Each glyph is 8 pixels wide, 8 pixels tall.
 * Stored as 1 byte per row (MSB-first, all bits used).
 * Total: 95 glyphs * 8 bytes = 760 bytes.
 *
 * This font is compiled into ROM as const data; no file I/O needed.
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
