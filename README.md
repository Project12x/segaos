# SegaOS

A Mac OS-inspired desktop operating system for the Sega CD (Mega CD), featuring a windowed GUI with mouse support rendered by the Sub CPU's software blitter and displayed via the Main CPU's VDP pipeline.

## Features

- **Windowed GUI** -- Mac/Win3.1-style window manager with drag, resize, close
- **Mouse Support** -- Sega Mega Mouse driver with full cursor tracking
- **Software Blitter** -- 2bpp (4 grayscale) and 4bpp Windows-like palette modes
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

# Build the Main-only direct VDP text canary
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso VDP_TEXT_PROBE=1
powershell -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 -Probe VdpText

# Build and capture the default boot-safe desktop frame deterministically
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso BOOT_SAFE_VISUAL_PROBE=1
powershell -ExecutionPolicy Bypass -File tools\capture_blastem_internal_screenshot.ps1 -DebugAutoBoot -InputMode PostMessage -StartKey Enter -ScreenshotKey P -Template segaos_debug_visual_p_%Y%m%d_%H%M%S.png

# Prove the boot-safe desktop can return Word RAM and render a second frame
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso DESKTOP_REPEAT_PROBE=1
powershell -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 -Probe DesktopRepeat

# Prove several boot-safe single-bank render/upload/return cycles
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso DESKTOP_LOOP_PROBE=1
powershell -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 -Probe DesktopLoop

# Measure the boot-safe full-frame upload path with VDP HV/status samples
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso DESKTOP_TIMING_PROBE=1
powershell -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 -Probe DesktopTiming -GdbTimeoutSeconds 60

# Prove minimal WM_NewWindow allocation/z-order drawing in the boot renderer
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso DESKTOP_WM_PROBE=1
powershell -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 -Probe DesktopWm

# Capture that WM-backed boot frame through BlastEm's internal screenshot path
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso DESKTOP_WM_PROBE=1 BOOT_SAFE_VISUAL_PROBE=1
powershell -ExecutionPolicy Bypass -File tools\capture_blastem_internal_screenshot.ps1 -DebugAutoBoot -InputMode PostMessage -StartKey Enter -ScreenshotKey P -Template segaos_wm_probe_%Y%m%d_%H%M%S.png

# After a probe build, force the normal variant so shared objects are rebuilt
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso

