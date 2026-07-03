# Roadmap

## Phase 1: Sub CPU Build -- COMPLETE
- [x] crt0.s with SP header and vector table
- [x] Freestanding C headers (stdint, stddef, stdbool, string)
- [x] Freestanding compiler contract (`-ffreestanding -fno-builtin`) so local
      libc primitives are not optimized into hosted-library/builtin calls
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
- [x] Windows-like palette in MD 9-bit RGB, with VDP index 0 reserved

## Phase 3: Word RAM Bank Sync -- IMPLEMENTED, NEEDS VALIDATION
- [x] 1M mode bank swap helpers (DMNA/RET bits)
- [x] Double-buffer handshake path between Main and Sub CPU
- [x] Verify one-way Sub bank-0 return timing in emulator
- [x] Verify two-frame single-bank return/reacquire timing in emulator
- [x] Verify short multi-frame single-bank render/upload/return timing in emulator
- [x] Add first HV/status timing probe for the full-frame upload path
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
  `$607E`, and `_TEXT_LENGTH = $0412` (1,042 bytes).
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
- [x] Prove the conservative single-bank path can complete two boot-safe
      render/upload cycles
- [x] Prove the conservative single-bank path can complete a short multi-frame
      boot-safe render/upload loop
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
starter frame with real SGDK-font menu, title, and body text through BLT's
word-safe framebuffer backend. The older block-canary capture is retained at
`C:\tmp\segaos_screens_internal\segaos_default_20260629_211333.png`; the
current accepted default capture is
`C:\tmp\segaos_screens_internal\segaos_repeat_20260630_231605.png`,
created by the `BOOT_SAFE_VISUAL_PROBE=1` / `-DebugAutoBoot` path after GDB
proved `segaos_visual_probe_halt` phase `0x76ff`.
The captured striped title/body-text attempt at
`C:\tmp\segaos_screens_internal\segaos_internal_20260629_171815.png` remains the
known-bad visual reference, so body text and striped title styling stay opt-in.
`WM_DrawDesktop()` supplies the boot-safe checker desktop/menu shell, while the
starter window intentionally remains compact rectangle drawing. An attempt to
move `WM_NewWindow()` into the boot render path regressed the Sub command loop
before command consumption. The proven lower rungs are now real fixed-font text,
dirty rectangles/clipping, root desktop redraw, and direct boot-safe window
furniture. The fixed-font rung uses SGDK v2.11's MIT `font_default.png`,
converted into SegaOS'
1bpp glyph format and proven through the DesktopInit text probe; its scaled
visual presentation is now readable in the opt-in compositor probe after
reserving VDP palette index 0 for transparency/backdrop and using index 1 for
opaque black ink. A lower
`VDP_TEXT_PROBE=1` rung now bypasses Sub CPU and Word RAM, uploads SGDK-derived
8x8 glyph tiles directly to VDP VRAM, proves `SEGAOS` / `TEXT OK` through
`tools/probe_blastem_boot.ps1 -Probe VdpText`, and has a readable BlastEm
internal screenshot at
`C:\tmp\segaos_screens_internal\segaos_vdp_text_direct_20260630_181733.png`.
`DESKTOP_REPEAT_PROBE=1` + `-Probe DesktopRepeat` now proves the conservative
single-bank repeat path: after the first upload and Main release, the Sub CPU
consumes a second `CMD_RENDER_FRAME`, returns status `0x0003` and trace
`0x7404`, Main observes the released MEM_MODE state as `0x2a06`, and the
repeated title row reads back from VDP as `0xf11f/0x1f11`.
`DESKTOP_LOOP_PROBE=1` + `-Probe DesktopLoop` now proves a short single-bank
loop: after the first frame, Main drives four additional render/upload/return
cycles, reaches phase `0x83ff`, counts four loop frames, keeps status
`0x0003` and trace `0x7404`, observes MEM_MODE `0x2a06`, and still reads the
final title row from VDP as `0xf11f/0x1f11`.
`DESKTOP_TIMING_PROBE=1` + `-Probe DesktopTiming` now gives the first measured
VDP upload evidence: the current full-frame path runs 7 strip DMA transfers,
reaches phase `0x84ff`, records HV `0xbc1d` to `0xfdb2` in BlastEm, records
final VDP status `0x320c`, and proves every strip changed HV and ended with DMA
clear through masks `0x007f/0x007f`.
`DESKTOP_WM_PROBE=1` + `-Probe DesktopWm` now proves a minimal clean-room
window-manager boot render path. The Sub CPU runs `WM_Init()`, allocates one
`WM_NewWindow()` document window, traverses bottom-to-top z-order, clips it via
`DR_PlanWindowRedraw()`, and reports window count `0x0001`, active flags
`0x0007`, frame origin `0x2822`, and terminal trace `0x7404`. The root cause of
the earlier command-loop loss was not the WM data structure: m68k GCC compiled
the local `memset()` into a recursive self-call until the build was made
explicitly freestanding with `-ffreestanding -fno-builtin`. The combined
`DESKTOP_WM_PROBE=1 BOOT_SAFE_VISUAL_PROBE=1` image also has an accepted
debugger-backed BlastEm internal screenshot at
`C:\tmp\segaos_screens_internal\segaos_wm_probe_20260630_235603.png`.
`DESKTOP_DIRTY_QUEUE_PROBE=1` + `-Probe DesktopDirtyQueue` now proves the first
hardware-backed dirty upload rung. The Main diagnostic path poisons the sampled
title tile row in VRAM, constructs a one-entry public `DirtyTileQueue` for tile
`0x0147`, calls `FB_UpdateTileQueue()`, reaches phase `0x85ff`, and reads
`0xf11f/0x1f11` back from VRAM matching the rendered Word RAM source. The probe
is deliberately narrow and size-trimmed; it does not yet define the production
VBlank scheduler.

