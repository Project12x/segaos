/*
 * blitter.c - Multi-Mode Bitmap Blitter for Genesis System 1
 *
 * Supports 2bpp (4 grayscale) and 4bpp (16 color) modes.
 * Renders to a 320x224 linear framebuffer in Word RAM.
 *
 * 2bpp: 4 pixels/byte, bits 7-6 = leftmost pixel
 * 4bpp: 2 pixels/byte, high nibble = left pixel
 */

#include "blitter.h"
#include "wm.h"
#include <string.h>

/* ============================================================
 * State
 * ============================================================ */
static uint8_t *fb;      /* Active framebuffer pointer  */
static Rect clipRect;    /* Current clip rectangle      */
static BlitMode curMode; /* Current video mode          */
static uint16_t bpr;     /* Bytes per row (current mode)*/
static uint32_t fbSize;  /* Framebuf size (current mode)*/

/* ============================================================
 * Built-in Patterns (1-bit masks, expanded at draw time)
 * ============================================================ */
const Pattern PAT_SOLID_BLACK = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
const Pattern PAT_SOLID_WHITE = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
const Pattern PAT_GRAY_50 = {{0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55}};
const Pattern PAT_GRAY_25 = {{0x88, 0x00, 0x22, 0x00, 0x88, 0x00, 0x22, 0x00}};
const Pattern PAT_HATCH_HORIZ = {
    {0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF}};
const Pattern PAT_HATCH_VERT = {
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11}};
const Pattern PAT_HATCH_DIAG = {
    {0x80, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01}};

/* ============================================================
 * Internal Helpers
 * ============================================================ */

static inline int16_t max16(int16_t a, int16_t b) { return a > b ? a : b; }
static inline int16_t min16(int16_t a, int16_t b) { return a < b ? a : b; }
static inline int16_t abs16(int16_t v) { return v < 0 ? -v : v; }

static inline uint8_t clip_point(int16_t x, int16_t y) {
  return (x >= clipRect.left && x < clipRect.right && y >= clipRect.top &&
          y < clipRect.bottom);
}

static uint8_t clip_hspan(int16_t *x, int16_t y, int16_t *w) {
  int16_t x0, x1;

  if (y < clipRect.top || y >= clipRect.bottom)
    return 0;

  x0 = *x;
  x1 = x0 + *w;

  if (x0 < clipRect.left)
    x0 = clipRect.left;
  if (x1 > clipRect.right)
    x1 = clipRect.right;

  if (x0 >= x1)
    return 0;

  *x = x0;
  *w = x1 - x0;
  return 1;
}

/* Build a byte filled with a single color repeated */
static inline uint8_t fill_byte_2bit(uint8_t color) {
  uint8_t c2 = color & 0x03;
  return (uint8_t)(c2 | (c2 << 2) | (c2 << 4) | (c2 << 6));
}

static inline uint8_t fill_byte_4bit(uint8_t color) {
  uint8_t c4 = color & 0x0F;
  return (uint8_t)(c4 | (c4 << 4));
}

static inline uint8_t fill_byte(uint8_t color) {
  if (curMode == BLT_MODE_2BIT)
    return fill_byte_2bit(color);
  else
    return fill_byte_4bit(color);
}

/* ============================================================
 * Mode Management
 * ============================================================ */

uint8_t BLT_GetWhite(void) {
  return (curMode == BLT_MODE_2BIT) ? BLT_2_WHITE : BLT_4_WHITE;
}

uint8_t BLT_GetMaxColor(void) { return BLT_GetWhite(); }

uint16_t BLT_GetBytesPerRow(void) { return bpr; }

uint32_t BLT_GetFramebufSize(void) { return fbSize; }

void BLT_SetMode(BlitMode mode) {
  curMode = mode;
  if (mode == BLT_MODE_2BIT) {
    bpr = BLT_BYTES_PER_ROW_2;
    fbSize = BLT_FRAMEBUF_SIZE_2;
  } else {
    bpr = BLT_BYTES_PER_ROW_4;
    fbSize = BLT_FRAMEBUF_SIZE_4;
  }
  if (fb)
    BLT_Clear(BLT_GetWhite());
}

BlitMode BLT_GetMode(void) { return curMode; }

/* ============================================================
 * Framebuffer Management
 * ============================================================ */

