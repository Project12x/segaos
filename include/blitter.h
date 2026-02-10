/*
 * blitter.h - Multi-Mode Bitmap Blitter for Genesis System 1
 *
 * All rendering targets a linear framebuffer in Word RAM.
 * 320x224 resolution, supports 2bpp and 4bpp modes.
 *
 * 2-bit mode (4 grayscale):
 *   - 2 bits per pixel, 4 pixels per byte, MSB-first
 *   - 80 bytes per row, 17,920 bytes per frame
 *   - Palette: Black, Dark Gray, Light Gray, White
 *
 * 4-bit mode (16 colors, Win 3.1 palette):
 *   - 4 bits per pixel, 2 pixels per byte (high nibble = left)
 *   - 160 bytes per row, 35,840 bytes per frame
 *   - Palette: Standard Windows 3.1 16-color palette
 *
 * The blitter runs on the Sub CPU (68000 @ 12.5MHz).
 */

#ifndef BLITTER_H
#define BLITTER_H

#include "sega_os.h"

/* ============================================================
 * Configuration
 * ============================================================ */
#define BLT_SCREEN_W 320
#define BLT_SCREEN_H 224

/* Per-mode constants */
#define BLT_BPP_2BIT 2
#define BLT_BPP_4BIT 4
#define BLT_BYTES_PER_ROW_2 (BLT_SCREEN_W * 2 / 8)               /* 80 bytes  */
#define BLT_BYTES_PER_ROW_4 (BLT_SCREEN_W * 4 / 8)               /* 160 bytes */
#define BLT_FRAMEBUF_SIZE_2 (BLT_BYTES_PER_ROW_2 * BLT_SCREEN_H) /* 17920 */
#define BLT_FRAMEBUF_SIZE_4 (BLT_BYTES_PER_ROW_4 * BLT_SCREEN_H) /* 35840 */

/* Legacy compat (resolves at runtime via current mode) */
#define BLT_BYTES_PER_ROW BLT_GetBytesPerRow()
#define BLT_FRAMEBUF_SIZE BLT_GetFramebufSize()

/* ============================================================
 * Video Modes
 * ============================================================ */
typedef enum {
  BLT_MODE_2BIT = 0, /* 4 grayscale shades */
  BLT_MODE_4BIT = 1  /* 16 colors (Win 3.1 palette) */
} BlitMode;

/* ============================================================
 * 2-Bit Palette (4 Grayscale)
 *
 * Index | Color
 * ------+------------
 *   0   | Black
 *   1   | Dark Gray
 *   2   | Light Gray
 *   3   | White
 * ============================================================ */
#define BLT_2_BLACK 0
#define BLT_2_DARK_GRAY 1
#define BLT_2_LIGHT_GRAY 2
#define BLT_2_WHITE 3

/* ============================================================
 * 4-Bit Palette (Windows 3.1 Standard 16 Colors)
 *
 * Index | Color         | RGB (8-bit)  | Genesis CRAM
 * ------+---------------+--------------+------------
 *   0   | Black         | 000,000,000  | 0x000
 *   1   | Dark Red      | 128,000,000  | 0x004
 *   2   | Dark Green    | 000,128,000  | 0x040
 *   3   | Dark Yellow   | 128,128,000  | 0x044
 *   4   | Dark Blue     | 000,000,128  | 0x400
 *   5   | Dark Magenta  | 128,000,128  | 0x404
 *   6   | Dark Cyan     | 000,128,128  | 0x440
 *   7   | Light Gray    | 192,192,192  | 0xAAA
 *   8   | Dark Gray     | 128,128,128  | 0x888
 *   9   | Red           | 255,000,000  | 0x00E
 *  10   | Green         | 000,255,000  | 0x0E0
 *  11   | Yellow        | 255,255,000  | 0x0EE
 *  12   | Blue          | 000,000,255  | 0xE00
 *  13   | Magenta       | 255,000,255  | 0xE0E
 *  14   | Cyan          | 000,255,255  | 0xEE0
 *  15   | White         | 255,255,255  | 0xEEE
 *
 * Note: Genesis CRAM is 0x0BBB GGG0 RRR0 (9-bit, BGR order).
 * ============================================================ */