Acceptance: a known 4bpp pattern drawn by Sub CPU appears correctly through
Main CPU tile conversion and DMA, and the boot-safe single-bank repeat path is
proven through a short multi-frame loop. Full alternating double buffering
remains a later production policy.

### Milestone D: VDP Timing Budget
- [ ] Treat full-frame conversion as a bring-up path, not the final frame loop
- [x] Add first HV/status probe around full-frame strip conversion/DMA
- [ ] Turn HV/status samples into a documented frame-budget policy
- [x] Harden `DesktopTiming` probe automation so missing breakpoints/autoboot
      failures return a bounded failure instead of hanging the harness
- [ ] Decide the production update policy:
  - [ ] VBlank-only dirty-tile queue
    - [x] Host-tested queue planner with static storage, byte-budget slicing,
          and separate budget/overflow flags
    - [x] Host-tested `FB_ConvertTileSpan()` seam that converts planned tile
          spans from linear 4bpp framebuffer layout into VDP tile bytes
    - [x] Host-tested queue consumer that chunks planned spans through a
          caller-provided upload sink and exposes `FB_UpdateTileQueue()`
    - [x] Emulator-proven narrow `FB_UpdateTileQueue()` flush of one queued
          tile span to VRAM
    - [ ] Production VBlank-scheduled flush of queued tile spans to VRAM
  - [ ] active-display transfer with acceptable artifacts
  - [ ] display-off/full redraw only for transitions
- [x] Tie dirty rectangles to tile-strip transfer ranges
  - [x] Add host-tested dirty tile transfer budgeting:
        `DR_TileRangeBudget()` reports first tile, tile count, byte count,
        row-span count, and whether a range fits a caller-supplied frame
        budget
  - [x] Add host-tested dirty tile upload span planning:
        `DR_QueueTileRange()` and `DR_BuildTileQueueFromDirtyList()` split
        dirty ranges into row/full-width spans and stop cleanly at the caller's
        byte budget or storage capacity

Acceptance: the project has a documented frame budget and a transfer policy
that matches Genesis VDP constraints.

### Milestone E: Desktop and App Bring-Up
- [x] Prove the boot-safe C desktop kernel reaches Sub-ready after BIOS handoff
- [x] Display a visible boot-safe desktop starter frame in BlastEm
- [x] Re-enable BLT rectangle/pattern drawing on top of proven 4bpp output
- [x] Implement the desktop fill pattern previously stubbed in `WM_DrawDesktop`
- [x] Study GEM/TOS/EmuTOS/FreeMiNT/OpenGEM prior art and document the
      reference-backed VDI/AES/Desktop architecture pivot
