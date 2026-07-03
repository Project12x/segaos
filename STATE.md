# Project State

## Current Phase
**Milestone C/D/E bridge: VDI/AES desktop foundations**

Phase 1 and Phase 2 are complete. The production Word RAM bank-sync code paths
are present; the boot-safe single-bank path now has two-frame and short
multi-frame loop probes, and the full-frame upload path now has a first
HV/status timing probe. Full alternating 1M double buffering still needs a
runtime policy.
Milestone A is complete:
the repeatable boot-disc artifact builds, verifies, includes regional security
bytes in the Main CPU IP, and reaches the SegaOS IP in BlastEm. Milestone B is
complete: the assembly-only `BOOT_PROBE=1` SP reports from `sp_init`, waits in
`sp_main`, echoes a Main command, clears RET to return its 1M bank-0 Word RAM
work area, and Main reads the expected `0xa55a/0x5aa5` pattern at `$200000`.
Milestone C has now crossed the first display gate: `-Probe Framebuffer` proves
a deterministic Sub-written 4bpp pattern survives the same handoff, runs
through Main's linear-to-tile path, and reads back correctly from VDP VRAM; a
visible `BOOT_PROBE=1 BOOT_PROBE_FRAMEBUFFER=1` build is also captured by
BlastEm's internal screenshot path with the expected full-screen pattern.

The default build uses a boot-safe minimal desktop SP (`BOOT_SAFE_DESKTOP=1`):
memory/sysfont/window-manager code are kept in the boot SP while menu/apps are
deferred. A C-runtime smoke image proves the normal SP header, BSS clear,
`sub_init`, `sub_main`, and command handshake. `DESKTOP_INIT_PROBE=1` now proves
the real boot-safe desktop SP reaches the C command loop, completes a first
`CMD_RENDER_FRAME`, and lets Main upload the returned Word RAM frame. The
default build now displays a visible Mac-like boot-safe frame through BLT's
word-safe framebuffer backend: checker desktop, menu separator, and a compact
window starter frame. The starter frame now uses the real SGDK-derived font for
menu, title, and body text instead of the old block `OS` visual canary. The
latest accepted default internal screenshot is
`C:\tmp\segaos_screens_internal\segaos_repeat_20260630_231605.png`,
captured from a `BOOT_SAFE_VISUAL_PROBE=1` build after GDB hit
`segaos_visual_probe_halt` with phase `0x76ff` and BlastEm's own
`ui.screenshot` binding wrote the PNG. The previous block-canary screenshot is
retained as historical evidence at
`C:\tmp\segaos_screens_internal\segaos_default_20260629_211333.png`.
An opt-in SGDK-derived font text probe capture is also available at
`C:\tmp\segaos_screens_internal\segaos_sgdk_text_20260629_215956.png`.
The earlier striped title/body-text attempt at
`C:\tmp\segaos_screens_internal\segaos_internal_20260629_171815.png` remains the
known-bad visual reference; body text stays opt-in behind `BOOT_SAFE_TEXT_PROBE=1`.
The old hand-authored 6x10 placeholder sysfont has been replaced with SGDK
v2.11's MIT 8x8 `font_default.png`, converted into SegaOS' 1bpp `Glyph` rows
with attribution in `third_party/sgdk_font/`.
The new `VDP_TEXT_PROBE=1` rung bypasses Sub CPU and Word RAM entirely: Main
CPU uploads SGDK-derived 8x8 glyph tiles directly to VDP VRAM, maps Plane A to
`SEGAOS` / `TEXT OK`, passes `tools/probe_blastem_boot.ps1 -Probe VdpText`,
and has a readable BlastEm internal screenshot at
`C:\tmp\segaos_screens_internal\segaos_vdp_text_direct_20260630_181733.png`.
That proves basic VDP tile text is not the hard part; any remaining corrupted
desktop text is in the scaled framebuffer/compositor path above it.
That scaled path now has its own visual pass: `DESKTOP_INIT_PROBE=1
BOOT_SAFE_TEXT_PROBE=1` renders readable SGDK-font text through the Sub
framebuffer, Main tile conversion, and VDP plane upload at
`C:\tmp\segaos_screens_internal\segaos_desktop_text_opaque_20260630_183441.png`.
The corruption was caused by treating palette index 0 as opaque black ink; VDP
background plane color 0 is transparent, so the 4bpp boot palette now reserves
index 0 for backdrop/transparent and uses index 1 for opaque black text/lines.
`DESKTOP_REPEAT_PROBE=1` now proves a second boot-safe render in one run:
after Main uploads and releases the first returned frame, the Sub CPU consumes
a second `CMD_RENDER_FRAME`, returns status `0x0003` with trace `0x7404`, and
Main reads the repeated title-row VRAM words as `0xf11f/0x1f11`.
`DESKTOP_LOOP_PROBE=1` now extends that same conservative single-bank path:
after the first frame, Main drives four additional render/upload/return cycles,
the probe reaches phase `0x83ff`, counts four loop frames, keeps status
`0x0003` and trace `0x7404`, observes MEM_MODE `0x2a06` across the Main-side
return points, and still reads the final title-row VRAM words as
`0xf11f/0x1f11`.
`DESKTOP_TIMING_PROBE=1` now gives the first VDP timing-budget evidence for
the current boot-safe full-frame upload path. The probe reaches phase `0x84ff`,
profiles 7 strip DMA transfers, records HV `0xbc1d` to `0xfdb2` in the current
BlastEm run, records final VDP status `0x320c`, and proves every strip both
moved the HV counter and ended with DMA clear via masks `0x007f/0x007f`.
`DESKTOP_WM_PROBE=1` now proves a minimal real window-manager boot render path:
the Sub CPU runs `WM_Init()`, creates one `WM_NewWindow()` document window,
walks z-order through the dirty-window clip path, and `-Probe DesktopWm`
verifies window count `0x0001`, active flags `0x0007`, frame origin `0x2822`,
and terminal trace `0x7404`. `DESKTOP_WM_PROBE=1 BOOT_SAFE_VISUAL_PROBE=1`
then captures the accepted WM-backed frame at
`C:\tmp\segaos_screens_internal\segaos_wm_probe_20260630_235603.png`. The root
cause of the earlier WM probe timeout was freestanding-toolchain related: the
local `memset()` compiled into a recursive self-call until the build added
`-ffreestanding -fno-builtin`.
`DESKTOP_DIRTY_QUEUE_PROBE=1` now proves the first hardware-backed dirty tile
upload through the Main queue wrapper. The diagnostic path poisons the sampled
title tile row in VRAM, constructs a one-entry `DirtyTileQueue` for tile
`0x0147`, calls `FB_UpdateTileQueue()`, reaches phase `0x85ff`, and reads
`0xf11f/0x1f11` from VRAM matching the rendered Word RAM source. This is a
narrow dirty-upload proof, not a production VBlank scheduler yet.

