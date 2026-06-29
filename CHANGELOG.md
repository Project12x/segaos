# Changelog

All notable changes to this project will be documented in this file.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added
- Added `LESSONS.md` to capture emulator-proven Sega CD bring-up lessons,
  including Megadev reference-first workflow, Word RAM ownership, and
  byte-versus-word framebuffer write constraints.
- Added Megadev's MIT regional Sega CD security block as
  `src/main/security.c`, linked first in the Main CPU IP.
- Added `CD_REGION` build selection for JP/US/EU security-code variants.
- Added `tools/probe_blastem_boot.ps1` to run the BlastEm/GDB IP breakpoint
  probe as a repeatable check.
- Added `BOOT_PROBE=1`, `include/boot_probe.h`, and a minimal dual-CPU
  BlastEm/GDB probe path that exposes Main/Sub boot state through symbols.
- Added `tools/controls/megadev_dualcpu` and
  `tools/build_megadev_dualcpu_control.ps1` as a Megadev-derived dual-CPU
  control fixture with pinned MIT reference-code notes.
- Added `DualCpuStatus`, `DualCpuWramSurvey`, `DualCpuWramRetClear`,
  `DualCpuWramSweep`, `Framebuffer`, and `MegadevControl` modes to
  `tools/probe_blastem_boot.ps1` for staged BlastEm/GDB validation.

### Changed
- Replaced the `pycdlib` ISO path with a Python standard-library cooked
  ISO9660/CUE builder that follows the Megadev-compatible Sega CD boot layout.
- Wired `tools/verify_disc.py` into `make iso` so disc-layout errors fail the
  build, including selected regional security-prefix checks.
- Advanced the roadmap from Milestone A to Milestone B after a BlastEm/GDB boot
  probe reached `$00FF0000` with US security bytes present.
- Changed the Sub CPU blitter startup default to 4bpp so it matches the Main CPU
  framebuffer conversion path.
- Tightened the ISO builder/verifier to match Megadev boot-header defaults:
  space-padded system strings, `$0100/$0000` system version/type, and
  region-specific hardware IDs (`SEGA GENESIS` for US).
- Changed the Sub CPU startup header/linker layout to use the Megadev SP module
  header and relative usercall jump table format.
- Tightened the Sub CPU linker to use Megadev-style 2-byte section subalignment
  for the SP module, placing the probe `sp_init` at `$602A`.
- Pivoted `BOOT_PROBE=1` to an assembly-only Sub CPU probe path for startup,
  command/status, and Word RAM handoff isolation; the current visible
  framebuffer probe SP is 930 text bytes.
- Added `BOOT_PROBE_FRAMEBUFFER=1` as an opt-in visible framebuffer probe path
  that fills the full 4bpp frame with a deterministic pattern.
- Added `BOOT_SAFE_DESKTOP=1` as the default Sub build shape, keeping only the
  minimal desktop kernel in the boot SP while deferring menu/apps.
- Added a reusable BlastEm internal screenshot helper at
  `tools/capture_blastem_internal_screenshot.ps1`.
- Added `SUB_RUNTIME_SMOKE=1` and `DESKTOP_INIT_PROBE=1` validation paths for
  normal C SP startup and boot-safe desktop init/render bring-up.
- Implemented `WM_DrawDesktop()` so it paints the boot-safe checker desktop and
  menu shell instead of remaining a stub.

### Fixed
- Fixed several UI border/cursor draw calls that passed ending coordinates to
  `BLT_DrawHLine`/`BLT_DrawVLine` instead of width/height.
- Removed stale Sub CPU compiler warnings for duplicate defines, duplicate
  `WORD_RAM_SIZE`, and unused local/static variables.
- Fixed the Main boot wait to poll Sub readiness across VBlank frames after
  Main-side VDP/framebuffer initialization instead of failing on an immediate
  zero status word.
- Fixed Main CPU stack placement so SegaOS starts below the `$FFF700+` Main BIOS
  work/system-use region instead of wrapping through `$01000000`.
- Fixed the Sub-side Word RAM return helper for the observed 1M boot state:
  clearing RET exposes Sub's `$0C0000` bank at Main `$200000`; setting RET and
  waiting was wrong for this handoff.
- Added a conservative Main-side bank return after `FB_UpdateFrame()` so the
  Sub CPU can own `$0C0000` again before the next render command.
