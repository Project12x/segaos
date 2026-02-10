# Architecture

## Overview

SegaOS runs on two 68000 CPUs with distinct roles:

```
 Main CPU (7.67 MHz)                    Sub CPU (12.5 MHz)
 +-----------------+                    +------------------+
 | VDP Display     |                    | Window Manager   |
 | Mouse Polling   |  Gate Array Comm   | Software Blitter |
 | Input Forwarding|<==================>| Applications     |
 | Tile Conversion |    Word RAM Bank   | Memory Manager   |
 | DMA to VRAM     |<==================>| Event Dispatch   |
 +-----------------+                    +------------------+
```

## Memory Map

### Main CPU View
| Range | Size | Description |
|-------|------|-------------|
| `$200000-$21FFFF` | 128 KB | Word RAM bank (1M mode) |
| `$420000-$43FFFF` | 128 KB | PRG-RAM window |
| `$A10000-$A1001F` | 32 B | I/O ports (controller/mouse) |
| `$A12000-$A1202F` | 48 B | Gate Array registers |
| `$C00000-$C00013` | 20 B | VDP ports |
| `$FF0000-$FFFFFF` | 64 KB | Work RAM (IP runs here) |

### Sub CPU View
| Range | Size | Description |
|-------|------|-------------|
| `$000000-$07FFFF` | 512 KB | PRG-RAM (SP at $006000) |
| `$0C0000-$0DFFFF` | 128 KB | Word RAM bank (1M mode) |
| `$FF8000-$FF803F` | 64 B | Gate Array registers |

## Component Descriptions

### Blitter (`src/sub/blitter.c`)
Software renderer targeting a linear framebuffer in Word RAM. Supports 2bpp (4 grayscale, 80 B/row) and 4bpp (16-color, 160 B/row) modes. Provides pixel, line, rect, fill, blit, and text rendering with clip rects.

### Window Manager (`src/sub/wm.c`)
Mac OS-style window management with z-ordered window list, title bars, drag, resize, close buttons. Dispatches mouse events to the topmost window. Each window has a content rect drawn by its owning application.

### Framebuffer Pipeline (`src/main/framebuffer.c`)
Converts the Sub CPU's linear 4bpp framebuffer to VDP 8x8 tile format using strip-based processing (5 KB buffer per strip, 7 strips per frame). Both formats use identical 4bpp nibble packing, so conversion is purely a memory rearrangement.

### VDP Interface (`include/vdp.h`)
Standalone hardware interface for the Genesis VDP. Direct register writes, DMA transfers, palette loading, VSync wait. No SGDK dependency.

### Mouse Driver (`src/main/mouse.c`, `include/mouse.h`)
Mega Mouse hardware driver using TH/TR/TL handshake protocol. Reads 9-nibble data packets per frame. Tracks absolute position with configurable bounds.

### Inter-CPU Communication (`include/common.h`, `include/ga_regs.h`)
Gate Array comm flag + 4 command registers for Main->Sub commands. Protocol: Main sets CMD registers, sets flag; Sub reads, processes, sets status flag; Main reads result.

## Data Flow

1. **Input**: Main CPU polls Mega Mouse -> packs event into CMD registers -> Sub CPU reads
2. **Render**: Sub CPU blitter draws to Word RAM (linear 4bpp scanlines)
3. **Display**: Main CPU converts linear->tiles (strip-based) -> DMA to VDP VRAM
4. **Sync**: Word RAM bank swap coordinates read/write access between CPUs