void BLT_Init(uint8_t *framebuffer) {
  fb = framebuffer;
  /* Default to 2-bit mode */
  curMode = BLT_MODE_2BIT;
  bpr = BLT_BYTES_PER_ROW_2;
  fbSize = BLT_FRAMEBUF_SIZE_2;
  BLT_ResetClip();
  BLT_Clear(BLT_GetWhite());
}

uint8_t *BLT_GetFramebuffer(void) { return fb; }

void BLT_SetFramebuffer(uint8_t *framebuffer) { fb = framebuffer; }

void BLT_Clear(uint8_t color) {
  if (!fb)
    return;
  memset(fb, fill_byte(color), fbSize);
}

/* ============================================================
 * Pixel Operations
 * ============================================================ */

void BLT_SetPixel(int16_t x, int16_t y, uint8_t color) {
  uint8_t *byte;
  uint8_t shift, mask;

  if (!fb || !clip_point(x, y))
    return;

  if (curMode == BLT_MODE_2BIT) {
    /* 2bpp: 4 pixels per byte, MSB-first */
    /* Pixel 0 in bits 7-6, pixel 1 in bits 5-4, etc. */
    byte = &fb[y * bpr + (x >> 2)];
    shift = 6 - ((x & 3) << 1); /* 6, 4, 2, 0 */
    mask = 0x03 << shift;
    *byte = (*byte & ~mask) | ((color & 0x03) << shift);
  } else {
    /* 4bpp: 2 pixels per byte, high nibble = left */
    byte = &fb[y * bpr + (x >> 1)];
    if (x & 1) {
      /* Low nibble (right pixel) */
      *byte = (*byte & 0xF0) | (color & 0x0F);
    } else {
      /* High nibble (left pixel) */
      *byte = (*byte & 0x0F) | ((color & 0x0F) << 4);
    }
  }
}

uint8_t BLT_GetPixel(int16_t x, int16_t y) {
  if (!fb || x < 0 || x >= BLT_SCREEN_W || y < 0 || y >= BLT_SCREEN_H)
    return 0;

  if (curMode == BLT_MODE_2BIT) {
    uint8_t shift = 6 - ((x & 3) << 1);
    return (fb[y * bpr + (x >> 2)] >> shift) & 0x03;
  } else {
    uint8_t b = fb[y * bpr + (x >> 1)];
    return (x & 1) ? (b & 0x0F) : (b >> 4);
  }
}

/* ============================================================
 * Line Drawing - Fast paths
 * ============================================================ */

void BLT_DrawHLine(int16_t x, int16_t y, int16_t w, uint8_t color) {
  int16_t x1, px;
  uint8_t fb_val;

  if (!fb || w <= 0)
    return;
  if (!clip_hspan(&x, y, &w))
    return;

  x1 = x + w;

  if (curMode == BLT_MODE_2BIT) {
    /* 2bpp: handle partial first byte, full middle bytes, partial last */
    int16_t byteStart = x >> 2;
    int16_t byteEnd = (x1 - 1) >> 2;
    uint8_t fillByte = fill_byte_2bit(color);

    if (byteStart == byteEnd) {
      /* Single byte span */
      for (px = x; px < x1; px++) {
        uint8_t shift = 6 - ((px & 3) << 1);
        uint8_t mask = 0x03 << shift;
        uint8_t *b = &fb[y * bpr + (px >> 2)];
        *b = (*b & ~mask) | ((color & 0x03) << shift);
      }
    } else {
      /* First partial byte */
      for (px = x; px < ((byteStart + 1) << 2) && px < x1; px++) {
        uint8_t shift = 6 - ((px & 3) << 1);
        uint8_t mask = 0x03 << shift;
        uint8_t *b = &fb[y * bpr + (px >> 2)];
        *b = (*b & ~mask) | ((color & 0x03) << shift);
      }
      /* Full middle bytes */
      {
        int16_t i;
        for (i = byteStart + 1; i < byteEnd; i++) {
          fb[y * bpr + i] = fillByte;
        }
      }
      /* Last partial byte */
      for (px = byteEnd << 2; px < x1; px++) {
        uint8_t shift = 6 - ((px & 3) << 1);
        uint8_t mask = 0x03 << shift;
        uint8_t *b = &fb[y * bpr + (px >> 2)];
        *b = (*b & ~mask) | ((color & 0x03) << shift);
      }
    }
  } else {
    /* 4bpp: handle partial first byte, full middle bytes, partial last */
    int16_t byteStart = x >> 1;
    int16_t byteEnd = (x1 - 1) >> 1;
    uint8_t fillByte = fill_byte_4bit(color);

    if (byteStart == byteEnd) {
      for (px = x; px < x1; px++) {
        uint8_t *b = &fb[y * bpr + (px >> 1)];
        if (px & 1)
          *b = (*b & 0xF0) | (color & 0x0F);
        else
          *b = (*b & 0x0F) | ((color & 0x0F) << 4);
      }
    } else {
      /* First pixel if odd */
      if (x & 1) {
        uint8_t *b = &fb[y * bpr + byteStart];
        *b = (*b & 0xF0) | (color & 0x0F);
        byteStart++;
      }
      /* Last pixel if even end */
      if (x1 & 1) {
        uint8_t *b = &fb[y * bpr + byteEnd];
        *b = (*b & 0x0F) | ((color & 0x0F) << 4);
        byteEnd--;
      }
      /* Full middle bytes */
      if (byteEnd >= byteStart) {
        memset(&fb[y * bpr + byteStart], fillByte,
               (uint16_t)(byteEnd - byteStart + 1));
      }
    }
  }
}