A June 2026 68k desktop prior-art pass is now documented in
`docs/reference/68k_desktop_prior_art.md`. EmuTOS is the primary desktop
structure reference, FreeMiNT/XaAES is a redraw/event failure-mode reference,
and OpenGEM is historical/API context. All three are GPL-family or mixed
license sources, so they are pattern-only references: no copying or close ports.
The active desktop strategy is now to split SegaOS into a VDI-like
drawing/text/clipping layer, an AES-like window/event/redraw ownership layer,
and a desktop shell above them.
The first clipping/redraw data structure is now in place:
`src/sub/dirty_rect.c` provides a host-tested static dirty rectangle list and
rectangle primitive helpers, while `WM_InvalidateRect()` delegates to that
contract. The same module now includes the first dirty-to-VDP planning helpers:
`DR_TileRangeBudget()` turns an 8x8 tile range into first tile, tile count,
byte count, row-span count, and caller-supplied budget fit, while
`DR_QueueTileRange()` and `DR_BuildTileQueueFromDirtyList()` turn dirty ranges
into explicit upload spans with separate budget-exceeded and storage-overflow
flags.

The active strategy is a bring-up ladder:

1. Reproducible boot artifact
2. Minimal dual-CPU probe
3. 4bpp framebuffer probe
4. VDP timing budget
5. Desktop foundations: text, clipping, root redraw, then window furniture
6. Storage and OS features, planned around external Backup RAM cartridge-class
   writable persistence with internal BRAM as fallback, plus a staged BASIC
   tooling path
7. Compatibility and release hardening

