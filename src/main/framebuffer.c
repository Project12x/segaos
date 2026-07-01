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

#ifndef FB_HOST_TEST
static uint8_t strip_buf[STRIP_BUF_SIZE];
#endif

#ifdef DESKTOP_TIMING_PROBE
volatile uint16_t segaos_desktop_timing_strip_count;
volatile uint16_t segaos_desktop_timing_hv_start;
volatile uint16_t segaos_desktop_timing_hv_end;
volatile uint16_t segaos_desktop_timing_status_end;
volatile uint16_t segaos_desktop_timing_transition_mask;
volatile uint16_t segaos_desktop_timing_dma_clear_mask;
#endif

static void fb_copy_tile_row(const uint8_t *src, uint8_t *dst) {
#if defined(MAIN_CPU) && !defined(FB_HOST_TEST)
  volatile const uint16_t *src_words = (volatile const uint16_t *)src;
  uint16_t row_word0 = src_words[0];
  uint16_t row_word1 = src_words[1];

  dst[0] = (uint8_t)(row_word0 >> 8);
  dst[1] = (uint8_t)(row_word0 & 0x00ff);
  dst[2] = (uint8_t)(row_word1 >> 8);
  dst[3] = (uint8_t)(row_word1 & 0x00ff);
#else
  dst[0] = src[0];
  dst[1] = src[1];
  dst[2] = src[2];
  dst[3] = src[3];
#endif
}

static void fb_convert_tile(const uint8_t *linear_fb, uint16_t tileIndex,
                            uint8_t *tile_dst) {
  uint16_t tx = (uint16_t)(tileIndex % FB_TILES_X);
  uint16_t ty = (uint16_t)(tileIndex / FB_TILES_X);
  uint16_t px_y = (uint16_t)(ty * 8U);
  uint16_t byte_x = (uint16_t)(tx * 4U);
  uint16_t r;

  for (r = 0; r < 8; r++) {
    uint32_t src_offset =
        ((uint32_t)(px_y + r) * FB_LINEAR_BPR) + (uint32_t)byte_x;
    fb_copy_tile_row(linear_fb + src_offset, &tile_dst[r * 4]);
  }
}

uint8_t FB_ConvertTileSpan(const uint8_t *linear_fb, uint16_t firstTile,
                           uint16_t tileCount, uint8_t *tile_out,
                           uint16_t tile_out_bytes) {
  uint32_t needed_bytes;
  uint16_t i;

  if (!linear_fb || !tile_out || tileCount == 0)
    return 0;
  if (firstTile >= FB_TILE_COUNT)
    return 0;
  if (tileCount > (uint16_t)(FB_TILE_COUNT - firstTile))
    return 0;

  needed_bytes = (uint32_t)tileCount * FB_BYTES_PER_TILE;
  if (needed_bytes > tile_out_bytes)
    return 0;

  for (i = 0; i < tileCount; i++) {
    fb_convert_tile(linear_fb, (uint16_t)(firstTile + i),
                    tile_out + ((uint32_t)i * FB_BYTES_PER_TILE));
  }

  return 1;
}

uint8_t FB_FlushTileQueueWithCallback(const uint8_t *linear_fb,
                                      const DirtyTileQueue *queue,
                                      uint8_t *scratch,
                                      uint16_t scratchBytes,
                                      FB_TileUploadCallback upload,
                                      void *user) {
  uint16_t maxChunkTiles;
  uint8_t i;

  if (!linear_fb || !queue || !scratch || !upload)
    return 0;
  if (queue->count == 0)
    return 1;
  if (!queue->items || queue->count > queue->capacity)
    return 0;

  maxChunkTiles = (uint16_t)(scratchBytes / FB_BYTES_PER_TILE);
  if (maxChunkTiles == 0)
    return 0;

  for (i = 0; i < queue->count; i++) {
    const DirtyTileUpload *item = &queue->items[i];
    uint16_t firstTile = item->firstTile;
    uint16_t remaining = item->tileCount;

    if (remaining == 0)
      return 0;
    if (item->byteCount !=
        (uint16_t)((uint32_t)item->tileCount * FB_BYTES_PER_TILE)) {
      return 0;
    }

    while (remaining > 0) {
      uint16_t chunkTiles = remaining;
      uint16_t vramAddr;
      uint16_t wordCount;

      if (chunkTiles > maxChunkTiles)
        chunkTiles = maxChunkTiles;

      if (!FB_ConvertTileSpan(linear_fb, firstTile, chunkTiles, scratch,
                              scratchBytes)) {
        return 0;
      }

      vramAddr = (uint16_t)(firstTile * FB_BYTES_PER_TILE);
      wordCount = (uint16_t)(chunkTiles * (FB_BYTES_PER_TILE / 2));
      if (!upload(scratch, firstTile, chunkTiles, vramAddr, wordCount, user))
        return 0;

      firstTile = (uint16_t)(firstTile + chunkTiles);
      remaining = (uint16_t)(remaining - chunkTiles);
    }
  }

  return 1;
}

