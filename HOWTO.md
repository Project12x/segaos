# How To

## Prerequisites

- **SGDK** installed at `C:\SDKS\SGDK\`
  - Download from [GitHub](https://github.com/Stephane-D/SGDK)
  - Verify: `C:\SDKS\SGDK\bin\gcc.exe --version` should show `m68k-elf-gcc`
- **Python 3** for `tools/build_iso.py` and `tools/verify_disc.py`.
  - The Makefile prefers this machine's local Python 3.12/3.14 install when
    present because the `python` alias may resolve to the Windows Store stub.
- Optional future/reference tool: **mkisofs** or **genisoimage**.
  - Megadev 1.2.0 uses `mkisofs -G boot.bin` semantics for Sega CD boot-disc
    generation.

## Build

```bash
# Full build (Sub CPU + Main CPU + disc image)
C:\SDKS\SGDK\bin\make.exe -f Makefile all

# Full build with a specific Sega CD security region; default is US
C:\SDKS\SGDK\bin\make.exe -f Makefile all CD_REGION=JP

# Minimal dual-CPU boot probe image
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile all BOOT_PROBE=1

# Visible framebuffer probe image
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile all BOOT_PROBE=1 BOOT_PROBE_FRAMEBUFFER=1

# Full app SP, currently for size/regression checks only
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile all BOOT_SAFE_DESKTOP=0

# Sub CPU only
C:\SDKS\SGDK\bin\make.exe -f Makefile sub

# Main CPU only
C:\SDKS\SGDK\bin\make.exe -f Makefile main

# ISO/CUE only; also runs tools/verify_disc.py
C:\SDKS\SGDK\bin\make.exe -f Makefile iso

# Clean all build artifacts
C:\SDKS\SGDK\bin\make.exe -f Makefile clean

# Show toolchain info
C:\SDKS\SGDK\bin\make.exe -f Makefile info
```

If the ISO step fails because `python` is not usable, override the Makefile
variable:

```bash
C:\SDKS\SGDK\bin\make.exe -f Makefile iso PYTHON=C:\Users\estee\AppData\Local\Programs\Python\Python312\python.exe
```

The Python ISO builder uses only the standard library. The desired boot layout
is documented in `docs/reference/sega_cd_boot_disc.md`; `make iso` verifies the
generated image before emulator testing.

## Project Structure

```
segaos/
  include/          # Shared headers (both CPUs)
    libc/           # Freestanding C headers (stdint, string, etc.)
    blitter.h       # Software renderer API
    common.h        # Inter-CPU communication protocol
    framebuffer.h   # Tile conversion pipeline
    ga_regs.h       # Gate Array register definitions
    mouse.h         # Mega Mouse driver
    vdp.h           # VDP hardware interface
    wm.h            # Window manager
  src/
    main/           # Main CPU sources
      crt0.s        # Startup assembly
      main.c        # Boot sequence + main loop
      main.ld       # Linker script (Work RAM $FF0000)
      mouse.c       # Mega Mouse hardware driver
      security.c    # Megadev MIT Sega CD regional security block
      framebuffer.c # Linear-to-tile conversion + DMA
    sub/            # Sub CPU sources
      crt0.s        # SP header + startup
      sub.c         # Sub CPU main + command loop
      sub.ld        # Linker script (PRG-RAM $006000)
      blitter.c     # Software framebuffer renderer
      wm.c          # Window manager
      mem.c         # Memory manager
      notepad.c     # Notepad application
      calc.c        # Calculator application
      paint.c       # Paint application
      vkbd.c        # Virtual keyboard
  docs/reference/   # Sega CD hardware reference docs
  build/            # Build output (gitignored)