- Fixed normal Sub startup readiness so READY is published from the C runtime
  path instead of early assembly callbacks.
- Fixed the C Word RAM handoff helpers to match the proven 1M RET polarity:
  Sub clears RET to expose its bank to Main, and Main sets RET to return it.
- Split boot-safe desktop initialization from rendering so `CMD_INIT_OS` only
  initializes state and `CMD_RENDER_FRAME` returns a direct Word RAM frame.
- Reworked the boot-safe startup frame to use word-safe 4bpp Word RAM drawing
  and made the default image display a visible checker desktop/menu/window
  starter frame.
- Aligned the boot-safe command wait loop with the probe-proven polling path so
  the default image consumes the first render command.
- Reworked BLT framebuffer access to route reads/writes through 16-bit Word RAM
  helpers, allowing the boot-safe starter frame to use BLT rectangle and pattern
  primitives again.
- Updated the boot-safe starter frame to draw a titled window and text through
  compact BLT title/text primitives while keeping `WM_NewWindow()` out of the
  first-render path until its command-loop regression is isolated.

### Documentation
- Updated Sega CD reference docs around Megadev 1.2.0, pinned at
  `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`.
- Added June 2026 homebrew/tooling update notes and boot-disc reconciliation
  risks.
- Updated roadmap and state docs to put boot-disc/Megadev reconciliation at the
  bottom of the bring-up ladder.
- Reworked the roadmap into a bring-up ladder: reproducible boot artifact,
  minimal dual-CPU probe, 4bpp framebuffer probe, VDP timing budget, desktop
  bring-up, storage, and compatibility hardening.
- Documented the earlier Milestone B isolation step where BlastEm reached the
  Main `segaos_probe_halt` symbol and SP bytes were loaded at `$006000`, while
  the C-runtime SP path still produced no Sub breadcrumbs.
- Documented that a Megadev `hello_world` control image built through the
  SegaOS ISO builder reached the Megadev Main IP loop, narrowing the next
  control to a dual-CPU Sub breadcrumb fixture.
- Updated Milestone B docs after the dual-CPU control and SegaOS
  `DualCpuStatus` probe proved Sub startup and Gate Array command/status,
  narrowing the next investigation to Word RAM 1M ownership.
- Updated Milestone B docs again after `DualCpu` proved one-way Word RAM
  bank-0 handoff. The active rung is now the deterministic 4bpp framebuffer
  probe and repeated-frame double-buffer policy.
- Updated Milestone C docs after `-Probe Framebuffer` proved deterministic
  4bpp Word RAM readback and VDP VRAM tile-0 readback through `FB_UpdateFrame()`.
- Updated Milestone C docs again after a visible framebuffer probe was captured
  with BlastEm's internal screenshotting.
- Documented the earlier stage where the boot-safe C desktop SP still failed
  the Sub-ready gate in BlastEm and the assembly framebuffer probe remained the
  known-good runtime proof.
- Documented that runtime smoke and boot-safe desktop render probes now pass,
  and that BLT's byte-oriented framebuffer writes are isolated from the boot
  path until Word RAM-safe rendering is designed.
- Added lessons from the word-safe BLT fix and updated current-state docs with
  the latest BLT-backed screenshot evidence.
- Documented the latest titled/text boot-safe desktop screenshot and the
  `WM_NewWindow()` bring-up risk.

## [0.1.0] - 2026-02-10

### Added
- Sub CPU build infrastructure (crt0.s, sub.ld, Makefile rules)
- Main CPU build infrastructure (crt0.s, main.ld, Makefile rules)
- Freestanding C headers (stdint.h, stddef.h, stdbool.h, string.h)
- Standalone VDP interface (vdp.h) -- no SGDK libmd dependency
- Strip-based framebuffer-to-tile conversion pipeline (framebuffer.c)
- Windows 3.1 16-color palette in MD 9-bit RGB
- Gate Array boot sequence for Sub CPU program loading
- Mega Mouse driver with absolute position tracking
- Input event system (Main -> Sub CPU via comm registers)
- Software blitter with 2bpp and 4bpp modes
- Window manager with drag, resize, z-order
- Applications: Notepad, Calculator, Virtual Keyboard, Paint
- Memory manager (handle-based, Mac OS style)