void BLT_DrawVLine(int16_t x, int16_t y, int16_t h, uint8_t color) {
  int16_t y0, y1, row;

  if (!fb || h <= 0)
    return;
  if (x < clipRect.left || x >= clipRect.right)
    return;

  y0 = max16(y, clipRect.top);
  y1 = min16(y + h, clipRect.bottom);
  if (y0 >= y1)
    return;

  if (curMode == BLT_MODE_2BIT) {
    int16_t byteCol = x >> 2;
    uint8_t shift = 6 - ((x & 3) << 1);
    uint8_t mask = 0x03 << shift;
    uint8_t val = (color & 0x03) << shift;

    for (row = y0; row < y1; row++) {
      uint8_t *b = &fb[row * bpr + byteCol];
      *b = (*b & ~mask) | val;
    }
  } else {
    int16_t byteCol = x >> 1;
    uint8_t mask, val;

    if (x & 1) {
      mask = 0xF0;
      val = color & 0x0F;
    } else {
      mask = 0x0F;
      val = (color & 0x0F) << 4;
    }

    for (row = y0; row < y1; row++) {
      uint8_t *b = &fb[row * bpr + byteCol];
      *b = (*b & mask) | val;
    }
  }
}

/* ============================================================
 * Line Drawing - Bresenham
 * ============================================================ */