/* ============================================================
 * FB_Init - Set up VDP tilemap and palette
 *
 * Fills Plane A name table with sequential tile indices so
 * tiles 0-1119 map directly to screen positions.
 * Loads the Windows-like boot palette to CRAM palette line 0.
 * ============================================================ */
#ifndef FB_HOST_TEST
void FB_Init(void) {
  uint16_t tile_idx;

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

  /* Load Windows-like palette to CRAM palette 0 (16 colors) */
  VDP_LoadPalette(FB_PALETTE_WIN31, 0, 16);

  /* Set background color to black (palette 0, color 0) */
  VDP_SET_REG(VDP_REG_BGCOLOR, 0x00);
}

void FB_ShowBootPattern(void) {
  uint16_t tx;

  VDP_FillVRAM(0x0000, 0x0000, FB_BYTES_PER_TILE / 2);
  VDP_FillVRAM(FB_BYTES_PER_TILE, 0xFFFF,
               (FB_TILE_COUNT - 1) * (FB_BYTES_PER_TILE / 2));

  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_WRITE(VRAM_PLANE_A + (64 * 2));
  for (tx = 0; tx < FB_TILES_X; tx++) {
    VDP_DATA_PORT = TILE_ENTRY(0, 0, 0, 0, 0);
  }
}
#endif /* FB_HOST_TEST */

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
 * Both formats pack pixels identically (high nibble = left). Word RAM access
 * is kept 16-bit wide and unpacked into bytes locally; byte reads from the
 * shared RAM path have produced misleading partial glyphs in emulator testing.
 * ============================================================ */
#ifndef FB_HOST_TEST
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
      fb_convert_tile(linear_fb, (uint16_t)((ty * FB_TILES_X) + tx),
                      tile_dst);
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

static uint8_t fb_dma_tile_upload(const uint8_t *tileData, uint16_t firstTile,
                                  uint16_t tileCount, uint16_t vramAddr,
                                  uint16_t wordCount, void *user) {
  (void)firstTile;
  (void)tileCount;
  (void)user;

  VDP_WaitDMA();
  VDP_DMAToVRAM((uint32_t)tileData, vramAddr, wordCount);
  return 1;
}

uint8_t FB_UpdateTileQueue(const uint8_t *wram_bank,
                           const DirtyTileQueue *queue) {
  return FB_FlushTileQueueWithCallback(wram_bank, queue, strip_buf,
                                       STRIP_BUF_SIZE, fb_dma_tile_upload,
                                       (void *)0);
}

#ifdef DESKTOP_TIMING_PROBE
void FB_UpdateFrameProfile(const uint8_t *wram_bank) {
  uint16_t vram_addr;
  uint16_t strip_tiles;
  uint16_t strip_index = 0;
  uint16_t strip_bit = 1;

  segaos_desktop_timing_strip_count = 0;
  segaos_desktop_timing_hv_start = VDP_HV_COUNTER;
  segaos_desktop_timing_hv_end = segaos_desktop_timing_hv_start;
  segaos_desktop_timing_status_end = VDP_CTRL_PORT;
  segaos_desktop_timing_transition_mask = 0;
  segaos_desktop_timing_dma_clear_mask = 0;

  for (uint16_t strip_y = 0; strip_y < FB_TILES_Y; strip_y += STRIP_TILE_ROWS) {
    uint16_t hv_before;
    uint16_t hv_after_convert;
    uint16_t hv_after_dma;
    uint16_t status_after_dma;
    uint16_t tile_rows = STRIP_TILE_ROWS;

    if (strip_y + tile_rows > FB_TILES_Y) {
      tile_rows = FB_TILES_Y - strip_y;
    }

    hv_before = VDP_HV_COUNTER;

    convert_strip(wram_bank, strip_y);
    hv_after_convert = VDP_HV_COUNTER;

    strip_tiles = FB_TILES_X * tile_rows;
    vram_addr = (strip_y * FB_TILES_X) * FB_BYTES_PER_TILE;

    VDP_WaitDMA();
    VDP_DMAToVRAM((uint32_t)strip_buf, vram_addr,
                  strip_tiles * (FB_BYTES_PER_TILE / 2));
    VDP_WaitDMA();

    hv_after_dma = VDP_HV_COUNTER;
    status_after_dma = VDP_CTRL_PORT;

    if (hv_before != hv_after_convert || hv_after_convert != hv_after_dma) {
      segaos_desktop_timing_transition_mask |= strip_bit;
    }
    if (!(status_after_dma & VDP_STATUS_DMA)) {
      segaos_desktop_timing_dma_clear_mask |= strip_bit;
    }

    segaos_desktop_timing_strip_count = (uint16_t)(strip_index + 1);
    strip_index++;
    strip_bit = (uint16_t)(strip_bit << 1);
  }

  segaos_desktop_timing_hv_end = VDP_HV_COUNTER;
  segaos_desktop_timing_status_end = VDP_CTRL_PORT;
}
#endif
#endif /* FB_HOST_TEST */
