# Project State

## Current Phase
**Milestone C/D bridge: visible boot-safe desktop frame**

Phase 1 and Phase 2 are complete. The production Word RAM bank-sync code paths
are present, but repeated-frame timing still needs validation. Milestone A is complete:
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
word-safe framebuffer backend: checker desktop, menu separator, titled starter
window, and text captured at
`C:\tmp\segaos_screens_internal\segaos_internal_20260629_171815.png`.

The active strategy is a bring-up ladder:

1. Reproducible boot artifact
2. Minimal dual-CPU probe
3. 4bpp framebuffer probe
4. VDP timing budget
5. Desktop and app bring-up
6. Storage and OS features
7. Compatibility and release hardening

## Build Status
| Target | Status | Notes |
|--------|--------|-------|
| Sub CPU (`build/sub_cpu.bin`) | Builds | Boot-safe desktop default: 10,338-byte SP binary observed locally with word-safe BLT framebuffer backend; full app SP is deferred |
| Main CPU (`build/main_cpu.bin`) | Builds | 2,708 text bytes observed locally with US security block |
| CPU-only build | Passing | `C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile sub main` |
| Disc image (`build/segaos.iso/.cue`) | Builds and verifies | `make iso` writes cooked `MODE1/2048` ISO/CUE and runs verifier |
| Emulator IP probe | Passing | BlastEm + USA BIOS + SGDK GDB hit `$00FF0000` and read US security bytes |
| Megadev dual-CPU control | Passing | `tools/build_megadev_dualcpu_control.ps1` + `-Probe MegadevControl` proves the builder/security path can boot a Megadev-shaped SP and report from Sub code |
| Dual-CPU status probe | Passing | `BOOT_PROBE=1` + `-Probe DualCpuStatus` proves SegaOS Main boot, Sub `sp_init`/`sp_main`, and command/status round trip |
| Strict Word RAM probe | Passing | `-Probe DualCpu` proves Sub bank-0 write, 1M RET clear, and Main `$200000` visibility with `0xa55a/0x5aa5` |
| Framebuffer probe | Passing | `-Probe Framebuffer` proves Sub 4bpp pattern, 1M RET clear, Main Word RAM readback, `FB_UpdateFrame()`, and VDP VRAM tile-0 readback; the visible probe is confirmed by BlastEm internal screenshot |
| Runtime smoke probe | Passing | `SUB_RUNTIME_SMOKE=1` + `-Probe RuntimeSmoke` proves normal C SP startup and command handshake without desktop modules |
| Boot-safe desktop render probe | Passing | `DESKTOP_INIT_PROBE=1` + `-Probe DesktopInit` proves real boot-safe C SP first render command and Main upload path |

## Toolchain
- SGDK m68k-elf-gcc (C:\SDKS\SGDK\bin\)
- Direct ld.exe linking (bypasses LTO plugin issue)
- libgcc.a for runtime math helpers
- Current ISO builder/verifier path uses Python standard-library scripts
- The Makefile prefers local Python 3.12/3.14 before falling back to `python`
- Megadev 1.2.0 is the primary Sega CD boot-layout reference

## Key Metrics
- Work RAM usage: Main CPU IP remains within the 0xE00 boot-sector envelope
  after the regional security block is linked first
- PRG-RAM usage: 10,338 bytes / ~488 KB observed locally for the default
  boot-safe Sub CPU SP binary
- BOOT_PROBE SP usage: 930 text bytes, intentionally below Megadev's 16KB
  default SP window
- BOOT_PROBE SP layout: Megadev-style `SUBALIGN(2)`, `sp_init` at `$602A`,
  `sp_main` at `$607E`, `_TEXT_LENGTH = $03a2`
- Boot-safe desktop SP usage: 10,338 bytes observed locally with
  `BOOT_SAFE_DESKTOP=1`
- Main CPU stack: now capped at `$FFF700`, below the Main BIOS work/system-use region
- Strip buffer: 5,120 bytes (4 tile-rows at a time)
- Framebuffer: 35,840 bytes @ 4bpp (320x224)
- Sub CPU blitter default: 4bpp, matching the Main CPU tile conversion path
- Disc image: 150 cooked sectors, `MODE1/2048`, 32KB boot/system area

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
  control-build/pattern-only for Megadev `hello_world` IP/SP behavior
- Inspected files: `README.md`, `VERSION`, `LICENSE`, `docs/manual.md`,
  `docs/boot.md`, `docs/disc.md`, `docs/cdrom.md`, `docs/megacd_dev.md`,
  `docs/modules.md`, `docs/program_design.md`, `new_project/README.md`,
  `megadev.make`, `lib/cd_boot.s`, `cfg/ip.ld`, `cfg/sp.ld`,
  `lib/security.c`, `lib/sub/sp_header.s`, `lib/main/gate_arr.def.h`,
  `lib/sub/gate_arr.def.h`, `lib/main/gate_arr.h`, `lib/sub/gate_arr.h`,
  `lib/main/gate_arr.macros.s`, `lib/sub/sub.macro.s`,
  `lib/main/mmd.h`, `lib/main/mmd.macros.s`, `lib/main/memmap.def.h`,
  `lib/build.def.h`, `lib/sub/bios.def.h`, `lib/sub/cdboot.def.h`,
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
- The framebuffer probe uses that handoff for a deterministic 4bpp tile:
  Sub writes `$1234/$5678` to linear row 0 and `$9abc/$def0` to row 1, Main
  reads those words through `$200000`, runs `FB_UpdateFrame()`, and reads the
  same words back from VDP VRAM tile 0. The visible probe variant fills the
  frame with the same alternating row pattern and has been captured through
  BlastEm's internal screenshotting.

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

## Active Risks

High priority:

- The verifier checks known JP/US/EU security prefixes and license markers, but
  this is still not a substitute for emulator and hardware/ODE validation.
- The current dual-CPU probes prove Main boot, Sub `sp_init`/`sp_main`, Gate
  Array command/status, one-way Sub bank-0 Word RAM return, framebuffer-to-VDP
  tile readback, and a visible deterministic framebuffer display. They do not
  yet prove the full alternating double-buffer policy for repeated frames.
- The boot-safe C desktop path now reaches Sub-ready, completes a first
  render/upload sequence, and displays a visible Mac-like starter frame through
  BLT. `WM_DrawDesktop()` now owns the checker desktop/menu shell, while the
  starter window stays a compact boot-safe BLT title/text renderer. BLT
  framebuffer access uses 16-bit Word RAM helpers; the next risk is moving to
  real `WM_NewWindow()`/menu/cursor rendering without regressing command timing
  or Word RAM ownership.
- The active boot decision has narrowed: keep the assembly probe as the
  low-level truth source, keep the boot-safe direct renderer as the startup
  path, and reintroduce BLT/window-manager drawing behind probe-proven command
  and Word RAM handoff invariants.

Runtime validation:

- Word RAM bank ownership is proven for the first Sub `$0C0000` to Main
  `$200000` handoff. Alternating 1M double buffering is still unresolved:
  after RET is cleared, Sub must not keep rendering blindly to `$0C0000`.
- Full-frame conversion/DMA is acceptable for bring-up, but needs a VDP timing
  policy before the desktop loop can be considered stable.

Lower priority:

- Some GUI draw calls need audit for width/height parameters versus endpoint
  coordinates.
- Storage features should wait until boot, CPU handshake, framebuffer, and VDP
  timing are proven.
