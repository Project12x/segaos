/*
 * vdp_text_probe.c - Main CPU direct VDP text canary.
 *
 * This probe intentionally bypasses the Sega CD Sub CPU and Word RAM
 * framebuffer path. It proves the smallest useful Genesis VDP text primitive:
 * palette load, 8x8 tile upload, Plane A name-table writes, and VRAM readback.
 */

#include "vdp.h"

#include <stdint.h>

#ifdef VDP_TEXT_PROBE

#define VDP_TEXT_TILE_BASE 1U
#define VDP_TEXT_PLANE_X 7U
#define VDP_TEXT_LINE0_Y 9U
#define VDP_TEXT_LINE1_Y 11U
#define VDP_TEXT_WORDS_PER_TILE_ROW 2U

typedef struct VdpProbeGlyph {
  uint8_t ch;
  uint8_t rows[8];
} VdpProbeGlyph;

/*
 * Reference-code-first record:
 * - Upstream: Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c
 * - License: MIT, copied in third_party/sgdk_font/LICENSE.txt
 * - Files inspected: res/libres.res, res/image/font_default.png, inc/font.h,
 *   src/vdp.c
 * - Reuse mode: direct-copy / format-converted rows for this canary's glyphs.
 *
 * These are SGDK v2.11 font_default.png 8x8 rows for the letters below. This
 * is a low-level canary font, not the final Mac-like SegaOS UI face.
 */
static const VdpProbeGlyph kProbeGlyphs[] = {
    {'S', {0x00, 0x3c, 0x60, 0x3c, 0x06, 0x06, 0x3c, 0x00}},
    {'E', {0x00, 0x7e, 0x60, 0x7c, 0x60, 0x60, 0x7e, 0x00}},
    {'G', {0x00, 0x3e, 0x60, 0x6e, 0x66, 0x66, 0x3e, 0x00}},
    {'A', {0x00, 0x3c, 0x66, 0x66, 0x7e, 0x66, 0x66, 0x00}},
    {'O', {0x00, 0x3c, 0x66, 0x66, 0x66, 0x66, 0x3c, 0x00}},
    {'T', {0x00, 0x7e, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00}},
    {'X', {0x00, 0x66, 0x66, 0x3c, 0x3c, 0x66, 0x66, 0x00}},
    {'K', {0x00, 0x66, 0x6c, 0x78, 0x78, 0x6c, 0x66, 0x00}},
};

static const uint16_t kProbePalette[16] = {
    0x0000, 0x0008, 0x0080, 0x0088, 0x0800, 0x0808, 0x0880, 0x0666,
    0x0444, 0x000e, 0x00e0, 0x00ee, 0x0e00, 0x0e0e, 0x0ee0, 0x0eee,
};

volatile uint16_t segaos_vdp_text_phase;
volatile uint16_t segaos_vdp_text_tile_s_index;
volatile uint16_t segaos_vdp_text_tile_t_index;
volatile uint16_t segaos_vdp_text_row1_word0;
volatile uint16_t segaos_vdp_text_row1_word1;
volatile uint16_t segaos_vdp_text_row2_word0;
volatile uint16_t segaos_vdp_text_row2_word1;
volatile uint16_t segaos_vdp_text_plane_entry0;
volatile uint16_t segaos_vdp_text_plane_entry1;
volatile uint16_t segaos_vdp_text_plane_entry2;
volatile uint16_t segaos_vdp_text_line1_entry0;

void segaos_vdp_text_halt(void) __attribute__((noinline, used));

static uint16_t vdp_text_build_word(uint8_t row_bits, uint8_t first_pixel) {
  uint16_t word = 0;
  uint8_t pixel;

  for (pixel = 0; pixel < 4U; pixel++) {
    uint8_t bit = (uint8_t)(0x80U >> (first_pixel + pixel));
    word <<= 4;
    if (row_bits & bit) {
      word |= 0x000fU;
    }
  }

  return word;
}

static uint16_t vdp_text_tile_for(uint8_t ch) {
  uint16_t i;

  for (i = 0; i < (uint16_t)(sizeof(kProbeGlyphs) / sizeof(kProbeGlyphs[0]));
       i++) {
    if (kProbeGlyphs[i].ch == ch) {
      return (uint16_t)(VDP_TEXT_TILE_BASE + i);
    }
  }

  return 0;
}

static void vdp_text_write_glyph(uint16_t tile_index, const uint8_t rows[8]) {
  uint16_t row;

  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_WRITE((uint16_t)(tile_index * 32U));
  for (row = 0; row < 8U; row++) {
    VDP_DATA_PORT = vdp_text_build_word(rows[row], 0);
    VDP_DATA_PORT = vdp_text_build_word(rows[row], 4);
  }
}

static void vdp_text_upload_glyphs(void) {
  uint16_t i;

  for (i = 0; i < (uint16_t)(sizeof(kProbeGlyphs) / sizeof(kProbeGlyphs[0]));
       i++) {
    vdp_text_write_glyph((uint16_t)(VDP_TEXT_TILE_BASE + i),
                         kProbeGlyphs[i].rows);
  }
}