- [x] Replace the placeholder sysfont with SGDK v2.11's MIT 8x8 font and
      record source/license provenance
- [x] Prove a real fixed-font text primitive visually and through
      direct VDP tile probes
- [x] Add a Main-CPU-only direct VDP text canary separate from Sub/Word RAM
- [x] Capture and visually accept a BlastEm internal screenshot with readable
      direct SGDK 8x8 tile text
- [x] Resolve the scaled-text transparency/corruption issue in the opt-in
      desktop compositor probe
- [x] Restore accepted text and title presentation to the default boot-safe
      desktop path
- [x] Capture and visually accept the restored default boot-safe desktop via
      debugger-backed BlastEm internal screenshotting, not BIOS/autoplay
      capture
- [x] Add a static dirty-rectangle/clipping pool with host tests before it is
      used by the boot-safe renderer
- [x] Route root desktop redraw through the dirty-rectangle/clipping contract
- [x] Prove minimal window furniture through the redraw list, without app
      content callbacks
- [x] Isolate and prove a minimal `WM_NewWindow()` boot render probe after
      dirty-rectangle/root-redraw contracts pass
- [x] Visually accept the WM-backed boot frame through debugger-backed BlastEm
      internal screenshotting
- [x] Prove a short boot-safe single-bank render/upload loop before returning
      to broad desktop/app rendering
- [x] Add a first VDP HV/status timing probe before selecting the long-running
      frame scheduler
- [ ] Re-enable the minimal window-manager render loop on top of the word-safe
      BLT backend outside the opt-in boot probe
- [ ] Move menu/apps out of the boot SP or load them after the boot-safe kernel
- [ ] Verify menu bar, cursor, window frames, Calculator, Notepad, keyboard, and Paint
- [ ] Audit line-drawing call sites for width/height versus endpoint confusion
- [x] Add an opt-in plain text render probe separate from title-bar stripes
- [x] Prove plain text probe pixels in Word RAM and VDP tile data
- [x] Prove the first scaled SGDK glyph as a full `0xd2dd` signature and prove
      Plane A maps the visible tiles to `0x0198/0x0199/0x019a`
- [x] Capture and visually accept a BlastEm internal screenshot with readable
      desktop-composited scaled SGDK-font text, not a BIOS frame or blank
      transition frame
- [x] Prove title-bar text composition pixels in Word RAM and VDP tile data
- [x] Visually accept restored default title presentation after the simpler
      fixed-font text proof passes
- [x] Add host tests for dirty-rect clipping, half-open intersection,
      deterministic subtraction strips, edge-touch merge, corner-touch
      separation, overflow behavior, 8x8 tile range rounding, and dirty tile
      transfer budgeting/upload queue planning
- [x] Add host tests for framebuffer tile-span conversion, including a span
      crossing a 40-tile row boundary
- [x] Add host tests for dirty queue upload chunking, including a 235-tile
      budgeted span split across the 5,120-byte strip buffer
- [x] Add host tests for window redraw clipping against dirty regions
- [ ] Decide whether the long-running desktop loop uses single-bank bring-up,
      alternating 1M double buffering, or a different transfer policy
- [ ] Validate mouse input -> window hit testing -> app callback flow

Acceptance: a full desktop session can be booted, rendered, and interacted
with in at least one emulator.

