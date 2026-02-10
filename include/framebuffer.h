/*
 * framebuffer.h - Framebuffer-to-VDP Pipeline for Sega CD Main CPU
 *
 * Converts the Sub CPU's linear scanline framebuffer in Word RAM
 * to VDP tile format and transfers via DMA.
 *
 * Pipeline:
 *   1. Sub CPU renders to Word RAM bank (linear 4bpp, 320x224)
 *   2. Main CPU converts linear -> tile format (in Work RAM)
 *   3. Main CPU DMA transfers tiles to VRAM during VBlank
 *
 * The VDP and blitter both use 4bpp with the same nibble packing
 * (high nibble = left pixel), so conversion is purely a memory
 * layout rearrangement -- no pixel reformatting needed.
 *
 * Tile layout:
 *   Screen = 40 tiles wide x 28 tiles tall = 1,120 tiles
 *   Each tile = 8x8 pixels @ 4bpp = 32 bytes (4 bytes/row x 8 rows)
 *   Total tile data = 35,840 bytes
 *
 * Memory budget:
 *   We can't fit the full tile buffer in Work RAM (35KB of 64KB).
 *   Instead, we convert and DMA in strips (e.g., 4 rows of tiles
 *   at a time = 40 * 4 * 32 = 5,120 bytes per strip).
 */

#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "vdp.h"
#include <stdint.h>


/* ============================================================
 * Display Constants
 * ============================================================ */
#define FB_SCREEN_W 320
#define FB_SCREEN_H 224
#define FB_TILES_X (FB_SCREEN_W / 8)            /* 40 */
#define FB_TILES_Y (FB_SCREEN_H / 8)            /* 28 */
#define FB_TILE_COUNT (FB_TILES_X * FB_TILES_Y) /* 1120 */
#define FB_BYTES_PER_TILE 32
#define FB_LINEAR_BPR 160 /* 4bpp: 320 pixels / 2 = 160 bytes/row */

/* Strip conversion: how many tile-rows per strip */
#define FB_STRIP_ROWS 4 /* 4 tile-rows = 32 pixel-rows */
#define FB_STRIP_TILES (FB_TILES_X * FB_STRIP_ROWS) /* 160 tiles */
#define FB_STRIP_SIZE (FB_STRIP_TILES * FB_TILE_COUNT_PER_STRIP)

/* Word RAM bank addresses as seen by Main CPU (1M mode) */
#define WRAM_BANK0_MAIN ((const uint8_t *)0x200000)
#define WRAM_BANK1_MAIN ((const uint8_t *)0x220000)

/* ============================================================
 * Windows 3.1 Palette (16 colors, Mega Drive 9-bit RGB format)
 *
 * MD format: ----BBB-GGG-RRR-  (3 bits each, shifted left 1)
 * Each component: 0-7 mapped to 9-bit (0=0x0, 7=0xE)
 * ============================================================ */
static const uint16_t FB_PALETTE_WIN31[16] = {
    0x0000, /* 0: Black         (0,0,0)     */
    0x0400, /* 1: Dark Red      (128,0,0)   */
    0x0040, /* 2: Dark Green    (0,128,0)   */
    0x0440, /* 3: Dark Yellow   (128,128,0) */
    0x0004, /* 4: Dark Blue     (0,0,128)   */
    0x0404, /* 5: Dark Magenta  (128,0,128) */
    0x0044, /* 6: Dark Cyan     (0,128,128) */
    0x0AAA, /* 7: Light Gray    (192,192,192) -> $0AAA */
    0x0666, /* 8: Dark Gray     (128,128,128) -> $0666 */
    0x060E, /* 9: Red           (255,0,0)   -> $060E (adjusted) */
    0x000E, /* A: Green         (0,255,0)   */
    0x0E0E, /* B: Yellow        (255,255,0) */
    0x0E00, /* C: Blue          (0,0,255)   */
    0x0E06, /* D: Magenta       (255,0,255) */
    0x0EE0, /* E: Cyan          (0,255,255) */
    0x0EEE, /* F: White         (255,255,255) */
};

/* 4-shade grayscale palette */
static const uint16_t FB_PALETTE_GRAY4[4] = {
    0x0000, /* Black */
    0x0444, /* Dark Gray */
    0x0AAA, /* Light Gray */
    0x0EEE, /* White */
};

/* ============================================================
 * Initialization
 *
 * Sets up the VDP tilemap (Plane A) with sequential tile indices,
 * loads the palette, and prepares for framebuffer updates.
 * ============================================================ */
void FB_Init(void);

/* ============================================================
 * Frame Update
 *
 * Converts the linear framebuffer in Word RAM (Main CPU's view)
 * to VDP tile format and DMAs to VRAM. Call once per frame after
 * the Sub CPU has finished rendering and banks are swapped.
 *
 * wram_bank: pointer to the Word RAM bank to read from
 *            (typically WRAM_BANK0_MAIN or WRAM_BANK1_MAIN)
 * ============================================================ */
void FB_UpdateFrame(const uint8_t *wram_bank);

#endif /* FRAMEBUFFER_H */
