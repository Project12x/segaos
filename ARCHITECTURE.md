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

## Reference Baseline

Low-level Sega CD boot and disc assumptions should be checked against
Megadev 1.2.0:

- repo: https://github.com/drojaazu/megadev
- commit: `7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
- license: MIT
- reuse mode: pattern-only for boot-layout/build architecture; direct-copy for
  `src/main/security.c` from Megadev `lib/security.c`; close-port/pattern-only
  for the SP header/linker contract; control-build/pattern-only for Megadev
  `hello_world` IP/SP behavior

SegaOS does not adopt Megadev's module-driven game architecture wholesale. It
uses Megadev as the maintained reference for boot-sector layout, IP/SP
constraints, Sub CPU CD-ROM ownership, Word RAM caveats, and cooked ISO
generation.

Desktop GUI assumptions should be checked against 68k GEM/TOS prior art,
recorded in `docs/reference/68k_desktop_prior_art.md`:

- EmuTOS: `emutos/emutos@bc34e0b7e7850c5c45a909409d11f7c75ecdc881`,
  GPL-family, reuse mode pattern-only / clean-room behavior specs.
- FreeMiNT/XaAES:
  `freemint/freemint@3172633539fb4281bc3b23c322892f565f303c16`,
  mixed GPL/MiNT-family terms, reuse mode pattern-only.
- OpenGEM:
  `shanecoughlan/OpenGEM@ac06b1a3fec3f3e8defcaaf7ea0338c38c3cef46`,
  GPL for GEM/FreeGEM/OpenGEM sections, reuse mode historical/API
  pattern-only.

These references are not code sources for SegaOS. They establish the clean-room
architecture target: a VDI-like drawing/text/clipping layer, an AES-like
window/event/redraw ownership layer, and a desktop shell on top.

The current pre-alpha boot posture is deliberately flexible. A permanent
Megadev-derived dual-CPU control image built through SegaOS's ISO builder
reports from Sub-side code, and the SegaOS `BOOT_PROBE=1` assembly path now
proves Sub `sp_init`, Sub `sp_main`, and Gate Array command/status exchange.
The one-way Word RAM handoff is also proven: in the observed 1M boot state, Sub
clears RET to expose its `$0C0000` bank at Main `$200000`. The first
framebuffer proof is now passing too: a deterministic 4bpp pattern written by
Sub is converted by Main, read back from VDP VRAM, and confirmed on the visible
display through BlastEm's internal screenshot path. A conservative single-bank
return path now gives bank 0 back to Sub after Main uploads a frame. The
boot-safe C desktop path now publishes ready, consumes the first render command,
and displays a visible Mac-like starter frame in BlastEm. The current starter
uses `WM_DrawDesktop()` for the desktop/menu shell and compact BLT rectangle
primitives for the window outline, with a coarse block `OS` canary. Sysfont and
title text are memory/VRAM-proven in opt-in probes but not visually accepted as
default UI. The next architecture work is the VDI/AES/Desktop split: fixed-font
text proof, dirty-rectangle/clipping proof, root desktop redraw, then minimal
window furniture. The product goal is still a 68k Mac-like desktop on Sega CD;
the bootstrap can change to make that goal reliable.

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

For current bring-up, 4bpp is the canonical runtime target because the Main CPU
framebuffer pipeline assumes 4bpp nibble packing. 2bpp remains a blitter
capability, but it should not be used for the displayed desktop until either a
2bpp-to-VDP converter exists or the Main CPU display path is explicitly changed.

### Window Manager (`src/sub/wm.c`)
Mac OS-style window management with z-ordered window list, title bars, drag, resize, close buttons. Dispatches mouse events to the topmost window. Each window has a content rect drawn by its owning application.

### Framebuffer Pipeline (`src/main/framebuffer.c`)
Converts the Sub CPU's linear 4bpp framebuffer to VDP 8x8 tile format using strip-based processing (5 KB buffer per strip, 7 strips per frame). Both formats use identical 4bpp nibble packing, so conversion is purely a memory rearrangement.

The full-frame path is a bring-up path first. Before the desktop loop is treated
as stable, the project needs a measured VDP transfer policy: VBlank-only dirty
tiles, accepted active-display transfer artifacts, or display-off/full redraws
for transitions.

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

Bring-up should validate those steps one at a time: boot image, Main/Sub CPU
heartbeat, Word RAM bank handoff, deterministic 4bpp test pattern, then the
text, dirty-rect, root desktop, and window-manager render contracts.

## Boot Disc Model

The intended disc image is a cooked ISO9660 data track (`MODE1/2048`) with a
32KB Sega CD system area, equivalent to Megadev's `mkisofs -G boot.bin` build
path.

Important boot-sector constraints:

- IP is loaded to `$FF0000`.
- SP is loaded to `$006000`.
- Header IP offset field should follow the Megadev `$0800` compatibility quirk.
- IP bytes are physically placed at boot-sector offset `$0200`.
- SP bytes are physically placed at boot-sector offset `$1000`.
- IP includes the selected Megadev regional security block first. `CD_REGION`
  defaults to `US`; `JP` and `EU` are supported by the same source file.
- The SP linker uses Megadev-style `SUBALIGN(2)` section layout; the current
  visible framebuffer `BOOT_PROBE=1` SP is 930 text bytes with `sp_init` at
  `$602A` and `sp_main` at `$607E`.
- Initialized Sub data is kept inside the resident SP image rather than in a
  separate boot-time data segment.
- `tools/verify_disc.py` validates the selected security prefix and license
  marker before emulator testing.

Current probe boundary: Main boot, Sub `sp_init`, Sub `sp_main`, Gate Array
command/status, one-way Word RAM bank-0 return, deterministic 4bpp
framebuffer-to-VDP tile readback, C runtime smoke, and boot-safe desktop first
render are proven by the BlastEm/GDB probes. The default visible desktop frame
is also confirmed by BlastEm internal screenshotting. Do not advance the full
desktop/app loop until the fixed-font text, dirty-rectangle/clipping, root
desktop redraw, minimal window-furniture, and repeated-frame bank policies are
explicit.