void BLT_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  uint8_t color) {
  int16_t dx, dy, sx, sy, err, e2;

  /* Fast paths */
  if (y0 == y1) {
    int16_t lx = min16(x0, x1);
    int16_t w = abs16(x1 - x0) + 1;
    BLT_DrawHLine(lx, y0, w, color);
    return;
  }
  if (x0 == x1) {
    int16_t ly = min16(y0, y1);
    int16_t h = abs16(y1 - y0) + 1;
    BLT_DrawVLine(x0, ly, h, color);
    return;
  }

  /* Bresenham */
  dx = abs16(x1 - x0);
  dy = -abs16(y1 - y0);
  sx = (x0 < x1) ? 1 : -1;
  sy = (y0 < y1) ? 1 : -1;
  err = dx + dy;

  while (1) {
    BLT_SetPixel(x0, y0, color);

    if (x0 == x1 && y0 == y1)
      break;

    e2 = err * 2;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

/* ============================================================
 * Rectangle Operations
 * ============================================================ */

void BLT_DrawRect(const Rect *r, uint8_t color) {
  int16_t w, h;

  if (!r)
    return;

  w = r->right - r->left;
  h = r->bottom - r->top;

  BLT_DrawHLine(r->left, r->top, w, color);        /* Top    */
  BLT_DrawHLine(r->left, r->bottom - 1, w, color); /* Bottom */
  BLT_DrawVLine(r->left, r->top, h, color);        /* Left   */
  BLT_DrawVLine(r->right - 1, r->top, h, color);   /* Right  */
}

void BLT_FillRect(const Rect *r, uint8_t color) {
  int16_t y, x0, w;

  if (!r)
    return;

  for (y = r->top; y < r->bottom; y++) {
    x0 = r->left;
    w = r->right - r->left;
    BLT_DrawHLine(x0, y, w, color);
  }
}

void BLT_FillRectPattern(const Rect *r, const Pattern *pat) {
  BLT_FillRectPattern2(r, pat, BLT_BLACK, BLT_GetWhite());
}

void BLT_FillRectPattern2(const Rect *r, const Pattern *pat, uint8_t fgColor,
                          uint8_t bgColor) {
  int16_t y, x;

  if (!r || !pat)
    return;

  for (y = r->top; y < r->bottom; y++) {
    uint8_t patRow;

    if (y < clipRect.top || y >= clipRect.bottom)
      continue;

    patRow = pat->rows[y & 7];

    for (x = r->left; x < r->right; x++) {
      if (x < clipRect.left || x >= clipRect.right)
        continue;

      /* Sample 1-bit pattern at (x mod 8) */
      uint8_t patBit = (patRow >> (7 - (x & 7))) & 1;
      BLT_SetPixel(x, y, patBit ? fgColor : bgColor);
    }
  }
}

/* ============================================================
 * Bitmap Blitting
 * ============================================================ */

/* Blit a 1-bit source bitmap. Set bits draw as 'color',
 * clear bits are transparent (destination unchanged). */
void BLT_BlitBitmap1(int16_t dstX, int16_t dstY, const uint8_t *src,
                     int16_t srcW, int16_t srcH, uint8_t color) {
  int16_t srcBytesPerRow;
  int16_t y, x;

  if (!fb || !src)
    return;

  srcBytesPerRow = (srcW + 7) / 8;

  for (y = 0; y < srcH; y++) {
    int16_t screenY = dstY + y;
    if (screenY < clipRect.top || screenY >= clipRect.bottom)
      continue;

    for (x = 0; x < srcW; x++) {
      int16_t screenX = dstX + x;
      if (screenX < clipRect.left || screenX >= clipRect.right)
        continue;

      /* Read 1-bit source */
      uint8_t srcBit =
          (src[y * srcBytesPerRow + (x >> 3)] >> (7 - (x & 7))) & 1;

      if (srcBit)
        BLT_SetPixel(screenX, screenY, color);
      /* Clear bits are transparent */
    }
  }
}

/* Blit a native-depth bitmap (2bpp or 4bpp matching current mode).
 * Source must be packed in the same format as the framebuffer. */
void BLT_BlitBitmap(int16_t dstX, int16_t dstY, const uint8_t *src,
                    int16_t srcW, int16_t srcH, BlitOp op) {
  int16_t y, x;
  int16_t srcBpr;

  if (!fb || !src)
    return;

  if (curMode == BLT_MODE_2BIT)
    srcBpr = (srcW + 3) / 4; /* 2bpp: 4 pixels per byte */
  else
    srcBpr = (srcW + 1) / 2; /* 4bpp: 2 pixels per byte */

  for (y = 0; y < srcH; y++) {
    int16_t screenY = dstY + y;
    if (screenY < clipRect.top || screenY >= clipRect.bottom)
      continue;

    for (x = 0; x < srcW; x++) {
      int16_t screenX = dstX + x;
      uint8_t srcPx, dstPx, result;

      if (screenX < clipRect.left || screenX >= clipRect.right)
        continue;

      /* Read source pixel */
      if (curMode == BLT_MODE_2BIT) {
        uint8_t shift = 6 - ((x & 3) << 1);
        srcPx = (src[y * srcBpr + (x >> 2)] >> shift) & 0x03;
      } else {
        uint8_t b = src[y * srcBpr + (x >> 1)];
        srcPx = (x & 1) ? (b & 0x0F) : (b >> 4);
      }

      dstPx = BLT_GetPixel(screenX, screenY);

      switch (op) {
      case BLT_OP_COPY:
        result = srcPx;
        break;
      case BLT_OP_OR:
        result = dstPx | srcPx;
        break;
      case BLT_OP_AND:
        result = dstPx & srcPx;
        break;
      case BLT_OP_XOR:
        result = dstPx ^ srcPx;
        break;
      case BLT_OP_NOT:
        result = ~srcPx;
        break;
      default:
        result = srcPx;
        break;
      }

      /* Mask result to valid range */
      if (curMode == BLT_MODE_2BIT)
        result &= 0x03;
      else
        result &= 0x0F;

      BLT_SetPixel(screenX, screenY, result);
    }
  }
}

/* ============================================================
 * Text Rendering
 * ============================================================ */

void BLT_DrawGlyph(int16_t x, int16_t y, const Glyph *glyph, uint8_t color) {
  int16_t gx, gy;
  int16_t bytesPerRow;

  if (!glyph || !glyph->data)
    return;

  bytesPerRow = (glyph->width + 7) / 8;

  for (gy = 0; gy < glyph->height; gy++) {
    for (gx = 0; gx < glyph->width; gx++) {
      uint8_t bit =
          (glyph->data[gy * bytesPerRow + (gx >> 3)] >> (7 - (gx & 7))) & 1;
      if (bit) {
        BLT_SetPixel(x + gx, y + gy, color);
      }
    }
  }
}

int16_t BLT_DrawString(int16_t x, int16_t y, const char *str, const Font *font,
                       uint8_t color) {
  if (!str || !font || !font->glyphs)
    return x;

  while (*str) {
    uint8_t ch = (uint8_t)*str;
    if (ch >= font->firstChar && ch <= font->lastChar) {
      const Glyph *g = &font->glyphs[ch - font->firstChar];
      BLT_DrawGlyph(x, y + font->ascent - g->baseline, g, color);
      x += g->advance;
    }
    str++;
  }
  return x;
}

int16_t BLT_StringWidth(const char *str, const Font *font) {
  int16_t w = 0;

  if (!str || !font || !font->glyphs)
    return 0;

  while (*str) {
    uint8_t ch = (uint8_t)*str;
    if (ch >= font->firstChar && ch <= font->lastChar) {
      w += font->glyphs[ch - font->firstChar].advance;
    }
    str++;
  }
  return w;
}

/* ============================================================
 * Window Frame Rendering (Mac System 1.0 style)
 * ============================================================ */

void BLT_DrawCloseBox(int16_t x, int16_t y, uint8_t pressed) {
  Rect box;
  box.left = x;
  box.top = y;
  box.right = x + 12;
  box.bottom = y + 12;

  BLT_DrawRect(&box, BLT_BLACK);

  if (pressed) {
    Rect inner;
    inner.left = x + 1;
    inner.top = y + 1;
    inner.right = x + 11;
    inner.bottom = y + 11;
    BLT_FillRect(&inner, BLT_BLACK);
  }
}

void BLT_DrawGrowBox(int16_t x, int16_t y) {
  Rect outer, inner;

  outer.left = x;
  outer.top = y;
  outer.right = x + 12;
  outer.bottom = y + 12;
  BLT_DrawRect(&outer, BLT_BLACK);

  inner.left = x + 3;
  inner.top = y + 3;
  inner.right = x + 9;
  inner.bottom = y + 9;
  BLT_DrawRect(&inner, BLT_BLACK);
}

void BLT_DrawShadow(const Rect *frame) {
  uint8_t shadowColor;

  if (!frame)
    return;

  /* Use dark gray for shadow in multi-color modes */
  if (curMode == BLT_MODE_2BIT)
    shadowColor = BLT_2_DARK_GRAY;
  else
    shadowColor = BLT_4_DARK_GRAY;

  BLT_DrawVLine(frame->right, frame->top + 1, frame->bottom - frame->top,
                shadowColor);
  BLT_DrawHLine(frame->left + 1, frame->bottom, frame->right - frame->left,
                shadowColor);
}

void BLT_DrawTitleBar(const Rect *titleBar, const char *title, uint8_t hilited,
                      uint8_t hasClose, const Font *titleFont) {
  int16_t textX, textY, textW;

  if (!titleBar)
    return;

  if (hilited) {
    /* Active: striped title bar pattern */
    uint8_t stripeFg, stripeBg;
    if (curMode == BLT_MODE_2BIT) {
      stripeFg = BLT_2_BLACK;
      stripeBg = BLT_2_LIGHT_GRAY;
    } else {
      stripeFg = BLT_4_BLACK;
      stripeBg = BLT_4_LIGHT_GRAY;
    }
    BLT_FillRectPattern2(titleBar, &PAT_GRAY_50, stripeFg, stripeBg);

    /* Clear centered rect for title text */
    if (title && titleFont) {
      Rect textBg;
      textW = BLT_StringWidth(title, titleFont);
      textX = titleBar->left + (titleBar->right - titleBar->left - textW) / 2;
      textY = titleBar->top + 2;

      textBg.left = textX - 4;
      textBg.top = titleBar->top + 1;
      textBg.right = textX + textW + 4;
      textBg.bottom = titleBar->bottom - 1;
      BLT_FillRect(&textBg, BLT_GetWhite());

      BLT_DrawString(textX, textY, title, titleFont, BLT_BLACK);
    }
  } else {
    /* Inactive: light fill */
    uint8_t inactiveBg;
    if (curMode == BLT_MODE_2BIT)
      inactiveBg = BLT_2_WHITE;
    else
      inactiveBg = BLT_4_LIGHT_GRAY;
    BLT_FillRect(titleBar, inactiveBg);

    if (title && titleFont) {
      textW = BLT_StringWidth(title, titleFont);
      textX = titleBar->left + (titleBar->right - titleBar->left - textW) / 2;
      textY = titleBar->top + 2;
      BLT_DrawString(textX, textY, title, titleFont, BLT_BLACK);
    }
  }

  BLT_DrawRect(titleBar, BLT_BLACK);

  if (hasClose) {
    BLT_DrawCloseBox(titleBar->left + 4, titleBar->top + 3, 0);
  }
}

void BLT_DrawWindowFrame(const struct Window *win, const Font *titleFont) {
  const Window *w = (const Window *)win;

  if (!w)
    return;

  /* Outer frame border */
  BLT_DrawRect(&w->frame, BLT_BLACK);

  /* Title bar */
  if (w->style != WM_STYLE_PLAIN && w->style != WM_STYLE_SHADOW) {
    BLT_DrawTitleBar(&w->titleBar, w->title, (w->flags & WF_HILITED) ? 1 : 0,
                     (w->flags & WF_HAS_CLOSE) ? 1 : 0, titleFont);
  }

  /* Grow box */
  if (w->flags & WF_HAS_GROW) {
    BLT_DrawGrowBox(w->frame.right - 14, w->frame.bottom - 14);
  }

  /* Drop shadow */
  if (w->style == WM_STYLE_SHADOW || w->style == WM_STYLE_DIALOG) {
    BLT_DrawShadow(&w->frame);
  }

  /* Content area: clear to white */
  BLT_FillRect(&w->content, BLT_GetWhite());
}

/* ============================================================
 * Clipping
 * ============================================================ */

void BLT_SetClipRect(const Rect *r) {
  if (r) {
    clipRect = *r;
    if (clipRect.left < 0)
      clipRect.left = 0;
    if (clipRect.top < 0)
      clipRect.top = 0;
    if (clipRect.right > BLT_SCREEN_W)
      clipRect.right = BLT_SCREEN_W;
    if (clipRect.bottom > BLT_SCREEN_H)
      clipRect.bottom = BLT_SCREEN_H;
  }
}

void BLT_ResetClip(void) {
  clipRect.left = 0;
  clipRect.top = 0;
  clipRect.right = BLT_SCREEN_W;
  clipRect.bottom = BLT_SCREEN_H;
}

void BLT_GetClipRect(Rect *r) {
  if (r)
    *r = clipRect;
}

/* ============================================================
 * Scroll / Block Transfer
 * ============================================================ */

void BLT_ScrollRect(const Rect *r, int16_t dx, int16_t dy) {
  int16_t y, srcY, w;
  int16_t startY, endY, stepY;

  if (!fb || !r || (dx == 0 && dy == 0))
    return;

  w = r->right - r->left;
  if (w <= 0)
    return;

  if (dy <= 0) {
    startY = r->top;
    endY = r->bottom;
    stepY = 1;
  } else {
    startY = r->bottom - 1;
    endY = r->top - 1;
    stepY = -1;
  }

  for (y = startY; y != endY; y += stepY) {
    srcY = y - dy;
    if (srcY < r->top || srcY >= r->bottom) {
      int16_t cx = r->left;
      int16_t cw = w;
      BLT_DrawHLine(cx, y, cw, BLT_GetWhite());
    } else {
      int16_t x;
      int16_t startX, endX, stepX;

      if (dx <= 0) {
        startX = r->left;
        endX = r->right;
        stepX = 1;
      } else {
        startX = r->right - 1;
        endX = r->left - 1;
        stepX = -1;
      }

      for (x = startX; x != endX; x += stepX) {
        int16_t srcX = x - dx;
        uint8_t pixel;

        if (srcX < r->left || srcX >= r->right) {
          pixel = BLT_GetWhite();
        } else {
          pixel = BLT_GetPixel(srcX, srcY);
        }
        BLT_SetPixel(x, y, pixel);
      }
    }
  }
}