### Milestone F: Storage and OS Features
- [ ] Add Sub CPU CD-ROM file access plan and API
- [ ] Add Backup RAM wrappers with a three-tier storage policy:
  - [x] Host-tested save-target policy contract:
        `STG_PlanSave()` prefers external cart storage, allows internal BRAM
        only for prefs/tiny text/BASIC fallback saves, enforces reserve space,
        and rejects image saves without the external cart path
  - [x] CD-ROM/ISO9660 remains the read-only app, demo, and asset source
  - [x] external Backup RAM cartridge is the planned primary writable store
        for small user documents, BASIC programs, imported text, and app data
  - [x] internal 8 KB Backup RAM is the fallback for preferences, launch state,
        and emergency tiny documents
  - [x] Host-tested BRAM BIOS wrapper contract for filename normalization,
        `BRMINIT`/`BRMSTAT` probing, directory/free-space/read/write semantics,
        and internal-BRAM `StorageVolumeInfo` mapping
  - [x] Live Sub CPU internal-BRAM BIOS vector adapter for
        `BRMINIT`/`BRMSTAT`/`BRMSERCH`/`BRMREAD`/`BRMWRITE`/`BRMDIR`, kept
        behind `BramBiosOps`
  - [x] Bind BASIC storage callbacks to the internal-BRAM adapter using a fixed
        BIOS-safe `BASIC` file and block-padded writes
  - [x] Prove BASIC SAVE/LOAD through the live internal-BRAM BIOS adapter in
        BlastEm/GDB with `BASIC_BRAM_PROBE=1` and `-Probe BasicBram`
  - [x] Host-tested external-cart probe contract that maps injected
        presence/capacity/free-byte results into `StorageVolumeInfo`
        without raw hardware calls
  - [ ] Implement and prove the live external Backup RAM cartridge adapter
        after the `_MBURAM`/`BRAM_CART` ABI is documented by source or probe
  - [ ] Bind BASIC/file-manager storage callbacks to the later external-cart
        driver and named-file UI
- [ ] Detect/probe external Backup RAM cartridge presence, capacity, and free
      blocks before designing the user-facing file manager:
  - [x] Normalize a probe result into the storage policy model with host tests
  - [ ] Prove the target cartridge probe against BlastEm/real hardware or an
        emulator that exposes external Backup RAM cart behavior
- [ ] Add a BASIC interpreter/tooling rung:
  - [x] Host-tested fixed-storage program buffer for numbered lines, small
        keyword tokenization, sorted insert/replace/delete, compaction, and
        decode
  - [x] Host-tested shell-facing line entry, `LIST`, and `NEW` commands with
        callback-based listing output
  - [x] Expression/value evaluator with signed 16-bit integer `+`/`-`
        arithmetic and quoted string literals
  - [x] Minimal sequential `RUN` behavior for stored `PRINT`/`END` programs
        with callback-based output and structured line-numbered errors
  - [x] Literal-line `GOTO` branching with missing-target errors and a hard
        runner step cap
  - [x] Fixed A-Z signed 16-bit integer runtime variables, `LET`
        assignment, undefined-variable errors, and bad-assignment errors
  - [x] Integer `IF`/`THEN` branching with truth/comparison conditions,
        bare line targets, `THEN GOTO`, missing-target errors, and bad
        condition errors
  - [x] Callback-backed integer `INPUT` for A-Z variable assignment, with
        unavailable-input and bad-input errors
  - [x] Literal-line `GOSUB`/`RETURN` subroutines with a fixed-depth return
        stack, missing-target errors, stray-return errors, and stack-overflow
        errors
  - [x] Fixed-format binary program image export/import for later storage
        integration
  - [x] Storage-callback `LOAD` and `SAVE` command shell behavior over the
        fixed-format BASIC program image
  - [x] Route BASIC `SAVE` through `STG_PlanSave()` with external-cart
        preference and tiny internal-BRAM fallback limits
  - [ ] String variables, arrays, and broader statement execution after `THEN`
  - [ ] Desktop text output/input binding through the VDI/AES-style layers
  - [x] Connect BASIC `LOAD`/`SAVE` storage callbacks to the internal-BRAM
        read/write bridge
  - [x] Add a live internal-BRAM BASIC smoke probe that writes a `BASPROBE`
        file, reloads it, and reports the result through GDB-visible Main
        variables without auto-formatting BRAM
  - [ ] Connect BASIC `LOAD`/`SAVE` storage callbacks to the later
        external-cart read/write driver
- [ ] Decide app/resource packaging format on ISO9660
- [ ] Add CD audio or PCM strategy only after storage ownership is clear

Acceptance: applications can load from CD and save at least one small
asset/document through the external Backup RAM cartridge path, with internal
BRAM fallback behavior documented and tested.

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
