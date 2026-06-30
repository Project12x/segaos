# SegaOS

A Mac OS-inspired desktop operating system for the Sega CD (Mega CD), featuring a windowed GUI with mouse support rendered by the Sub CPU's software blitter and displayed via the Main CPU's VDP pipeline.

## Features

- **Windowed GUI** -- Mac/Win3.1-style window manager with drag, resize, close
- **Mouse Support** -- Sega Mega Mouse driver with full cursor tracking
- **Software Blitter** -- 2bpp (4 grayscale) and 4bpp (16-color Win3.1 palette) modes
- **Applications** -- Notepad, Calculator, Virtual Keyboard, Paint
- **Dual-CPU Architecture** -- Sub CPU renders framebuffer, Main CPU handles VDP display
- **Sega CD Boot Disc Work** -- IP/SP packaging, regional security bytes, and
  cooked ISO generation are aligned with the Megadev 1.2.0 boot-layout
  reference

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
- Python 3 for the stdlib-only ISO builder/verifier path
- Megadev 1.2.0 is the canonical Sega CD boot-layout reference:
  `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`

### Build

```bash
# Build both CPUs and boot-disc image
C:\SDKS\SGDK\bin\make.exe -f Makefile all

# Build a specific Sega CD security region; default is US
C:\SDKS\SGDK\bin\make.exe -f Makefile all CD_REGION=EU

# Build CPU binaries only
C:\SDKS\SGDK\bin\make.exe -f Makefile sub
C:\SDKS\SGDK\bin\make.exe -f Makefile main

# Clean
C:\SDKS\SGDK\bin\make.exe -f Makefile clean
```

On this Windows machine, the `python` app-execution alias may resolve to the
Windows Store stub. The Makefile prefers the local Python 3.12/3.14 install
when present; override `PYTHON` explicitly if needed.

### Output

| Output | Description |
|--------|-------------|
| `build/sub_cpu.bin` | Sub CPU program (SP payload, PRG-RAM) |
| `build/main_cpu.bin` | Main CPU IP payload (Work RAM) |
| `build/segaos.iso` | Cooked ISO9660 data track with Sega CD boot sector |
| `build/segaos.cue` | CUE sheet for the data track |

## Current Boot-Disc Status

The current codebase builds Main/Sub CPU binaries, generates a cooked
`MODE1/2048` ISO track with a 32KB Sega CD system area, and verifies the
Megadev-compatible boot layout. The Main CPU IP now includes Megadev's MIT
regional security block as its first bytes, selected by `CD_REGION` (`US` by
default; `JP` and `EU` are also supported). `make iso` runs
`tools/verify_disc.py` so layout errors and missing or mismatched security
markers fail the build before emulator testing.

The current emulator gate has also been crossed at the boot-disc and
command/status levels. BlastEm with a USA Sega CD BIOS and SGDK GDB reaches the
SegaOS IP, a Megadev-derived dual-CPU control reports from Sub-side code, and
`BOOT_PROBE=1` now proves SegaOS Sub `sp_init`, Sub `sp_main`, and a Gate Array
command/status round trip via `-Probe DualCpuStatus`. The strict `-Probe
DualCpu` check also proves a one-way Word RAM handoff: Sub writes its `$0C0000`
bank, clears RET in 1M mode, and Main sees the pattern at `$200000`. The
`-Probe Framebuffer` check now takes the next step: a deterministic 4bpp
pattern survives the handoff, runs through Main's tile conversion path, and
reads back correctly from VDP VRAM. The visible probe build
`BOOT_PROBE=1 BOOT_PROBE_FRAMEBUFFER=1` also displays the expected full-screen
pattern and is captured by BlastEm's internal screenshotting. The default build
now uses a boot-safe minimal desktop SP, consumes a first `CMD_RENDER_FRAME`,
uploads the returned Word RAM frame, and displays a visible checker
desktop/menu/window starter frame with a coarse block `OS` canary in BlastEm. BLT framebuffer access and
Main framebuffer upload now use word-safe 16-bit Word RAM helpers, and
`WM_DrawDesktop()` owns the boot-safe desktop/menu shell. `BOOT_SAFE_TEXT_PROBE=1`
is now the opt-in build rung for plain body text without the striped title-bar
renderer, and the combined `DESKTOP_INIT_PROBE=1 BOOT_SAFE_TEXT_PROBE=1` path
proves SGDK-derived 8x8 font pixels as a full first-glyph signature (`0xa429`)
in Word RAM and VDP tile data, with Plane A entries `0x0198/0x0199/0x019a`.
An accepted readable BlastEm internal screenshot for that text path is still
pending. The system font is now converted
from SGDK v2.11's MIT `font_default.png`; provenance and license are recorded
in `src/sub/sysfont.c` and `third_party/sgdk_font/`. The visible title/body
canary is now a block wordmark, with
`DESKTOP_INIT_PROBE=1 BOOT_SAFE_TITLE_PROBE=1` proving the sampled block title
row as `0x0fff/0xffff` in both Word RAM and VDP tile data. The current default
capture is
`C:\tmp\segaos_screens_internal\segaos_default_20260629_211333.png`; the
latest opt-in SGDK-font text probe capture attempts are still diagnostic only;
the blank capture `C:\tmp\segaos_screens_internal\segaos_text_probe_20260630_114924.png`
is not a visual pass.
That block canary is diagnostic, not the final UI. After the 68k desktop
prior-art pass and the real-font correction, the next desktop gates are
cleaning up the remaining scaled-text visual presentation, then a
dirty-rectangle/clipping proof, then root desktop redraw, before returning to
minimal window furniture and normal menu/cursor/app rendering.

See [docs/reference/sega_cd_homebrew_2026.md](docs/reference/sega_cd_homebrew_2026.md)
and [docs/reference/sega_cd_boot_disc.md](docs/reference/sega_cd_boot_disc.md)
before changing boot-disc code. See [LESSONS.md](LESSONS.md) before changing
boot, Word RAM, framebuffer, or desktop bring-up code.
See [docs/reference/68k_desktop_prior_art.md](docs/reference/68k_desktop_prior_art.md)
before changing window-manager, text, redraw, or desktop-shell architecture.

## License

All rights reserved.
