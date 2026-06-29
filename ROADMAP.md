# Roadmap

## Phase 1: Sub CPU Build -- COMPLETE
- [x] crt0.s with SP header and vector table
- [x] Freestanding C headers (stdint, stddef, stdbool, string)
- [x] Linker script (sub.ld) for PRG-RAM
- [x] Makefile with ld.exe direct linking + libgcc.a

## Phase 2: Main CPU VDP Pipeline -- COMPLETE
- [x] crt0.s for Work RAM entry
- [x] Linker script (main.ld) for $FF0000, with stack below the `$FFF700+`
      Main BIOS work/system-use region
- [x] Standalone VDP interface (vdp.h)
- [x] Tilemap setup (40x28 sequential tiles)
- [x] Strip-based linear-to-tile conversion
- [x] DMA pipeline (5KB strip buffer)
- [x] Win3.1 palette in MD 9-bit RGB

## Phase 3: Word RAM Bank Sync -- IMPLEMENTED, NEEDS VALIDATION
- [x] 1M mode bank swap helpers (DMNA/RET bits)
- [x] Double-buffer handshake path between Main and Sub CPU
- [x] Verify one-way Sub bank-0 return timing in emulator
- [ ] Confirm which Main CPU Word RAM window should be displayed after each swap
- [ ] DMA timing refinement (VBlank vs active display)

## Bring-Up Ladder

The next work should proceed as a debug ladder, not as broad feature phases.
Do not advance to desktop/app polish until the lower rung has a concrete
artifact and a repeatable verification step.

### Milestone A: Reproducible Boot Artifact -- COMPLETE
- [x] Identify Megadev 1.2.0 as primary permissive Sega CD boot reference
- [x] Make the disc-image build reproducible on this machine:
  - [x] avoid the Windows Store `python` alias failure
  - [x] document the chosen Python dependency
  - [x] keep `make all` as a one-command build target
- [x] Reconcile `tools/build_iso.py` with Megadev boot layout:
  - [x] Header IP offset field `$0800`
  - [x] IP bytes physically placed at `$0200`
  - [x] SP bytes physically placed at `$1000`
  - [x] 32KB boot sector/system area
  - [x] cooked ISO9660 data track (`MODE1/2048`)
