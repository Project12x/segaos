/*
 * vdp.h - VDP (Video Display Processor) Direct Hardware Interface
 *
 * Minimal standalone VDP definitions for Sega CD Main CPU.
 * Does NOT depend on SGDK -- uses direct register writes.
 *
 * VDP Ports (Main CPU addresses):
 *   $C00000 - Data port (R/W)
 *   $C00004 - Control port (R/W)
 *   $C00008 - HV Counter (R)
 */

#ifndef VDP_H
#define VDP_H

#include <stdint.h>

/* VDP port addresses */
#define VDP_DATA_PORT (*(volatile uint16_t *)0xC00000)
#define VDP_DATA_PORT32 (*(volatile uint32_t *)0xC00000)
#define VDP_CTRL_PORT (*(volatile uint16_t *)0xC00004)
#define VDP_CTRL_PORT32 (*(volatile uint32_t *)0xC00004)
#define VDP_HV_COUNTER (*(volatile uint16_t *)0xC00008)

/* VDP register write: OR reg number with $8000, write to control port */
#define VDP_SET_REG(reg, val)                                                  \
  VDP_CTRL_PORT = (uint16_t)(0x8000 | ((reg) << 8) | ((val) & 0xFF))

/* VRAM access commands (write to control port) */
#define VDP_VRAM_WRITE(addr)                                                   \
  VDP_CTRL_PORT32 = (uint32_t)(0x40000000 | (((addr) & 0x3FFF) << 16) |        \
                               (((addr) >> 14) & 3))
#define VDP_CRAM_WRITE(addr)                                                   \
  VDP_CTRL_PORT32 = (uint32_t)(0xC0000000 | (((addr) & 0x3FFF) << 16) |        \
                               (((addr) >> 14) & 3))
#define VDP_VSRAM_WRITE(addr)                                                  \
  VDP_CTRL_PORT32 = (uint32_t)(0x40000010 | (((addr) & 0x3FFF) << 16) |        \
                               (((addr) >> 14) & 3))
#define VDP_VRAM_READ(addr)                                                    \
  VDP_CTRL_PORT32 = (uint32_t)(0x00000000 | (((addr) & 0x3FFF) << 16) |        \
                               (((addr) >> 14) & 3))

/* Status register bits (read from control port) */
#define VDP_STATUS_PAL 0x0001        /* PAL mode */
#define VDP_STATUS_DMA 0x0002        /* DMA in progress */
#define VDP_STATUS_HBLANK 0x0004     /* In HBlank */
#define VDP_STATUS_VBLANK 0x0008     /* In VBlank */
#define VDP_STATUS_ODD 0x0010        /* Odd frame (interlace) */
#define VDP_STATUS_FIFO_FULL 0x0100  /* FIFO full */
#define VDP_STATUS_FIFO_EMPTY 0x0200 /* FIFO empty */

/* VDP Register numbers and their common settings */
#define VDP_REG_MODE1 0x00      /* Mode Set 1 */
#define VDP_REG_MODE2 0x01      /* Mode Set 2 */
#define VDP_REG_PLANEA 0x02     /* Plane A Name Table address */
#define VDP_REG_WINDOW 0x03     /* Window Name Table address */
#define VDP_REG_PLANEB 0x04     /* Plane B Name Table address */
#define VDP_REG_SPRITE 0x05     /* Sprite Table address */
#define VDP_REG_MODE3 0x0B      /* Mode Set 3 */
#define VDP_REG_MODE4 0x0C      /* Mode Set 4 (H40/H32) */
#define VDP_REG_HSCROLL 0x0D    /* HScroll Table address */
#define VDP_REG_AUTOINC 0x0F    /* Auto-increment value */
#define VDP_REG_SCROLLSZ 0x10   /* Scroll size */
#define VDP_REG_WINX 0x11       /* Window X position */
#define VDP_REG_WINY 0x12       /* Window Y position */
#define VDP_REG_DMALEN_LO 0x13  /* DMA Length (low) */
#define VDP_REG_DMALEN_HI 0x14  /* DMA Length (high) */
#define VDP_REG_DMASRC_LO 0x15  /* DMA Source (low) */
#define VDP_REG_DMASRC_MID 0x16 /* DMA Source (mid) */
#define VDP_REG_DMASRC_HI 0x17  /* DMA Source (high + type) */
#define VDP_REG_BGCOLOR 0x07    /* Background color (palette:index) */

/* Plane A name table at VRAM $C000 (default SGDK-compatible) */
#define VRAM_PLANE_A 0xC000
#define VRAM_PLANE_B 0xE000
#define VRAM_SPRITES 0xF800
#define VRAM_HSCROLL 0xFC00

/* Tile entry format in name table:
 * Bits 15    : Priority
 * Bits 14-13 : Palette line (0-3)
 * Bit  12    : Vertical flip
 * Bit  11    : Horizontal flip
 * Bits 10-0  : Tile index
 */
#define TILE_ENTRY(pri, pal, vf, hf, idx)                                      \
  ((uint16_t)(((pri) << 15) | ((pal) << 13) | ((vf) << 12) | ((hf) << 11) |    \
              ((idx) & 0x7FF)))

/* ============================================================
 * Inline VDP Functions
 * ============================================================ */

