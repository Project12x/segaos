# Project State

## Current Phase
**Phase 2: Main CPU VDP Pipeline** (complete)

## Build Status
| Target | Status | Size |
|--------|--------|------|
| Sub CPU (`build/sub_cpu.bin`) | Builds | ~29 KB |
| Main CPU (`build/main_cpu.bin`) | Builds | ~28 KB |

## Toolchain
- SGDK m68k-elf-gcc (C:\SDKS\SGDK\bin\)
- Direct ld.exe linking (bypasses LTO plugin issue)
- libgcc.a for runtime math helpers

## Key Metrics
- Work RAM usage: ~28 KB / 64 KB (Main CPU)
- PRG-RAM usage: ~29 KB / ~488 KB (Sub CPU)
- Strip buffer: 5,120 bytes (4 tile-rows at a time)
- Framebuffer: 35,840 bytes @ 4bpp (320x224)

## Key Classes / Modules
| Module | CPU | File | Purpose |
|--------|-----|------|---------|
| Blitter | Sub | `src/sub/blitter.c` | Software framebuffer renderer |
| Window Manager | Sub | `src/sub/wm.c` | Mac-style window management |
| Memory Manager | Sub | `src/sub/mem.c` | Handle-based allocation |
| Mouse Driver | Main | `src/main/mouse.c` | Mega Mouse hardware polling |
| Framebuffer | Main | `src/main/framebuffer.c` | Linear-to-tile + DMA pipeline |
| VDP | Main | `include/vdp.h` | Standalone VDP register interface |

## Warnings (non-blocking)
- `LOAD segment with RWX permissions` -- linker script refinement needed
- `WORD_RAM_SIZE` redefined between sega_os.h and common.h
