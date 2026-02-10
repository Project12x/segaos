/*
 * framebuffer.c - Framebuffer-to-VDP Pipeline (Main CPU)
 *
 * Converts the Sub CPU's linear 4bpp framebuffer in Word RAM
 * to VDP tile format and DMAs to VRAM.
 *
 * Conversion strategy:
 *   Process in strips of FB_STRIP_ROWS tile-rows to fit in Work RAM.
 *   Each strip: 40 tiles wide * N rows = 40*N*32 bytes.
 *   Convert strip to a Work RAM buffer, then DMA to VRAM.
 *
 * Because full conversion (35KB) won't fit alongside our code in
 * 64KB Work RAM, we convert and DMA one strip at a time.
 */

#include "framebuffer.h"
#include <string.h>

/* ============================================================
 * Strip conversion buffer (in Work RAM / .bss)
 *
 * 4 tile-rows * 40 tiles * 32 bytes = 5,120 bytes
 * This is our conversion scratch space.
 * ============================================================ */
#define STRIP_TILE_ROWS 4
#define STRIP_TILES (FB_TILES_X * STRIP_TILE_ROWS)       /* 160 */
#define STRIP_BUF_SIZE (STRIP_TILES * FB_BYTES_PER_TILE) /* 5120 */

static uint8_t strip_buf[STRIP_BUF_SIZE];

/* ============================================================
 * FB_Init - Set up VDP tilemap and palette
 *
 * Fills Plane A name table with sequential tile indices so
 * tiles 0-1119 map directly to screen positions.
 * Loads the Win3.1 palette to CRAM palette line 0.
 * ============================================================ */
void FB_Init(void) {
  uint16_t tile_idx;

  /* Clear VRAM (tiles + name tables) */
  VDP_ClearVRAM();

  /*
   * Fill Plane A name table at VRAM $C000.
   * Name table is 64 tiles wide (H40 scroll size) * 32 tall.
   * We fill the first 40 columns x 28 rows with sequential tiles.
   * Remaining columns (40-63) are set to tile 0 (blank).
   *
   * Name table entry: priority=0, palette=0, no flip, tile index
   */
  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_WRITE(VRAM_PLANE_A);

  tile_idx = 0;
  for (uint16_t ty = 0; ty < 32; ty++) {
    for (uint16_t tx = 0; tx < 64; tx++) {
      if (tx < FB_TILES_X && ty < FB_TILES_Y) {
        /* Active screen area: sequential tile index */
        VDP_DATA_PORT = TILE_ENTRY(0, 0, 0, 0, tile_idx);
        tile_idx++;
      } else {
        /* Outside screen: tile 0 (will be overwritten anyway) */
        VDP_DATA_PORT = 0;
      }
    }
  }

  /* Load Windows 3.1 palette to CRAM palette 0 (16 colors) */
  VDP_LoadPalette(FB_PALETTE_WIN31, 0, 16);

  /* Set background color to black (palette 0, color 0) */
  VDP_SET_REG(VDP_REG_BGCOLOR, 0x00);
}

/* ============================================================
 * convert_strip - Convert one strip of tile-rows from linear to tiles
 *
 * Converts STRIP_TILE_ROWS rows of tiles from the linear framebuffer
 * into the strip_buf in VDP tile format.
 *
 * Linear layout (4bpp):
 *   byte[y * 160 + x/2], high nibble = left pixel
 *
 * VDP tile layout (4bpp):
 *   For tile at (tx, ty), row r within tile:
 *   4 bytes = 8 pixels starting at column tx*8
 *   tile_data[tile_index * 32 + r * 4 .. +3]
 *
 * Both formats pack pixels identically (high nibble = left),
 * so we just copy 4 bytes from the linear row to the tile row.
 * ============================================================ */
static void convert_strip(const uint8_t *linear_fb, uint16_t strip_y) {
  uint16_t tile_rows = STRIP_TILE_ROWS;

  /* Clamp last strip if screen height isn't evenly divisible */
  if (strip_y + tile_rows > FB_TILES_Y) {
    tile_rows = FB_TILES_Y - strip_y;
  }

  for (uint16_t tr = 0; tr < tile_rows; tr++) {
    uint16_t ty = strip_y + tr;

    for (uint16_t tx = 0; tx < FB_TILES_X; tx++) {
      /* Destination: tile within strip buffer */
      uint16_t tile_idx = tr * FB_TILES_X + tx;
      uint8_t *tile_dst = &strip_buf[tile_idx * FB_BYTES_PER_TILE];

      /* Source: 8 rows of 4 bytes each from linear FB */
      uint16_t px_y = ty * 8;
      uint16_t byte_x = tx * 4; /* 8 pixels @ 4bpp = 4 bytes */

      for (uint16_t r = 0; r < 8; r++) {
        const uint8_t *src_row =
            &linear_fb[(px_y + r) * FB_LINEAR_BPR + byte_x];
        uint8_t *dst_row = &tile_dst[r * 4];

        /* Copy 4 bytes (8 pixels) - same nibble packing */
        dst_row[0] = src_row[0];
        dst_row[1] = src_row[1];
        dst_row[2] = src_row[2];
        dst_row[3] = src_row[3];
      }
    }
  }
}

/* ============================================================
 * FB_UpdateFrame - Full frame conversion and DMA
 *
 * Processes the framebuffer in strips:
 *   1. Convert strip from linear Word RAM to tile format
 *   2. DMA strip tiles to VRAM
 *   3. Repeat for all strips
 *
 * Must be called during active display time (not just VBlank)
 * since the full conversion takes ~20ms.
 * ============================================================ */
void FB_UpdateFrame(const uint8_t *wram_bank) {
  uint16_t vram_addr;
  uint16_t strip_tiles;

  for (uint16_t strip_y = 0; strip_y < FB_TILES_Y; strip_y += STRIP_TILE_ROWS) {
    /* Convert this strip */
    convert_strip(wram_bank, strip_y);

    /* Calculate number of tiles in this strip */
    uint16_t tile_rows = STRIP_TILE_ROWS;
    if (strip_y + tile_rows > FB_TILES_Y) {
      tile_rows = FB_TILES_Y - strip_y;
    }
    strip_tiles = FB_TILES_X * tile_rows;

    /* VRAM address for this strip's tiles
     * Tiles are sequential in VRAM starting from tile 0
     * Each tile = 32 bytes = 16 words */
    vram_addr = (strip_y * FB_TILES_X) * FB_BYTES_PER_TILE;

    /* DMA the converted tiles to VRAM
     * Size in words = strip_tiles * 32 / 2 = strip_tiles * 16 */
    VDP_WaitDMA();
    VDP_DMAToVRAM((uint32_t)strip_buf, vram_addr,
                  strip_tiles * (FB_BYTES_PER_TILE / 2));
  }
}