```

## Boot Disc Verification

Current reference target:

- cooked ISO9660 data track
- CUE track mode `MODE1/2048`
- 32KB boot sector/system area
- Megadev-compatible boot header fields:
  - IP offset field `$0800`
  - IP bytes physically placed at `$0200`
  - SP offset `$1000`
  - system header string fields are space-padded
  - US builds use `SEGA GENESIS` as the hardware ID
- regional security code included at the start of IP before retail-compatible
  boot testing

`tools/verify_disc.py` checks generated images and exits nonzero on layout
errors. When an IP binary is available, it also checks that the selected
regional security prefix and license marker are present. The selected region is
controlled by `CD_REGION` and defaults to `US`.

## BlastEm IP Probe

Keep Sega CD BIOS files outside the repo. For a local boot-recognition probe,
use a legally obtained BIOS. `tools/probe_blastem_boot.ps1` can copy it to the
BlastEm working directory as `cdbios.bin`, run BlastEm in GDB-remote mode, and
verify that the IP address contains the expected region security prefix.

```powershell
powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -BlastEmDir C:\tmp\blastem\blastem-win64-0.6.3-pre-ec47c727cd65 `
  -Bios "C:\path\to\your\usa-sega-cd-bios.bin"
```

Expected minimum evidence for Milestone A is a breakpoint hit at `$00FF0000`
and the selected region's security bytes visible at `$FF0000`.

## BlastEm Dual-CPU Probe

Build the SegaOS probe image first:

```powershell
C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile all BOOT_PROBE=1
```

Then run the status-only rung:

```powershell
powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Probe DualCpuStatus
```

Expected passing evidence:

- breakpoint hit at `segaos_probe_halt`
- Main magic `0x4d50`
- Sub ready words `0x0002` and `0x5244`
- command status `0x0003`
- echoed result words `0x444e`, `0xa55a`, `0x5aa5`
- Sub trace `0x52fe`

Then run the stricter Word RAM rung:

```powershell
powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Probe DualCpu
```

Expected passing evidence adds:

- Main phase `0x00ff`
- Sub trace `0x54fe`
- Sub readback from `$0C0000`: `0xa55a`, `0x5aa5`
- Main readback from `$200000`: `0xa55a`, `0x5aa5`
- MEM_MODE moves from `0x2a05` to `0x2a04`

Important current diagnostics:

- The current visible framebuffer probe SP is 930 text bytes; `src/sub/sub.ld` uses Megadev-style
  `SUBALIGN(2)`, placing probe `sp_init` at `$602A` and `sp_main` at `$607E`.
- The probe now proves Sub `sp_init`, Sub `sp_main`, and Gate Array
  command/status exchange.
- The Word RAM proof works by clearing RET from Sub in 1M mode. The earlier
  “set RET and wait” approach timed out because RET was already set in the
  boot state where Sub owns bank 0.
- The Main stack should read near `$FFF700`, not `$FFFFxx`; the stack is
  intentionally kept below the Main BIOS work/system-use region.

Then run the framebuffer rung:

```powershell
powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Probe Framebuffer
```

Expected passing evidence adds:

- `probe_sub_trace=0x55fe`
- `probe_wram_survey=0x0003`
- `probe_fb_wram_row0_word0/1=0x1234/0x5678`
- `probe_fb_wram_row1_word0/1=0x9abc/0xdef0`
- `probe_fb_tile_row0_vram=True`
- `probe_fb_tile_row1_vram=True`

For visible-pixel confirmation, build with
`BOOT_PROBE=1 BOOT_PROBE_FRAMEBUFFER=1`, launch the resulting CUE in BlastEm,
press START to leave the Sega CD BIOS screen, then press BlastEm's screenshot
hotkey (`p` by default). Configure BlastEm's `screenshot_path` if the output
directory needs to be controlled. The current passing internal screenshot shows
the expected full-screen alternating 4bpp stripe pattern.

That narrows the next work to an explicit repeated-frame double-buffer policy.

## Boot-Safe Desktop Status