- [x] Reject oversized IP/SP payloads instead of truncating silently
- [x] Make `tools/verify_disc.py` fail on layout errors, not only print fields
- [x] Record upstream reference details in implementation notes:
  - repo `https://github.com/drojaazu/megadev`
  - commit `7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
  - license MIT
  - inspected files and reuse mode
- [x] Add actual regional security-code inclusion or adaptation for IP:
  - [x] direct-copy Megadev `lib/security.c` under MIT attribution
  - [x] link `.security` before Main CPU entry code
  - [x] validate JP/US/EU security prefix and license marker in
        `tools/verify_disc.py`
- [x] Emulator boot test reaches the Sega CD boot/IP path:
  - [x] BlastEm 0.6.3-pre with USA BIOS and SGDK GDB hit breakpoint
        `$00FF0000`
  - [x] `$FF0000` contained the expected US security bytes

Acceptance: a clean build produces `build/segaos.iso` and `build/segaos.cue`;
the verifier proves the Megadev-compatible boot layout and selected regional
security block; an emulator at least reaches the Sega CD boot/IP path instead
of failing disc recognition.

### Milestone B: Minimal Dual-CPU Probe -- COMPLETE
- [x] Add a repeatable BlastEm/GDB probe command or script
- [x] Add a tiny boot-probe mode or branch separate from the desktop loop
- [x] Prove Main CPU IP starts and initializes the VDP
- [x] Prove BIOS loads SP payload bytes to PRG-RAM bank 0 at `$006000`
- [x] Prove Sub BIOS can enter a Megadev-style SP `sp_init` and `sp_main`
- [x] Prove Sub CPU SP starts from `$006000`
- [x] Prove Gate Array command/status flags survive round trips
- [x] Prove Word RAM 1M bank handoff with a known byte pattern
- [x] Log or expose probe status in a way each emulator can inspect

Current evidence:

- `tools/controls/megadev_dualcpu` is a permanent Megadev-derived control
  fixture. Built through SegaOS's Python ISO builder, it reaches
  `megadev_control_halt` in BlastEm and reports Sub-side breadcrumbs:
  `SP/IN`, `MA/IN`, and trace `0x1202`.
- A hybrid image using the passing control IP with the older SegaOS C-runtime
  probe SP produced zero Sub status, isolating that failure to the SegaOS SP
  runtime/startup shape rather than the ISO builder, security block, or BIOS
  SP load.
- `BOOT_PROBE=1` now uses an assembly-only SP path for the probe. The latest
  visible framebuffer probe build places `sp_init` at `$602A`, `sp_main` at
  `$607E`, and `_TEXT_LENGTH = $03a2` (930 bytes).
- `tools/probe_blastem_boot.ps1 -Probe DualCpuStatus` now passes in BlastEm:
  Main magic `0x4d50`, Sub ready `0x0002/0x5244`, command status `0x0003`,
  result words `0x444e/0xa55a/0x5aa5`, and trace `0x52fe`.
- `tools/probe_blastem_boot.ps1 -Probe DualCpu` now passes the strict Word RAM
  rung. Sub writes `0xa55a/0x5aa5` to its `$0C0000` bank, clears RET in 1M
  mode, and Main reads the same words at `$200000`; MEM_MODE moves from
  `0x2a05` to `0x2a04` and the probe ends in phase `0x00ff`.
- Main stack placement was a real bug and is fixed: the stack is now below
  `$FFF700` instead of wrapping through `$01000000` to the BIOS/system area.
- A prior attempt to set RET and wait for Main-side Word RAM visibility timed
  out in BlastEm. The corrected 1M handoff is the opposite operation for the
  observed boot state: clear RET from Sub after writing bank 0. The production
  double-buffer policy still needs to decide how/when Sub switches away from
  `$0C0000` for subsequent frames.

Acceptance: the probe produces an unambiguous pass/fail signal for Main boot,
Sub boot, inter-CPU flags, and Word RAM bank ownership.

### Milestone C: Framebuffer Probe
- [x] Make 4bpp the canonical runtime path until a 2bpp converter exists
- [x] Change or gate the Sub CPU blitter default so it matches Main 4bpp display
- [x] Render a deterministic Sub CPU test pattern into Word RAM
- [x] Convert that pattern through the Main CPU tile path
- [x] Verify VDP VRAM tile readback against the expected 4bpp layout in emulator
- [x] Verify visible pixels against the expected 4bpp layout in emulator
- [ ] Decide whether `WRAM_BANK0_MAIN`, `WRAM_BANK1_MAIN`, or an alternating
      pointer is correct after each swap
- [x] Add a conservative single-bank return path after Main uploads a frame
- [x] Prove the boot-safe C desktop SP publishes Sub-ready in emulator
- [x] Prove boot-safe C desktop first `CMD_RENDER_FRAME` completes in emulator

Current evidence: `tools/probe_blastem_boot.ps1 -Probe Framebuffer` passes in
BlastEm. Sub writes a deterministic full-screen 4bpp stripe pattern into
`$0C0000`, clears RET, Main reads `0x1234/0x5678` from `$200000`, reads
`0x9abc/0xdef0` from the next linear row, runs
`FB_UpdateFrame(WRAM_BANK0_MAIN)`, and reads the same two tile rows back from
VDP VRAM tile 0. A visible probe build with
`BOOT_PROBE=1 BOOT_PROBE_FRAMEBUFFER=1` was also captured with BlastEm's
internal screenshot hotkey after pressing START, proving the expected pattern
reaches the displayed frame.

Additional evidence: `SUB_RUNTIME_SMOKE=1` + `-Probe RuntimeSmoke` proves the
normal C SP startup path without desktop modules. `DESKTOP_INIT_PROBE=1` +
`-Probe DesktopInit` proves the real boot-safe desktop SP reaches `sub_main`,
handles a first `CMD_RENDER_FRAME`, and lets Main upload the returned Word RAM
frame. The default build now displays a visible checker desktop/menu/window
outline starter frame through BLT's word-safe framebuffer backend. The captured
title/text attempt at
`C:\tmp\segaos_screens_internal\segaos_internal_20260629_171815.png` was visibly
corrupted, so title/text drawing has been backed out of the boot-safe starter.
`WM_DrawDesktop()` supplies the boot-safe checker desktop/menu shell, while the
starter window intentionally remains compact rectangle drawing. An attempt to
move `WM_NewWindow()` into the boot render path regressed the Sub command loop
before command consumption, so the next rungs are isolating font/title drawing
and proving a minimal `WM_NewWindow()` render probe before enabling the full
window-manager/menu/cursor loop.

Acceptance: a known 4bpp pattern drawn by Sub CPU appears correctly through
Main CPU tile conversion and DMA, then remains stable under the chosen
repeated-frame bank policy.

### Milestone D: VDP Timing Budget
- [ ] Treat full-frame conversion as a bring-up path, not the final frame loop
- [ ] Measure conversion and DMA cost with scanline/VCounter instrumentation
- [ ] Decide the production update policy:
  - [ ] VBlank-only dirty-tile queue
  - [ ] active-display transfer with acceptable artifacts
  - [ ] display-off/full redraw only for transitions
- [ ] Tie dirty rectangles to tile-strip transfer ranges

Acceptance: the project has a documented frame budget and a transfer policy
that matches Genesis VDP constraints.

### Milestone E: Desktop and App Bring-Up
- [x] Prove the boot-safe C desktop kernel reaches Sub-ready after BIOS handoff
- [x] Display a visible boot-safe desktop starter frame in BlastEm
- [x] Re-enable BLT rectangle/pattern drawing on top of proven 4bpp output
- [x] Implement the desktop fill pattern previously stubbed in `WM_DrawDesktop`
- [ ] Re-enable the minimal window-manager render loop on top of the word-safe BLT backend
- [ ] Move menu/apps out of the boot SP or load them after the boot-safe kernel
- [ ] Verify menu bar, cursor, window frames, Calculator, Notepad, keyboard, and Paint
- [ ] Audit line-drawing call sites for width/height versus endpoint confusion
- [ ] Isolate and prove a minimal `WM_NewWindow()` boot render probe
- [ ] Validate mouse input -> window hit testing -> app callback flow

Acceptance: a full desktop session can be booted, rendered, and interacted
with in at least one emulator.

### Milestone F: Storage and OS Features
- [ ] Add Sub CPU CD-ROM file access plan and API
- [ ] Add BRAM wrappers for settings/documents
- [ ] Decide app/resource packaging format on ISO9660
- [ ] Add CD audio or PCM strategy only after storage ownership is clear

Acceptance: applications can load or save at least one small asset/document
through the chosen Sega CD storage path.

### Milestone G: Compatibility and Release Hardening
- [ ] ares v148+ boot and runtime test
- [ ] BlastEm boot/debug test if available
- [ ] Genesis Plus GX / Picodrive sanity check if available
- [ ] Real hardware or ODE validation before treating disc format as release-safe
- [ ] Clean up linker warnings (RWX permissions)
- [ ] Resolve symbol redefinition warnings
- [ ] Remove unused variables
- [ ] Preserve reference-code-first attribution for any copied or closely ported code

Acceptance: the same image boots across the target emulator set and has a
documented hardware/ODE validation result.