#define BLT_4_BLACK 0
#define BLT_4_DARK_RED 1
#define BLT_4_DARK_GREEN 2
#define BLT_4_DARK_YELLOW 3
#define BLT_4_DARK_BLUE 4
#define BLT_4_DARK_MAGENTA 5
#define BLT_4_DARK_CYAN 6
#define BLT_4_LIGHT_GRAY 7
#define BLT_4_DARK_GRAY 8
#define BLT_4_RED 9
#define BLT_4_GREEN 10
#define BLT_4_YELLOW 11
#define BLT_4_BLUE 12
#define BLT_4_MAGENTA 13
#define BLT_4_CYAN 14
#define BLT_4_WHITE 15

/* ============================================================
 * Mode-Aware Color Constants
 *
 * BLT_BLACK and BLT_WHITE resolve to the correct palette index
 * for the current mode. Safe to use in all rendering code.
 * ============================================================ */
#define BLT_BLACK 0
#define BLT_WHITE BLT_GetWhite()

uint8_t BLT_GetWhite(void);    /* Returns 3 (2-bit) or 15 (4-bit) */
uint8_t BLT_GetMaxColor(void); /* Same as GetWhite */

/* ============================================================
 * Draw Modes (raster operation)
 * ============================================================ */
typedef enum {
  BLT_OP_COPY = 0, /* dst = src                        */
  BLT_OP_OR = 1,   /* dst = dst | src                  */
  BLT_OP_AND = 2,  /* dst = dst & src                  */
  BLT_OP_XOR = 3,  /* dst = dst ^ src                  */
  BLT_OP_NOT = 4   /* dst = ~src                       */
} BlitOp;

/* ============================================================
 * Fill Patterns (8x8)
 *
 * In 2-bit mode: each pattern byte is 4 pixels (2bpp packed).
 * In 4-bit mode: patterns use 2 bytes per row (4bpp packed).
 * For simplicity, patterns store a 1-bit mask and expand to
 * foreground/background colors at fill time.
 * ============================================================ */
typedef struct {
  uint8_t rows[8]; /* 1-bit mask, 8 pixels per row */
} Pattern;

/* Built-in patterns (1-bit masks, expanded at draw time) */
extern const Pattern PAT_SOLID_BLACK; /* All 1s               */
extern const Pattern PAT_SOLID_WHITE; /* All 0s               */
extern const Pattern PAT_GRAY_50;     /* Checkerboard 50%     */
extern const Pattern PAT_GRAY_25;     /* 25% density          */
extern const Pattern PAT_HATCH_HORIZ; /* Horizontal lines     */
extern const Pattern PAT_HATCH_VERT;  /* Vertical lines       */
extern const Pattern PAT_HATCH_DIAG;  /* Diagonal lines       */

/* ============================================================
 * Glyph / Bitmap
 *
 * For text rendering and icon blitting.
 * Glyphs are ALWAYS stored as 1-bit packed, MSB-first.
 * The renderer expands 1-bit glyph data to the active color.
 * ============================================================ */
typedef struct {
  uint8_t width;       /* Width in pixels              */
  uint8_t height;      /* Height in pixels             */
  uint8_t advance;     /* Horizontal advance (spacing) */
  uint8_t baseline;    /* Y offset from top to baseline*/
  const uint8_t *data; /* Packed 1-bit bitmap data     */
} Glyph;

/* ============================================================
 * Font - Collection of glyphs
 * ============================================================ */
typedef struct {
  uint8_t firstChar;   /* First ASCII char (usually 32)*/
  uint8_t lastChar;    /* Last ASCII char (usually 126)*/
  uint8_t height;      /* Line height in pixels        */
  uint8_t ascent;      /* Ascent from baseline         */
  const Glyph *glyphs; /* Array of (lastChar-firstChar+1) */
} Font;

/* ============================================================
 * Framebuffer / Mode Management
 * ============================================================ */