## Build Status
| Target | Status | Notes |
|--------|--------|-------|
| Sub CPU (`build/sub_cpu.bin`) | Builds | Boot-safe desktop default: 11,808 text bytes / 2,292 BSS bytes observed locally with the SGDK-font starter window, dirty-rect module, target-compiled but unreferenced BASIC core, and target-assembled internal BRAM BIOS raw-call stubs; full app SP is deferred |
| Main CPU (`build/main_cpu.bin`) | Builds | 2,856 text bytes observed locally with US security block in the forced normal default build |
| CPU-only build | Passing | `C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile sub main` |
| Full app Sub build | Passing | `C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile sub BOOT_SAFE_DESKTOP=0` now excludes `runtime_smoke.c`; observed 24,088 text bytes / 8,500 BSS bytes |
| Disc image (`build/segaos.iso/.cue`) | Builds and verifies | `make iso` writes cooked `MODE1/2048` ISO/CUE and runs verifier |
| Emulator IP probe | Passing | BlastEm + USA BIOS + SGDK GDB hit `$00FF0000` and read US security bytes |
| Megadev dual-CPU control | Passing | `tools/build_megadev_dualcpu_control.ps1` + `-Probe MegadevControl` proves the builder/security path can boot a Megadev-shaped SP and report from Sub code |
| Dual-CPU status probe | Passing | `BOOT_PROBE=1` + `-Probe DualCpuStatus` proves SegaOS Main boot, Sub `sp_init`/`sp_main`, and command/status round trip |
| Strict Word RAM probe | Passing | `-Probe DualCpu` proves Sub bank-0 write, 1M RET clear, and Main `$200000` visibility with `0xa55a/0x5aa5` |
| Framebuffer probe | Passing | `-Probe Framebuffer` proves Sub 4bpp pattern, 1M RET clear, Main Word RAM readback, `FB_UpdateFrame()`, and VDP VRAM tile-0 readback; the visible probe is confirmed by BlastEm internal screenshot |
| Runtime smoke probe | Passing | `SUB_RUNTIME_SMOKE=1` + `-Probe RuntimeSmoke` proves normal C SP startup and command handshake without desktop modules |
| Boot-safe desktop render probe | Passing | `DESKTOP_INIT_PROBE=1` + `-Probe DesktopInit` proves real boot-safe C SP first render command and Main upload path |
| Direct VDP text canary | Passing | `VDP_TEXT_PROBE=1` + `-Probe VdpText` proves SGDK-derived 8x8 glyph tile upload, VRAM readback `0x00ff/0xff00`, Plane A entries `0x0001/0x0002/0x0003`, and a readable internal screenshot |
| Desktop scaled text isolation | Passing | `DESKTOP_INIT_PROBE=1 BOOT_SAFE_TEXT_PROBE=1` proves the first scaled SGDK-font "S" as row sample `0xffff/0xff11`, full-glyph signature `0xd2dd`, Plane A entries `0x0198/0x0199/0x019a`, and readable desktop-compositor screenshot `C:\tmp\segaos_screens_internal\segaos_desktop_text_opaque_20260630_183441.png` |
| Default text/title render isolation | Passing | `DESKTOP_INIT_PROBE=1 BOOT_SAFE_TITLE_PROBE=1` proves sampled default SGDK-font body text as `0xf11f/0x1f11` in both Word RAM and VDP tile data |
| Desktop repeated-frame probe | Passing | `DESKTOP_REPEAT_PROBE=1` + `-Probe DesktopRepeat` proves a second boot-safe `CMD_RENDER_FRAME` after Main returns Word RAM; terminal phase `0x82ff`, repeat status `0x0003`, trace `0x7404`, MEM_MODE `0x2a06`, repeated title VRAM `0xf11f/0x1f11` |
| Desktop short loop probe | Passing | `DESKTOP_LOOP_PROBE=1` + `-Probe DesktopLoop` proves four additional single-bank render/upload/return cycles after the first frame; terminal phase `0x83ff`, loop count `0x0004`, status `0x0003`, trace `0x7404`, MEM_MODE `0x2a06`, final title VRAM `0xf11f/0x1f11` |
| Desktop upload timing probe | Passing | `DESKTOP_TIMING_PROBE=1` + `-Probe DesktopTiming` proves 7 strip DMA transfers, HV movement on every strip, DMA clear after every strip, terminal phase `0x84ff`, HV `0xbc1d` to `0xfdb2`, final VDP status `0x320c`, masks `0x007f/0x007f`, and `probe_gdb_timeout=False` |
| Desktop WM allocation/render probe | Passing | `DESKTOP_WM_PROBE=1` + `-Probe DesktopWm` proves one `WM_NewWindow()` document window through z-order and dirty-window clipping; window count `0x0001`, flags `0x0007`, frame origin `0x2822`, trace `0x7404` |
| Desktop WM visual capture | Passing | `DESKTOP_WM_PROBE=1 BOOT_SAFE_VISUAL_PROBE=1` + debugger-backed BlastEm internal screenshot captures readable WM-backed title/body text at `C:\tmp\segaos_screens_internal\segaos_wm_probe_20260630_235603.png` |
| Desktop dirty queue upload probe | Passing | `DESKTOP_DIRTY_QUEUE_PROBE=1` + `-Probe DesktopDirtyQueue` proves one queued 32-byte tile upload through `FB_UpdateTileQueue()`; terminal phase `0x85ff`, tile `0x0147`, queue bytes `0x0020`, WRAM `0xf11f/0x1f11`, VRAM `0xf11f/0x1f11` |
| BASIC internal-BRAM runtime probe | Passing | `BASIC_BRAM_PROBE=1` + `-Probe BasicBram` proves live Sub BIOS internal BRAM access in BlastEm: formatted status `0x0003`, 2 total/free 4K blocks before the write, `SAVE`/`LOAD` summary `0x0101`, loaded line/target summary `0x0211`, and terminal trace `0x75ff` |
| Host tests | Passing | `make host-tests` covers dirty-rect clipping, half-open intersection, root/window redraw planning, subtraction strips, edge-touch merge, corner-touch separation, overflow collapse, 8x8 tile range mapping, dirty tile transfer budgeting, dirty tile upload queue planning, BRAM BIOS wrapper contract behavior, internal BRAM BIOS adapter callback routing, BASIC internal-BRAM storage bridge and smoke behavior, BASIC program-buffer parsing/token storage/replacement/deletion/decoding plus binary image export/import, shell line entry/LIST/NEW/RUN/SAVE/LOAD, BASIC storage adapter routing through the save-target policy, integer/string expression evaluation, sequential PRINT/END execution, GOTO target resolution and step-limit handling, A-Z integer `LET` variables and runtime expression lookup, integer `IF`/`THEN` branching, callback-backed integer `INPUT`, fixed-depth `GOSUB`/`RETURN`, framebuffer tile-span conversion, dirty-queue upload chunking, storage save-target policy, and the fake-GDB timeout regression for the BlastEm probe harness |
| Default visual capture | Passing | `BOOT_SAFE_VISUAL_PROBE=1` + `tools\capture_blastem_internal_screenshot.ps1 -DebugAutoBoot -InputMode PostMessage -StartKey Enter -ScreenshotKey P` proves the default desktop frame reaches `segaos_visual_probe_halt` phase `0x76ff` and captures readable menu/title/body text through BlastEm internal screenshotting at `C:\tmp\segaos_screens_internal\segaos_repeat_20260630_231605.png` |