static uint16_t vdp_text_plane_offset(uint16_t x, uint16_t y) {
  return (uint16_t)(VRAM_PLANE_A + (((y * 64U) + x) << 1));
}

static void vdp_text_draw_string(uint16_t x, uint16_t y, const char *text) {
  uint16_t cursor = x;

  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_WRITE(vdp_text_plane_offset(x, y));
  while (*text) {
    uint16_t tile = 0;

    if (*text != ' ') {
      tile = vdp_text_tile_for((uint8_t)*text);
    }
    VDP_DATA_PORT = TILE_ENTRY(0, 0, 0, 0, tile);
    cursor++;
    text++;
  }

  (void)cursor;
}

static uint16_t vdp_text_read_vram_word(uint16_t byte_offset) {
  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_READ(byte_offset);
  return VDP_DATA_PORT;
}

static uint16_t vdp_text_read_plane_entry(uint16_t x, uint16_t y) {
  return vdp_text_read_vram_word(vdp_text_plane_offset(x, y));
}

void segaos_vdp_text_probe(void) {
  segaos_vdp_text_phase = 0x7001;

  VDP_Init();
  VDP_SET_REG(VDP_REG_MODE2, 0x14);
  VDP_ClearVRAM();
  VDP_LoadPalette(kProbePalette, 0, 16);

  segaos_vdp_text_phase = 0x7002;
  vdp_text_upload_glyphs();
  vdp_text_draw_string(VDP_TEXT_PLANE_X, VDP_TEXT_LINE0_Y, "SEGAOS");
  vdp_text_draw_string(VDP_TEXT_PLANE_X, VDP_TEXT_LINE1_Y, "TEXT OK");

  segaos_vdp_text_tile_s_index = vdp_text_tile_for('S');
  segaos_vdp_text_tile_t_index = vdp_text_tile_for('T');
  segaos_vdp_text_row1_word0 =
      vdp_text_read_vram_word((uint16_t)(segaos_vdp_text_tile_s_index * 32U +
                                         VDP_TEXT_WORDS_PER_TILE_ROW * 2U));
  segaos_vdp_text_row1_word1 =
      vdp_text_read_vram_word((uint16_t)(segaos_vdp_text_tile_s_index * 32U +
                                         VDP_TEXT_WORDS_PER_TILE_ROW * 2U +
                                         2U));
  segaos_vdp_text_row2_word0 =
      vdp_text_read_vram_word((uint16_t)(segaos_vdp_text_tile_s_index * 32U +
                                         (VDP_TEXT_WORDS_PER_TILE_ROW * 2U *
                                          2U)));
  segaos_vdp_text_row2_word1 =
      vdp_text_read_vram_word((uint16_t)(segaos_vdp_text_tile_s_index * 32U +
                                         (VDP_TEXT_WORDS_PER_TILE_ROW * 2U *
                                          2U) +
                                         2U));
  segaos_vdp_text_plane_entry0 =
      vdp_text_read_plane_entry(VDP_TEXT_PLANE_X, VDP_TEXT_LINE0_Y);
  segaos_vdp_text_plane_entry1 =
      vdp_text_read_plane_entry((uint16_t)(VDP_TEXT_PLANE_X + 1U),
                                VDP_TEXT_LINE0_Y);
  segaos_vdp_text_plane_entry2 =
      vdp_text_read_plane_entry((uint16_t)(VDP_TEXT_PLANE_X + 2U),
                                VDP_TEXT_LINE0_Y);
  segaos_vdp_text_line1_entry0 =
      vdp_text_read_plane_entry(VDP_TEXT_PLANE_X, VDP_TEXT_LINE1_Y);

  VDP_SET_REG(VDP_REG_MODE2, 0x74);

  if (segaos_vdp_text_row1_word0 == 0x00ffU &&
      segaos_vdp_text_row1_word1 == 0xff00U &&
      segaos_vdp_text_row2_word0 == 0x0ff0U &&
      segaos_vdp_text_row2_word1 == 0x0000U &&
      segaos_vdp_text_plane_entry0 == TILE_ENTRY(0, 0, 0, 0, 1U) &&
      segaos_vdp_text_plane_entry1 == TILE_ENTRY(0, 0, 0, 0, 2U) &&
      segaos_vdp_text_plane_entry2 == TILE_ENTRY(0, 0, 0, 0, 3U) &&
      segaos_vdp_text_line1_entry0 ==
          TILE_ENTRY(0, 0, 0, 0, segaos_vdp_text_tile_t_index)) {
    segaos_vdp_text_phase = 0x70ff;
  } else {
    segaos_vdp_text_phase = 0x70fe;
  }

  segaos_vdp_text_halt();
}

void segaos_vdp_text_halt(void) {
  while (1) {
  }
}

#endif /* VDP_TEXT_PROBE */