/* Initialize blitter with framebuffer base addresses */
void BLT_Init(uint8_t *framebuffer);

/* Set video mode (2-bit or 4-bit). Clears framebuffer. */
void BLT_SetMode(BlitMode mode);
BlitMode BLT_GetMode(void);

/* Runtime mode-dependent values */
uint16_t BLT_GetBytesPerRow(void);
uint32_t BLT_GetFramebufSize(void);

/* Get/set the active framebuffer pointer */
uint8_t *BLT_GetFramebuffer(void);
void BLT_SetFramebuffer(uint8_t *framebuffer);

/* Clear entire framebuffer to a palette index */
void BLT_Clear(uint8_t color);

/* ============================================================
 * Pixel Operations
 * ============================================================ */
void BLT_SetPixel(int16_t x, int16_t y, uint8_t color);
uint8_t BLT_GetPixel(int16_t x, int16_t y);

/* ============================================================
 * Line Drawing (Bresenham)
 * ============================================================ */
void BLT_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  uint8_t color);

/* Horizontal/vertical fast-path lines */
void BLT_DrawHLine(int16_t x, int16_t y, int16_t w, uint8_t color);
void BLT_DrawVLine(int16_t x, int16_t y, int16_t h, uint8_t color);

/* ============================================================
 * Rectangle Operations
 * ============================================================ */

/* Outlined rectangle (1px border) */
void BLT_DrawRect(const Rect *r, uint8_t color);

/* Filled rectangle (solid color) */
void BLT_FillRect(const Rect *r, uint8_t color);

/* Pattern-filled rectangle (1-bit pattern, fg=black bg=white) */
void BLT_FillRectPattern(const Rect *r, const Pattern *pat);

/* Pattern fill with custom fg/bg colors */
void BLT_FillRectPattern2(const Rect *r, const Pattern *pat, uint8_t fgColor,
                          uint8_t bgColor);

/* ============================================================
 * Bitmap Blitting
 * ============================================================ */

/* Blit a 1-bit packed bitmap to framebuffer.
 * Source is ALWAYS 1bpp. Set bits are drawn as 'color',
 * clear bits are transparent (not drawn). */
void BLT_BlitBitmap1(int16_t dstX, int16_t dstY, const uint8_t *src,
                     int16_t srcW, int16_t srcH, uint8_t color);

/* Blit a native-depth bitmap (2bpp or 4bpp matching current mode) */
void BLT_BlitBitmap(int16_t dstX, int16_t dstY, const uint8_t *src,
                    int16_t srcW, int16_t srcH, BlitOp op);

/* ============================================================
 * Text Rendering
 * ============================================================ */

/* Draw a single character glyph (1bpp source, expanded to color) */
void BLT_DrawGlyph(int16_t x, int16_t y, const Glyph *glyph, uint8_t color);

/* Draw a null-terminated string */
/* Returns the X position after the last character */
int16_t BLT_DrawString(int16_t x, int16_t y, const char *str, const Font *font,
                       uint8_t color);

/* Measure string width in pixels (without drawing) */
int16_t BLT_StringWidth(const char *str, const Font *font);

/* ============================================================
 * Window Frame Rendering (Mac System 1.0 style)
 * ============================================================ */

struct Window;

void BLT_DrawWindowFrame(const struct Window *win, const Font *titleFont);
void BLT_DrawTitleBar(const Rect *titleBar, const char *title, uint8_t hilited,
                      uint8_t hasClose, const Font *titleFont);
void BLT_DrawCloseBox(int16_t x, int16_t y, uint8_t pressed);
void BLT_DrawGrowBox(int16_t x, int16_t y);
void BLT_DrawShadow(const Rect *frame);

/* ============================================================
 * Clipping
 * ============================================================ */
void BLT_SetClipRect(const Rect *r);
void BLT_ResetClip(void);
void BLT_GetClipRect(Rect *r);

/* ============================================================
 * Scroll / Block Transfer
 * ============================================================ */
void BLT_ScrollRect(const Rect *r, int16_t dx, int16_t dy);

#endif /* BLITTER_H */