## Toolchain
- SGDK m68k-elf-gcc (C:\SDKS\SGDK\bin\)
- Both CPU C builds use `-ffreestanding -fno-builtin`; without this, m68k GCC
  can rewrite SegaOS' local libc primitives into unsafe hosted/builtin calls
- Direct ld.exe linking (bypasses LTO plugin issue)
- libgcc.a for runtime math helpers
- Current ISO builder/verifier path uses Python standard-library scripts
- The Makefile prefers local Python 3.12/3.14 before falling back to `python`
- Megadev 1.2.0 is the primary Sega CD boot-layout reference

## Key Metrics
- Work RAM usage: Main CPU IP remains within the 0xE00 boot-sector envelope
  after the regional security block is linked first
- PRG-RAM usage: 11,808 text bytes / 2,292 BSS bytes observed locally for the
  default boot-safe Sub CPU SP binary; the unreferenced BASIC core is
  compiled for target but dead-stripped from the boot path
- BOOT_PROBE SP usage: 1,042 text bytes, intentionally below Megadev's 16KB
  default SP window
- BOOT_PROBE SP layout: Megadev-style `SUBALIGN(2)`, `sp_init` at `$602A`,
  `sp_main` at `$607E`, `_TEXT_LENGTH = $0412`
- Boot-safe desktop SP usage: 11,808 text bytes / 2,292 BSS bytes observed
  locally with `BOOT_SAFE_DESKTOP=1`
- Boot-safe text probe SP usage: 9,248 bytes observed locally with
  `DESKTOP_INIT_PROBE=1 BOOT_SAFE_TEXT_PROBE=1`
- Boot-safe title/default-text probe SP usage: 11,836 text bytes observed locally with
  `DESKTOP_INIT_PROBE=1 BOOT_SAFE_TITLE_PROBE=1`
- Boot-safe repeated-frame probe usage: Main IP 3,568 bytes / SP 11,836 text
  bytes observed locally with `DESKTOP_REPEAT_PROBE=1`
- Boot-safe loop probe usage: Main IP 3,560 bytes / SP 11,836 text bytes
  observed locally with `DESKTOP_LOOP_PROBE=1`
- Boot-safe timing probe usage: Main IP 3,460 bytes / SP 11,836 text bytes
  observed locally with `DESKTOP_TIMING_PROBE=1`
- Boot-safe WM probe usage: Main IP 3,304 bytes / SP 13,118 text bytes,
  2,298 BSS bytes observed locally with `DESKTOP_WM_PROBE=1`
- Boot-safe WM visual probe usage: Main IP 3,320 bytes / SP 13,118 text bytes,
  2,298 BSS bytes observed locally with
  `DESKTOP_WM_PROBE=1 BOOT_SAFE_VISUAL_PROBE=1`
- Boot-safe dirty queue probe usage: Main IP 3,556 bytes / SP 11,836 text
  bytes observed locally with `DESKTOP_DIRTY_QUEUE_PROBE=1`; this diagnostic
  Main build uses `-Os` and skips mouse init plus boot checker fill to stay
  under the 3,584-byte IP boot-sector limit while preserving the
  `FB_UpdateTileQueue()` call path
- BASIC internal-BRAM probe usage: Main IP 3,112 text bytes / 5,168 BSS bytes
  and SP 24,446 text bytes / 15,318 BSS bytes observed locally with
  `BASIC_BRAM_PROBE=1`; the probe remains opt-in because it intentionally links
  the BASIC runtime, storage adapter, BRAM bridge, and live Sub BIOS adapter
  into the boot SP
- Boot-safe visual-probe IP usage: 2,832 text bytes last observed with
  `BOOT_SAFE_VISUAL_PROBE=1`; SP now follows the 11,808-byte boot-safe desktop
  payload
- Direct VDP text probe IP usage: 2,704 text bytes observed locally with
  `VDP_TEXT_PROBE=1`; SP remains the boot-safe payload but is not
  part of the probe path
- Default Main IP usage after exposing `FB_ConvertTileSpan()`: 2,856 text bytes
  observed locally with a forced normal `make iso`
- Main CPU stack: now capped at `$FFF700`, below the Main BIOS work/system-use region
- Strip buffer: 5,120 bytes (4 tile-rows at a time)
- Framebuffer: 35,840 bytes @ 4bpp (320x224)
- Dirty tile budget baseline: full 40x28 tile frame = 1,120 tiles / 35,840
  bytes, which exceeds the 7,524-byte NTSC VBlank reference budget; a 2x2-tile
  dirty range = 4 tiles / 128 bytes and fits that same budget; the queue
  planner slices a full-frame request to 235 tiles / 7,520 bytes for one NTSC
  VBlank-budgeted pass