/* Wait for VBlank */
static inline void VDP_WaitVSync(void) {
  /* Wait until we enter VBlank */
  while (!(VDP_CTRL_PORT & VDP_STATUS_VBLANK)) {
  }
  /* Wait until VBlank ends (so we don't re-trigger) */
  while (VDP_CTRL_PORT & VDP_STATUS_VBLANK) {
  }
}

/* Wait for VBlank start only (for DMA timing) */
static inline void VDP_WaitVBlankStart(void) {
  while (!(VDP_CTRL_PORT & VDP_STATUS_VBLANK)) {
  }
}

/* Wait for DMA completion */
static inline void VDP_WaitDMA(void) {
  while (VDP_CTRL_PORT & VDP_STATUS_DMA) {
  }
}

/* DMA transfer from 68000 address space to VRAM */
static inline void VDP_DMAToVRAM(uint32_t src, uint16_t dest,
                                 uint16_t len_words) {
  /* Source must be word-aligned; address is in words (>>1) */
  uint32_t src_addr = src >> 1;

  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_SET_REG(VDP_REG_DMALEN_LO, len_words & 0xFF);
  VDP_SET_REG(VDP_REG_DMALEN_HI, (len_words >> 8) & 0xFF);
  VDP_SET_REG(VDP_REG_DMASRC_LO, src_addr & 0xFF);
  VDP_SET_REG(VDP_REG_DMASRC_MID, (src_addr >> 8) & 0xFF);
  VDP_SET_REG(VDP_REG_DMASRC_HI, (src_addr >> 16) & 0x7F);

  /* Write DMA destination command (VRAM write + DMA bit) */
  VDP_CTRL_PORT32 =
      (uint32_t)(0x40000080 | (((dest) & 0x3FFF) << 16) | (((dest) >> 14) & 3));
}

/* Load palette (16 colors) to CRAM */
static inline void VDP_LoadPalette(const uint16_t *colors, uint8_t palLine,
                                   uint8_t count) {
  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_CRAM_WRITE(palLine * 32);
  for (uint8_t i = 0; i < count; i++) {
    VDP_DATA_PORT = colors[i];
  }
}

/* Fill VRAM range with a value */
static inline void VDP_FillVRAM(uint16_t dest, uint16_t value, uint16_t count) {
  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_WRITE(dest);
  for (uint16_t i = 0; i < count; i++) {
    VDP_DATA_PORT = value;
  }
}

/* Clear all VRAM */
static inline void VDP_ClearVRAM(void) {
  VDP_SET_REG(VDP_REG_AUTOINC, 2);
  VDP_VRAM_WRITE(0x0000);
  for (uint16_t i = 0; i < 0x8000; i++) {
    VDP_DATA_PORT = 0;
  }
}

/*
 * Initialize VDP to a known state.
 * H40 mode (320px), 224 lines, display enabled.
 */
static inline void VDP_Init(void) {
  /* Read control port to reset any pending state */
  (void)VDP_CTRL_PORT;

  /* Set registers to known defaults */
  VDP_SET_REG(0x00, 0x04); /* Mode 1: HInt disabled, HV counter latch off */
  VDP_SET_REG(0x01,
              0x74); /* Mode 2: Display ON, VInt ON, DMA ON, V28 (224 lines) */
  VDP_SET_REG(0x02, VRAM_PLANE_A >> 10); /* Plane A at $C000 */
  VDP_SET_REG(0x03, 0x00);               /* Window name table unused */
  VDP_SET_REG(0x04, VRAM_PLANE_B >> 13); /* Plane B at $E000 */
  VDP_SET_REG(0x05, VRAM_SPRITES >> 9);  /* Sprite table at $F800 */
  VDP_SET_REG(0x06, 0x00);               /* (unused) */
  VDP_SET_REG(0x07, 0x00);               /* Background: palette 0, color 0 */
  VDP_SET_REG(0x08, 0x00);               /* (unused) */
  VDP_SET_REG(0x09, 0x00);               /* (unused) */
  VDP_SET_REG(0x0A, 0xFF);               /* HInt counter (disabled) */
  VDP_SET_REG(0x0B,
              0x00); /* Mode 3: Full screen VScroll, full screen HScroll */
  VDP_SET_REG(0x0C, 0x81);               /* Mode 4: H40 (320px), no interlace */
  VDP_SET_REG(0x0D, VRAM_HSCROLL >> 10); /* HScroll table at $FC00 */
  VDP_SET_REG(0x0E, 0x00);               /* (unused) */
  VDP_SET_REG(0x0F, 0x02);               /* Auto-increment: 2 bytes */
  VDP_SET_REG(0x10, 0x01);               /* Scroll size: 64x32 (H=64, V=32) */
  VDP_SET_REG(0x11, 0x00);               /* Window X: disabled */
  VDP_SET_REG(0x12, 0x00);               /* Window Y: disabled */

  /* Clear VRAM, CRAM, VSRAM */
  VDP_ClearVRAM();

  /* Clear CRAM (all colors to black) */
  VDP_CRAM_WRITE(0);
  for (uint8_t i = 0; i < 64; i++) {
    VDP_DATA_PORT = 0;
  }

  /* Clear VSRAM */
  VDP_VSRAM_WRITE(0);
  for (uint8_t i = 0; i < 40; i++) {
    VDP_DATA_PORT = 0;
  }
}

#endif /* VDP_H */