The default normal build uses `BOOT_SAFE_DESKTOP=1`. This intentionally links
only the minimal Sub-side desktop kernel into the boot SP: blitter, memory,
sysfont, window manager, and command loop. Menu/apps are deferred because the
boot SP should stay small until the project has an explicit SPX/module loading
plan.

Current local evidence:

- normal `make all`: passes verifier with a 7,904-byte Sub SP observed with the
  coarse block visual canary in the boot-safe frame
- `BOOT_PROBE=1 BOOT_PROBE_FRAMEBUFFER=1`: passes `-Probe Framebuffer` and
  visible BlastEm internal screenshotting
- `DESKTOP_INIT_PROBE=1`: passes `-Probe DesktopInit`, proving the boot-safe C
  desktop reaches `sub_main`, consumes a first `CMD_RENDER_FRAME`, and returns
  Word RAM for Main upload
- `DESKTOP_INIT_PROBE=1 BOOT_SAFE_TEXT_PROBE=1`: proves plain body text reaches
  Word RAM and VDP tile data without using the striped title-bar renderer
- `DESKTOP_INIT_PROBE=1 BOOT_SAFE_TITLE_PROBE=1`: proves the sampled block title
  row reaches Word RAM and VDP tile data
- normal boot-safe C desktop: visible as a checker desktop/menu/window frame
  with a large block `OS` canary in
  `C:\tmp\segaos_screens_internal\segaos_big_os_20260629_202714.png`;
  sysfont/body text remains opt-in because the earlier striped title/body-text
  screenshot was visibly corrupted

The internal screenshot helper defaults to targeted window messages. For reliable
BIOS START automation under SDL, use `-InputMode SendInputGuarded -ClickToFocus`;
this clicks the BlastEm window, verifies it owns foreground focus before each
key event, remaps `p` to controller START and `f12` to `ui.screenshot`, waits
longer after START, and restores the user's BlastEm config afterward.

Do not advance the full desktop/app loop until the minimal `WM_NewWindow()`
render rung and repeated-frame Word RAM policy are proven.

Additional diagnostic modes:

```powershell
powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Probe DualCpuWramSurvey

powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Probe DualCpuWramRetClear

C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso `
  DESKTOP_INIT_PROBE=1 BOOT_SAFE_TEXT_PROBE=1

powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Probe DesktopInit

C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile iso `
  DESKTOP_INIT_PROBE=1 BOOT_SAFE_TITLE_PROBE=1

powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Probe DesktopInit
```

## Megadev Dual-CPU Control

This independent fixture proves the SegaOS Python ISO builder and regional
security path can boot a Megadev-shaped IP/SP pair and receive Sub-side
breadcrumbs.

```powershell
powershell.exe -ExecutionPolicy Bypass -File tools\build_megadev_dualcpu_control.ps1

powershell.exe -ExecutionPolicy Bypass -File tools\probe_blastem_boot.ps1 `
  -Cue build\megadev_dualcpu_control\megadev_dualcpu_control.cue `
  -Elf build\megadev_dualcpu_control\ip.elf `
  -Probe MegadevControl
```

Expected passing evidence includes Main magic `0x4d43`, phase `0x0002`,
COMSTAT0/1 `0x5350/0x494e`, and Sub flag `0x0003` or `0x0005`.

## Adding a New Application

1. Create `src/sub/myapp.c` with app init and event handlers
2. Create `include/myapp.h` with the public API
3. Register the app in `wm.c` (add to app launcher / menu bar)
4. The Makefile auto-discovers `src/sub/*.c`, so no Makefile changes needed

## Common Tasks

### Change Video Mode
In your Sub CPU app, call `BLT_SetMode(BLT_MODE_4BIT)` for 16-color or `BLT_SetMode(BLT_MODE_2BIT)` for 4 grayscale.

### Modify Palette
Edit `FB_PALETTE_WIN31` in `include/framebuffer.h`. MD format: `----BBB-GGG-RRR-` (3 bits per component, shifted left 1).