- Sub CPU blitter default: 4bpp, matching the Main CPU tile conversion path
- Disc image: 150 cooked sectors, `MODE1/2048`, 32KB boot/system area
- Storage planning assumption: CD-ROM/ISO9660 is the read-only app/resource
  source; external Backup RAM cartridge-class storage is the primary target for
  user documents, BASIC programs, imported text, and small app data; internal
  8 KB Backup RAM is only the fallback for preferences, launch state, and tiny
  emergency saves until a probe proves broader capacity and behavior
- Storage policy seam: `STG_PlanSave()` is host-tested. It prefers external
  cart saves, allows internal BRAM only for prefs/tiny text/BASIC fallback
  saves, enforces reserve space, and rejects image documents without the
  external cart path. Internal BRAM Sub BIOS vector calls now exist behind
  `BramBiosOps`; external-cart capacity probes are still pending.
- BRAM wrapper seam: `BRM_Probe()`, `BRM_ReadFile()`, `BRM_WriteFile()`, and
  `BRM_ReadDirectory()` are host-tested against injectable BIOS operations.
  The contract follows Megadev's MIT BRAM definitions at
  `7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`: `BURAM` vector `$005F16`,
  4KB `BRMINIT`/`BRMSTAT` units, normal 0x40-byte file blocks, 11-byte
  NUL-terminated BIOS filenames, and 16-byte directory entries. The
  target-side adapter is `BRM_InitInternalBiosOps()`, backed by
  `src/sub/bram_bios_68k.s` raw calls for `BRMINIT`, `BRMSTAT`,
  `BRMSERCH`, `BRMREAD`, `BRMWRITE`, and `BRMDIR`.
- BASIC storage adapter seam: `BAS_StorageInitAdapter()` and
  `BAS_StorageBindIO()` are host-tested as the first bridge from BASIC
  `SAVE`/`LOAD` callbacks to `STG_PlanSave()`. `SAVE` plans a BASIC document
  before invoking the selected-volume write callback; `LOAD` reads from the
  external cart when present and falls back to internal BRAM only when the cart
  is absent.
- BASIC internal-BRAM bridge seam: `BAS_BramStorageInit()`,
  `BAS_BramStorageProbeInternal()`, `BAS_BramStorageWrite()`, and
  `BAS_BramStorageRead()` bind the generic BASIC storage callbacks to
  `BRM_ReadFile()`/`BRM_WriteFile()` for the internal Backup RAM volume. The
  current shell scope uses the fixed BIOS-safe `BASIC` filename. Writes are
  padded to 0x40-byte normal BRAM blocks from a static 2 KB scratch buffer;
  reads may report the padded block count, which the `SBAS` importer accepts
  because the image header defines the actual required bytes.
- BASIC internal-BRAM smoke seam: `BAS_RunBramSmoke()` is host-tested against
  fake `BramBiosOps` and target-proven through `BASIC_BRAM_PROBE=1`. It stores
  a two-line BASIC program, routes shell `SAVE`/`LOAD` through the policy
  adapter and internal BRAM bridge, verifies the loaded lines and image hash,
  and reports a compact result through Main CPU GDB variables. The target probe
  uses `BASPROBE` rather than the user-facing `BASIC` filename and does not
  format BRAM when `BRMINIT` reports unformatted storage.
- BASIC program-buffer/shell seam: `BAS_StoreSourceLine()`,
  `BAS_SubmitConsoleLine()`, and `BAS_SubmitConsoleLineWithStorage()` are
  host-tested as clean-room fixed-storage BASIC
  foundations. They parse numbered lines, tokenize the first keyword, keep
  program lines sorted, replace/delete by line number, compact caller-owned
  storage, decode stored lines, export/import a fixed-format binary program
  image, list programs through a caller-supplied sink, clear with `NEW`, and
  move program images through callback-backed `SAVE`/`LOAD` commands using
  caller-owned scratch buffers. They evaluate simple signed 16-bit integer `+`/`-`
  expressions plus quoted string literals. `BAS_RunProgram()` now executes
  stored lines sequentially for `PRINT` and `END`, reports unsupported/bad
  expression lines, routes output through the caller-supplied sink, supports
  literal-line `GOTO` with missing-target and step-limit errors, and provides a
  fixed A-Z signed 16-bit integer runtime for `LET` assignment and expression
  lookup. `IF`/`THEN` now branches on integer truth/comparison conditions to a
  literal line target or `THEN GOTO` target. `BAS_RunProgramWithIO()` adds
  callback-backed integer `INPUT` assignment for A-Z variables. `GOSUB` and
  `RETURN` use a fixed-depth local return stack with explicit missing-target,
  stray-return, and overflow errors. This is not a string-variable, array,
  desktop I/O, or persistence layer yet.

## Current Reference Baseline

Before changing boot, Word RAM, framebuffer, or desktop bring-up code, read
`LESSONS.md`; it captures emulator-proven constraints that should not be
rediscovered by trial and error.

