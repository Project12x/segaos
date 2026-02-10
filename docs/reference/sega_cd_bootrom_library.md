# Boot ROM Library Reference (Main CPU)
Source: Megadev bootlib.md — https://github.com/drojaazu/megadev/blob/master/bootlib.md

## Memory Usage
Boot ROM library uses ~1.25KB of Work RAM at $FFFD00+ and below. It provides:
- Decompression buffer
- Sprite list cache
- Palette cache

The Boot ROM Use block begins at FFFD00 - 1.25KB. You can pick and choose which components to use.

Stack: 256 bytes by default (at $FFFD00, grows downward). For complex C programs, 512+ bytes recommended.

## Default VDP Settings (_BLIB_LOAD_VDPREGS_DEFAULT)

| Register | Value | Description |
|----------|-------|-------------|
| $8004 | Mode Reg 1 | HINT disabled, normal color |
| $8124 | Mode Reg 2 | MD mode, NTSC, VINT enabled, display OFF |
| $9011 | Plane Size | 512x512 (64x64 cells) |
| $8B00 | Mode Reg 3 | Full screen scroll |
| $8C81 | Mode Reg 4 | 40 cell (320px) width |
| $8328 | Window Nametable | $A000 |
| $8230 | Plane A Nametable | $C000 |
| $8407 | Plane B Nametable | $E000 |
| $855C | Sprite Table | $B800 |
| $8D2F | HScroll Table | $BC00 |
| $8700 | BG Color | 0 |
| $8A00 | HINT Counter | 0 |
| $8F02 | Auto-increment | 2 |
| $9100 | Window X | 0 |
| $9200 | Window Y | 0 |

## Key DMA Functions

### _BLIB_DMA_XFER
Standard DMA transfer to VRAM.

### _BLIB_DMA_XFER_WRDRAM
DMA transfer to VRAM for **source data in Word RAM**. This is the key function for our framebuffer→VDP pipeline.

### _BLIB_DMA_QUEUE
Simple DMA queue — processes a list of DMA transfers from Word RAM.

### _BLIB_DMA_FILL / _BLIB_DMA_FILL_CLEAR
Fill/clear VRAM regions via DMA.

## Key VDP Functions
- `_BLIB_CLEAR_VRAM` — Clear all VRAM + VSRAM
- `_BLIB_CLEAR_TABLES` — Clear nametables + sprite table
- `_BLIB_VDP_DISP_ENABLE` / `_BLIB_VDP_DISP_DISABLE` — Toggle display
- `_BLIB_LOAD_PAL` / `_BLIB_LOAD_PAL_UPDATE` — Load palette to cache (+ flag CRAM update)
- `_BLIB_COPY_PAL` — Copy palette cache to CRAM via DMA
- `_BLIB_LOAD_1BPP_TILES` — Load 1bpp tile data

## Comm Flag Semantics
The COMFLAGS register (GA_COMFLAGS) is 16-bit: upper=Main, lower=Sub.

Boot ROM library assigns meaning to bits 0-1:
- **Bit 0 SET** = comm registers updated, fresh copy needed
- **Bit 1 SET** = CPU ready for copy; **CLEAR** = CPU busy

**WARNING**: If using `_BLIB_VINT_HANDLER`, it calls `_BLIB_COMM_SYNC` which uses these predefined flag semantics.

## Interrupt Handler
`_BLIB_VINT_HANDLER` / `_BLIB_VINT_HANDLER_WAIT` — Built-in VBlank handler.
Includes palette/sprite updates and comm sync. Use `_BLIB_SET_VINT` to install a custom handler.