# Run host-side redraw, dirty-transfer queue, framebuffer, and probe-timeout tests
C:\SDKS\SGDK\bin\make.exe -r -f Makefile host-tests

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
command/status round trip via `-Probe DualCpuStatus`. Main-to-Sub commands use
CFM only as a pending signal; the opcode is carried in `CMD0`, with payload in
`CMD1`-`CMD4`. The strict `-Probe
DualCpu` check also proves a one-way Word RAM handoff: Sub writes its `$0C0000`
bank, clears RET in 1M mode, and Main sees the pattern at `$200000`. The
`-Probe Framebuffer` check now takes the next step: a deterministic 4bpp
pattern survives the handoff, runs through Main's tile conversion path, and
reads back correctly from VDP VRAM. The visible probe build
`BOOT_PROBE=1 BOOT_PROBE_FRAMEBUFFER=1` also displays the expected full-screen
pattern and is captured by BlastEm's internal screenshotting. The default build
now uses a boot-safe minimal desktop SP, consumes a first `CMD_RENDER_FRAME`,
uploads the returned Word RAM frame, and displays a visible checker
desktop/menu/window starter frame with real SGDK-font menu, title, and body
text. BLT framebuffer access and
Main framebuffer upload now use word-safe 16-bit Word RAM helpers, and
`WM_DrawDesktop()` owns the boot-safe desktop/menu shell. `VDP_TEXT_PROBE=1` is
now the Main-only text canary: it bypasses Sub CPU and Word RAM, uploads
SGDK-derived 8x8 glyph tiles directly to VDP VRAM, passes `-Probe VdpText`, and
has a readable BlastEm internal screenshot at
`C:\tmp\segaos_screens_internal\segaos_vdp_text_direct_20260630_181733.png`.
`BOOT_SAFE_TEXT_PROBE=1` remains the opt-in build rung for desktop-composited
plain body text without the striped title-bar renderer, and the combined
`DESKTOP_INIT_PROBE=1 BOOT_SAFE_TEXT_PROBE=1` path proves SGDK-derived 8x8 font
pixels as a full first-glyph signature (`0xd2dd`) in Word RAM and VDP tile data,
with Plane A entries `0x0198/0x0199/0x019a`. The desktop-composited scaled text
path now has an accepted readable BlastEm internal screenshot at
`C:\tmp\segaos_screens_internal\segaos_desktop_text_opaque_20260630_183441.png`.
The fix was to reserve VDP background-plane palette index 0 for
transparent/backdrop and render opaque black framebuffer ink with palette index
1. The system font is now converted
from SGDK v2.11's MIT `font_default.png`; provenance and license are recorded
in `src/sub/sysfont.c` and `third_party/sgdk_font/`. The visible title/body
canary now uses that font in the default boot-safe window, with
`DESKTOP_INIT_PROBE=1 BOOT_SAFE_TITLE_PROBE=1` proving a sampled default body
text row as `0xf11f/0x1f11` in both Word RAM and VDP tile data. The default
boot-safe visual capture is now debugger-driven: build with
`BOOT_SAFE_VISUAL_PROBE=1` and run
`tools\capture_blastem_internal_screenshot.ps1` with
`-DebugAutoBoot -StartKey Enter -ScreenshotKey P`. GDB proves
`segaos_visual_probe_halt` and phase `0x76ff`,
then resumes BlastEm so the emulator's internal `ui.screenshot` binding captures
the app frame instead of a BIOS screen. The latest accepted default screenshot is
`C:\tmp\segaos_screens_internal\segaos_repeat_20260630_231605.png`; the
older block-canary frame at
`C:\tmp\segaos_screens_internal\segaos_default_20260629_211333.png` is retained
only as historical evidence. The
known-bad blank capture
`C:\tmp\segaos_screens_internal\segaos_text_probe_20260630_114924.png` and the
pre-fix sparse/corrupt capture
`C:\tmp\segaos_screens_internal\segaos_desktop_text_probe_20260630_182543.png`
are diagnostic references, not current visual passes.
`DESKTOP_REPEAT_PROBE=1` now proves a second boot-safe `CMD_RENDER_FRAME`
after Main returns Word RAM to Sub: the probe reaches phase `0x82ff`, sees the
second command complete with status `0x0003` and trace `0x7404`, observes the
released 1M state as MEM_MODE `0x2a06`, and reads the repeated title row from
VDP as `0xf11f/0x1f11`. `DESKTOP_LOOP_PROBE=1` extends that same conservative
path to four additional render/upload/return cycles after the first frame:
the probe reaches phase `0x83ff`, counts four loop frames, keeps status
`0x0003` and trace `0x7404`, observes MEM_MODE `0x2a06` before and after each
Main-side return, and still reads the title-row VRAM words as
`0xf11f/0x1f11`. `DESKTOP_TIMING_PROBE=1` adds the first measured VDP
frame-transfer rung: it profiles the boot-safe full-frame upload as 7
strip-based DMA transfers, reaches phase `0x84ff`, observes HV `0xbc1d` to
`0xfdb2` in the current BlastEm run, records final VDP status `0x320c`, and
proves every strip changed the HV counter and ended with DMA clear via masks
`0x007f/0x007f`. `DESKTOP_WM_PROBE=1` now proves the next narrow
window-manager rung: `WM_Init()` plus one `WM_NewWindow()` creates a visible,
hilited document window, renders it through the dirty-window clip path, and
passes `-Probe DesktopWm` with window count `0x0001`, active flags `0x0007`,
and frame origin `0x2822` (`40,34`). The combined
`DESKTOP_WM_PROBE=1 BOOT_SAFE_VISUAL_PROBE=1` build also has a
debugger-backed BlastEm internal screenshot at
`C:\tmp\segaos_screens_internal\segaos_wm_probe_20260630_235603.png`. That
probe also exposed a freestanding C-runtime bug: m68k GCC optimized the local
`memset()` into a recursive self-call until the Makefile explicitly added
`-ffreestanding -fno-builtin`.
That boot-safe window is diagnostic, not the final UI. After the 68k desktop
prior-art pass, the real-font correction, the palette-index transparency fix,
default text restoration, and the host-tested dirty-rectangle/clipping pool, root
desktop redraw and the first direct boot-safe window furniture now go through
the same dirty-list clipping path. The short multi-frame single-bank loop is
now GDB-proven and the full-frame upload path has its first HV/status timing
probe. The dirty-rectangle module now also has host-tested VDP transfer
planning: small dirty tile ranges can be counted against a caller-supplied
budget and turned into explicit tile upload spans, while the full 40x28 tile
frame is known to exceed the NTSC VBlank reference budget and is sliced to that
budget. The Main framebuffer module now exposes a host-tested
`FB_ConvertTileSpan()` seam that converts any planned tile span from linear
4bpp Word RAM layout into VDP tile bytes. This is still a planner/conversion
checkpoint; the live VBlank hardware flush is pending. The next desktop gate is
still a measured long-running frame policy; the full alternating double-buffer
and dirty-tile VBlank policies remain later stability work before returning to
normal menu/cursor/app rendering.

See [docs/reference/sega_cd_homebrew_2026.md](docs/reference/sega_cd_homebrew_2026.md)
and [docs/reference/sega_cd_boot_disc.md](docs/reference/sega_cd_boot_disc.md)
before changing boot-disc code. See [LESSONS.md](LESSONS.md) before changing
boot, Word RAM, framebuffer, or desktop bring-up code.
See [docs/reference/68k_desktop_prior_art.md](docs/reference/68k_desktop_prior_art.md)
before changing window-manager, text, redraw, or desktop-shell architecture.

## License

All rights reserved.