Megadev 1.2.0:

- Repo: https://github.com/drojaazu/megadev
- Commit: `7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
- License: MIT
- Reuse mode: pattern-only for boot-layout/build references; direct-copy for
  `src/main/security.c` from Megadev `lib/security.c`; close-port/pattern-only
  for the SP module header and jump-table contract from `lib/sub/sp_header.s`;
  pattern-only/clean-room for the BRAM Sub BIOS vector ABI in
  `src/sub/bram_bios_68k.s`; control-build/pattern-only for Megadev
  `hello_world` IP/SP behavior
- Inspected files: `README.md`, `VERSION`, `LICENSE`, `docs/manual.md`,
  `docs/boot.md`, `docs/disc.md`, `docs/cdrom.md`, `docs/megacd_dev.md`,
  `docs/modules.md`, `docs/program_design.md`, `new_project/README.md`,
  `megadev.make`, `lib/cd_boot.s`, `cfg/ip.ld`, `cfg/sp.ld`,
  `lib/security.c`, `lib/sub/sp_header.s`, `lib/main/gate_arr.def.h`,
  `lib/sub/gate_arr.def.h`, `lib/main/gate_arr.h`, `lib/sub/gate_arr.h`,
  `lib/main/gate_arr.macros.s`, `lib/sub/sub.macro.s`,
  `lib/main/mmd.h`, `lib/main/mmd.macros.s`, `lib/main/memmap.def.h`,
  `lib/build.def.h`, `lib/sub/bios.def.h`, `lib/sub/bram.def.h`,
  `lib/sub/bram.h`, `lib/sub/cdboot.def.h`,
  `examples/hello_world/src/ip.s`,
  `examples/hello_world/src/sp.s`, `new_project/src/ip.s`,
  `new_project/src/sp.s`

Key adopted assumptions:

- cooked ISO9660 data track (`MODE1/2048`) is canonical for now
- boot-sector injection should match `mkisofs -G boot.bin` semantics
- header IP offset field is `$0800`, but IP bytes are physically placed at
  `$0200`
- SP bytes are physically placed at `$1000`
- system header string fields are space-padded, and US builds use
  `SEGA GENESIS` as the hardware ID to match Megadev defaults
- IP includes the selected regional security code first; `CD_REGION` defaults to
  `US` and supports `JP`/`EU`
- Main IP startup must not use the `$FFF700+` Main BIOS work/system region as a
  C stack.
- SP linker sections should keep Megadev-style 2-byte subalignment so the SP
  header and relative user-call table match the maintained reference layout.
- A permanent Megadev-derived dual-CPU control fixture proves the current image
  builder and regional security path can boot a Megadev-shaped SP and receive
  Sub-side COMSTAT breadcrumbs.
- A hybrid control-IP/SegaOS-C-runtime-SP image produced zero Sub status. That
  isolated the old failure to the SegaOS SP runtime/startup shape, so
  `BOOT_PROBE=1` now uses an assembly-only SP path for low-level handoff
  debugging.
- In the observed 1M boot state, MEM_MODE starts at `0x2a05`: RET is set and
  Sub owns the `$0C0000` bank. Clearing RET from Sub exposes that bank at Main
  `$200000`; setting RET and waiting was the wrong operation for this handoff.
- The repeated desktop probe shows a different Main-released 1M state:
  after Main writes RET to release the displayed bank, BlastEm reports
  `MEM_MODE=0x2a06` (DMNA set, RET clear). The boot-safe Sub-side ownership
  check now accepts either RET or DMNA, and `DESKTOP_REPEAT_PROBE=1` proves two
  render/upload cycles through the same single-bank path.
- `DESKTOP_LOOP_PROBE=1` proves the same single-bank release/reacquire path can
  survive a short loop of four additional render/upload/return cycles after the
  first frame, with the final title row still present in VDP tile data.
- `DESKTOP_TIMING_PROBE=1` proves the current full-frame upload path runs as 7
  strip DMAs and gives the first HV/status measurement for frame-policy work:
  current BlastEm sample HV `0xbc1d` to `0xfdb2`, final status `0x320c`, and
  transition/DMA-clear masks `0x007f/0x007f`.
- The framebuffer probe uses that handoff for a deterministic 4bpp tile:
  Sub writes `$1234/$5678` to linear row 0 and `$9abc/$def0` to row 1, Main
  reads those words through `$200000`, runs `FB_UpdateFrame()`, and reads the
  same words back from VDP VRAM tile 0. The visible probe variant fills the
  frame with the same alternating row pattern and has been captured through
  BlastEm's internal screenshotting.
- Main-to-Sub commands use CFM only as a pending signal (`COMM_MAIN_PENDING`,
  currently `0x02`). The opcode is stored in `CMD0`, and parameters are stored
  in `CMD1`-`CMD4`. Both the C command loop and the assembly `BOOT_PROBE` path
  follow this protocol.

SGDK font source:

- Repo: https://github.com/Stephane-D/SGDK
- Commit: `ef9292c03fe33a2f8af3a2589ab856a53dcef35c` (`v2.11`)
- License: MIT, copied to `third_party/sgdk_font/LICENSE.txt`
- Reuse mode: direct-copy, format-converted from indexed PNG tiles to 1bpp
  `Glyph` rows in `src/sub/sysfont.c` and limited direct VDP canary rows in
  `src/main/vdp_text_probe.c`
- Inspected files: `license.txt`, `res/libres.res`,
  `res/image/font_default.png`, `inc/font.h`, `src/vdp.c`
- Adopted assumption: SegaOS' current system font should come from this real
  8x8 tile font until a deliberate, licensed Mac-like bitmap font is selected.

SGDK DMA queue source:

- Repo: https://github.com/Stephane-D/SGDK
- Commit: `ef9292c03fe33a2f8af3a2589ab856a53dcef35c` (`v2.11`)
- License: MIT
- Reuse mode: pattern-only / clean-room. No SGDK DMA queue source is copied or
  closely ported into SegaOS.
- Inspected files: `inc/dma.h`, `src/dma.c`, `src/sys.c`
- Adopted assumption: SegaOS' VDP update path should use caller-owned static
  queue storage, explicit capacity and byte-budget checks, and a later VBlank
  flush point. The current `DirtyTileQueue` is only the host-tested planner for
  those upload spans.

68k desktop prior art:

- EmuTOS:
  `emutos/emutos@bc34e0b7e7850c5c45a909409d11f7c75ecdc881`, GPL-family,
  pattern-only. Inspected AES window/redraw/object code, VDI text/line code,
  desktop code, and memory docs.
- FreeMiNT/XaAES:
  `freemint/freemint@3172633539fb4281bc3b23c322892f565f303c16`, mixed
  GPL/MiNT-family terms, pattern-only. Inspected kernel init notes and XaAES
  redraw/event change history.
- OpenGEM:
  `shanecoughlan/OpenGEM@ac06b1a3fec3f3e8defcaaf7ea0338c38c3cef46`, GPL for
  GEM/FreeGEM/OpenGEM sections, historical/API pattern-only.
- PC/GEOS:
  `bluewaysw/pcgeos@867154f966314155fdc2ee04593b21c0a5f6e724`, GitHub
  reports Apache-2.0. Candidate golden source only; no file-level porting pass
  has been done yet.
- CP/M-68K and reverse-engineered GEOS sources:
  golden historical references, but not pinned for implementation reuse yet.
  Do not copy or closely port until repo/source, commit, license, inspected
  files, and reuse mode are recorded.
- Adopted architectural assumption: prove a small VDI-like text/clipping layer,
  then an AES-like dirty-rect/window ownership layer, then desktop/window/app
  behavior. The current block `OS` canary is diagnostic evidence, not the final
  text system.

## Key Classes / Modules
| Module | CPU | File | Purpose |
|--------|-----|------|---------|
| Blitter | Sub | `src/sub/blitter.c` | Software framebuffer renderer |
| Window Manager | Sub | `src/sub/wm.c` | Mac-style window management |
| Dirty Rects | Sub/host | `src/sub/dirty_rect.c` | Host-tested dirty-region clipping, merging, subtraction, tile-range mapping, transfer-budget planning, and upload queue span planning |
| Memory Manager | Sub | `src/sub/mem.c` | Handle-based allocation |
| Storage Policy | Sub/host | `src/sub/storage.c` | Host-tested save-target policy for external Backup RAM cart preference and internal BRAM fallback limits |
| BRAM Wrapper | Sub/host | `src/sub/bram.c` | Host-tested BRAM BIOS contract wrapper for probe/stat/read/write/directory semantics through injectable ops |
| BRAM BIOS Adapter | Sub/host | `src/sub/bram_bios.c`, `src/sub/bram_bios_68k.s` | Host-tested callback binding plus target-assembled raw Sub BIOS `$005F16` calls for internal Backup RAM |
| BASIC Storage Adapter | Sub/host | `src/sub/basic_storage.c` | Host-tested bridge from BASIC `SAVE`/`LOAD` byte callbacks to `STG_PlanSave()` and selected-volume read/write callbacks |
| BASIC BRAM Storage | Sub/host | `src/sub/basic_bram_storage.c` | Host-tested bridge from BASIC storage callbacks to internal BRAM read/write operations using fixed filename and block-padded writes |
| BASIC BRAM Smoke | Sub/host | `src/sub/basic_bram_smoke.c` | Host-tested and BlastEm-proven smoke seam for BASIC `SAVE`/`LOAD` over live internal BRAM via the BRAM BIOS adapter |
| BASIC Core | Sub/host | `src/sub/basic.c` | Clean-room fixed-storage BASIC program buffer and shell/evaluator/runner seam with numbered-line parsing, keyword tokenization, sorted insert/replace/delete, compaction, decode, binary image export/import, line entry, `LIST`, `NEW`, callback-backed `SAVE`/`LOAD`, simple integer/string values, sequential `PRINT`/`END`, literal-line `GOTO`, fixed A-Z integer `LET` variables, integer `IF`/`THEN` branching, callback-backed integer `INPUT`, and fixed-depth `GOSUB`/`RETURN` |
| Mouse Driver | Main | `src/main/mouse.c` | Mega Mouse hardware polling |
| Framebuffer | Main/host | `src/main/framebuffer.c` | Linear-to-tile conversion seam, dirty queue upload consumer, and Main-side DMA pipeline |
| VDP | Main | `include/vdp.h` | Standalone VDP register interface |
| VDP text probe | Main | `src/main/vdp_text_probe.c` | Main-only SGDK 8x8 tile text canary |

## Warnings (non-blocking)
- `LOAD segment with RWX permissions` -- linker script refinement needed

## Active Risks

High priority:

- The verifier checks known JP/US/EU security prefixes and license markers, but
  this is still not a substitute for emulator and hardware/ODE validation.
- The current dual-CPU probes prove Main boot, Sub `sp_init`/`sp_main`, Gate
  Array command/status, one-way Sub bank-0 Word RAM return, framebuffer-to-VDP
  tile readback, a visible deterministic framebuffer display, and the
  boot-safe desktop's short single-bank render loop plus first VDP upload
  timing sample. They do not yet prove the full alternating double-buffer
  policy or long-running frame scheduler.
- The boot-safe C desktop path now reaches Sub-ready, completes a first
  render/upload sequence, and displays a visible Mac-like starter frame through
  BLT. `WM_DrawDesktop()` now owns the checker desktop/menu shell, while the
  starter window stays a compact boot-safe BLT rectangle renderer. Plain body
  text rendering now uses a real SGDK-derived 8x8 font. The Main-only direct
  VDP tile path is visually accepted through `VDP_TEXT_PROBE=1` and
  `-Probe VdpText`; the desktop scaled-text path is also GDB-proven at the Word
  RAM, VDP tile, and Plane A levels via `BOOT_SAFE_TEXT_PROBE=1` and visually
  accepted through the readable `segaos_desktop_text_opaque_20260630_183441.png`
  capture. The key lesson is that VDP background-plane color index 0 is
  transparent, so framebuffer black ink must be a nonzero palette index.
  The default boot-safe path now restores a real-font menu/title/body starter
  frame without using `WM_NewWindow()` allocation or app callbacks, and
  `BOOT_SAFE_TITLE_PROBE=1` proves a sampled default text row through both Word
  RAM and VDP tile readback. `BOOT_SAFE_VISUAL_PROBE=1` now also gives a
  debugger-backed BlastEm internal screenshot of the default frame, so a BIOS
  screen or old block-canary capture is no longer acceptable visual evidence.
  BLT framebuffer access and Main framebuffer upload both use 16-bit Word RAM
  helpers. The dirty-rectangle/clipping pool now has host tests for both root
  and window redraw planning, 8x8 tile-range mapping, tile transfer budgeting,
  dirty upload queue span generation, framebuffer tile-span conversion, and
  dirty-queue upload chunking; it is wired into `WM_InvalidateRect()` and is
  used by the
  boot-safe direct renderer's dirty loop. The narrow real
  `WM_NewWindow()` allocation/z-order render probe and its debugger-backed
  visual screenshot are now green, and the short single-bank render/upload loop
  is now GDB-proven. The HV/status timing probe starts the frame-budget work,
  but the long-running frame scheduler is still separate stability work before
  menu/cursor/app rendering.
- The active boot decision has narrowed: keep the assembly probe as the
  low-level truth source, keep the boot-safe direct renderer as the startup
  path, and reintroduce BLT/window-manager drawing behind probe-proven command
  and Word RAM handoff invariants.

Runtime validation:

- Word RAM bank ownership is proven for the first Sub `$0C0000` to Main
  `$200000` handoff, for a second boot-safe render after Main releases the same
  bank, and for a short loop of repeated single-bank renders. Alternating 1M
  double buffering is still unresolved: the proven path is a single-bank
  bring-up loop, not the final frame scheduler.
- Full-frame conversion/DMA is acceptable for bring-up, but needs a VDP timing
  policy before the desktop loop can be considered stable. The first timing
  probe measures the current full-frame upload shape; `DR_TileRangeBudget()`
  and `DR_QueueTileRange()` now prove the dirty-region side can report
  tile/byte/span costs and build bounded upload spans, and
  `FB_ConvertTileSpan()` plus `FB_FlushTileQueueWithCallback()` prove the Main
  side can convert and chunk those spans into VDP upload units. The
  `FB_UpdateTileQueue()` hardware wrapper now exists, but the default desktop
  loop does not call it yet; the live VBlank policy, active-display policy, and
  double-buffer policy remain open.
- The `DesktopTiming` probe script now has a bounded failure path around the
  GDB `continue` phase. `tools/probe_blastem_boot.ps1` accepts
  `-GdbTimeoutSeconds`, reports `probe_gdb_timeout=True` on timeout, and
  `make host-tests` covers that path with generated fake BlastEm/GDB
  executables. The fresh real `DesktopTiming` run returns
  `probe_gdb_timeout=False`.

Lower priority:

- Some GUI draw calls need audit for width/height parameters versus endpoint
  coordinates.
- Storage features should wait until boot, CPU handshake, framebuffer, and VDP
  timing are proven.
