# SegaOS

A Mac OS-inspired desktop operating system for the Sega CD (Mega CD), featuring a windowed GUI with mouse support rendered by the Sub CPU's software blitter and displayed via the Main CPU's VDP pipeline.

## Features

- **Windowed GUI** -- Mac/Win3.1-style window manager with drag, resize, close
- **Mouse Support** -- Sega Mega Mouse driver with full cursor tracking
- **Software Blitter** -- 2bpp (4 grayscale) and 4bpp (16-color Win3.1 palette) modes
- **Applications** -- Notepad, Calculator, Virtual Keyboard, Paint
- **Dual-CPU Architecture** -- Sub CPU renders framebuffer, Main CPU handles VDP display

## Architecture

```
Main CPU (68000 @ 7.67 MHz)          Sub CPU (68000 @ 12.5 MHz)
+--------------------------+         +--------------------------+
| VDP Init & DMA           |         | Window Manager           |
| Mouse Polling            |         | Software Blitter         |
| Input Event Forwarding   |         | Applications             |
| Framebuffer -> Tile Conv |         | Memory Manager           |
+--------------------------+         +--------------------------+
        |                                    |
        +------ Gate Array Comm Regs --------+
        |                                    |
        +-------- Word RAM (shared) ---------+
```

## Quick Start

### Requirements

- [SGDK](https://github.com/Stephane-D/SGDK) installed at `C:\SDKS\SGDK\`
- SGDK's `bin/` must contain `gcc.exe`, `as.exe`, `ld.exe`, `objcopy.exe`, `make.exe`

### Build

```bash
# Build both CPUs
C:\SDKS\SGDK\bin\make.exe -f Makefile main

# Build Sub CPU only
C:\SDKS\SGDK\bin\make.exe -f Makefile sub

# Clean
C:\SDKS\SGDK\bin\make.exe -f Makefile clean
```

### Output

| Binary | Size | Description |
|--------|------|-------------|
| `build/sub_cpu.bin` | ~29 KB | Sub CPU program (PRG-RAM) |
| `build/main_cpu.bin` | ~28 KB | Main CPU IP (Work RAM) |

## License

All rights reserved.
