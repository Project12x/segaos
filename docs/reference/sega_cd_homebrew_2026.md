# Sega CD Homebrew Update - June 2026

Research date: 2026-06-19

This note records Sega CD homebrew/tooling developments that were not part of
the original SegaOS project assumptions.

## Primary Update: Megadev 1.2.0

`drojaazu/megadev` is a current Sega Mega Drive and Mega CD framework in C and
68k assembly. It is less turnkey than SGDK, but it directly targets the Mega CD
problems SegaOS is solving: boot-sector generation, IP/SP structure, Sub CPU
CD-ROM access, module loading, Word RAM ownership, PCM/BRAM helpers, and
hardware definitions.

- Repo: https://github.com/drojaazu/megadev
- Version inspected: MEGADEV 1.2.0
- Release date shown by GitHub: 2026-05-10
- Commit inspected: `7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
- License: MIT
- Reuse mode in SegaOS: pattern-only for boot-layout/build architecture;
  direct-copy for `src/main/security.c` from Megadev `lib/security.c`

Source files/docs inspected at that commit:

- `README.md`
- `VERSION`
- `LICENSE`
- `docs/manual.md`
- `docs/boot.md`
- `docs/disc.md`
- `docs/cdrom.md`
- `docs/megacd_dev.md`
- `docs/modules.md`
- `docs/program_design.md`
- `new_project/README.md`
- `megadev.make`
- `lib/cd_boot.s`
- `cfg/ip.ld`
- `cfg/sp.ld`
- `lib/security.c`
- `lib/sub/sp_header.s`
- `lib/sub/bios.def.h`
- `lib/sub/cdboot.def.h`
- `examples/hello_world/src/ip.s`
- `examples/hello_world/src/sp.s`
- `new_project/src/ip.s`
- `new_project/src/sp.s`

## Corrected Toolchain Conclusion

Old assumption: no new Sega CD SDK/toolchain materially changed SegaOS's plan.

Corrected conclusion: Megadev 1.2.0 is a major new/current permissive reference
for Sega CD homebrew and should be used before inventing new Sega CD boot,
module, or CD-ROM access machinery.

This does not mean SegaOS should become a Megadev project wholesale. SegaOS
already has a custom dual-CPU OS architecture, a custom blitter, and a local
SGDK-provided m68k toolchain. The useful move is to align SegaOS's low-level
disc/IP/SP assumptions with Megadev's maintained reference patterns.

## Boot Disc Implications

Megadev's build flow creates a 32KB boot sector and injects it into a cooked
ISO9660 data track with `mkisofs -G boot.bin`. Its CUE examples use
`TRACK 01 MODE1/2048` for the data track.

Important boot-sector details from `lib/cd_boot.s`:

- Disc ID: `SEGADISCSYSTEM  `
- Header IP offset field: `$0800`
- Header IP size field: `$0800`
- Header SP offset field: `$1000`
- Actual IP bytes are included starting at boot-sector offset `$0200`
- Actual SP bytes are included starting at boot-sector offset `$1000`
- Boot sector is padded to `$8000` bytes

The `$0800` IP header field is a Sega CD BIOS compatibility quirk for US/EU
security code. Do not treat it as the physical offset where the IP bytes are
inserted into `boot.bin`.

## Current SegaOS Reconciliation Items

- `tools/build_iso.py` now writes a cooked ISO9660 `MODE1/2048` image with a
  Sega CD system area using Python's standard library.
- The builder now records the Megadev-compatible `$0800` IP header field,
  physically places IP bytes at `$0200`, and physically places SP bytes at
  `$1000`.
- Oversized IP/SP payloads are rejected instead of truncated.
- `tools/verify_disc.py` now checks the generated ISO/CUE and fails on layout
  errors or selected JP/US/EU security-marker mismatches.
- SegaOS now direct-copies Megadev's MIT `lib/security.c` to provide the
  regional IP security block, links it first, and validates it during
  `make iso`.
- SegaOS's SP header/linker layout now follows the Megadev SP module contract
  and uses 2-byte section subalignment. The current assembly-only probe SP is
  1,042 text bytes in the visible framebuffer probe, with `sp_init` at `$602A`,
  `sp_main` at `$607E`, and `_TEXT_LENGTH = $0412`.
- Raw `MODE1/2352` output can be generated later if a target emulator, ODE, or
  burning workflow needs it.

## Other 2026-Current Items

- ares v148 was current during the June 2026 research pass and should be in the
  emulator regression matrix for boot testing.
- BlastEm remains useful for Mega Drive/Sega CD debugging workflows and should
  stay in the matrix if available.
- SGDK's latest release observed during research was still 2.11 from April 2025;
  it remains a toolchain source here, not a Sega CD framework replacement.
- `MCD_BOOT` is useful prior art for behavior and testing, but it is GPL. Do not
  copy source into SegaOS.
- `Clownacy/clownmdemu-core` is active Sega CD emulator-side prior art, but it
  is AGPL. Use only for behavioral understanding.

## Practical Priority For SegaOS

Use Megadev reconciliation as the bottom rung of a bring-up ladder:

1. Prove a minimal dual-CPU probe: Main CPU boot, Sub CPU boot, comm flags, and
   Word RAM handoff with a known pattern.
2. Prove the framebuffer path with a deterministic 4bpp pattern before running
   the full desktop loop.
3. Measure or choose the VDP transfer policy before treating frame updates as
   stable.
4. Bring the desktop/apps back after boot, CPU handshake, Word RAM, framebuffer,
   and VDP timing are known-good.
5. Add CD-ROM/BRAM/storage features after the core runtime is proven. Plan
   writable persistence around external Backup RAM cartridge-class storage, not
   only the internal 8 KB BRAM: CD-ROM is the read-only app/resource source,
   the external cartridge is the primary small-document store, and internal
   BRAM is the fallback for preferences and emergency tiny saves.

Done locally on 2026-06-19: BlastEm 0.6.3-pre with a USA BIOS and SGDK GDB
hit `$00FF0000`; `$FF0000` contained the expected US security bytes. That closes
boot-disc recognition as the first gate and moves the focus to dual-CPU runtime
proof.

A manual Megadev `hello_world` control image was built with SegaOS's Python ISO
builder. It reached the Megadev Main IP loop in BlastEm, proving that the
current builder/security path can boot a true Megadev Main IP. Reuse mode:
control-build/pattern-only from the MIT Megadev files listed above; no control
source was copied into SegaOS.

Follow-up evidence from the same sprint first showed the minimal C-runtime
`BOOT_PROBE=1` image reaching a Main-side halt with SP payload bytes visible in
PRG-RAM bank 0 at `$006000`, but no Sub breadcrumbs. A hybrid image using the
passing Megadev-style control IP with the SegaOS C-runtime probe SP also
produced zero Sub status, isolating the old failure to the SegaOS SP runtime
shape.

The current probe ladder has moved past that blocker. A permanent
Megadev-derived dual-CPU control fixture now reports from Sub-side code, and
SegaOS `BOOT_PROBE=1` has been pivoted to an assembly-only SP path.
`tools/probe_blastem_boot.ps1 -Probe DualCpuStatus` passes in BlastEm: Main
magic `0x4d50`, Sub ready `0x0002/0x5244`, command status `0x0003`, result
words `0x444e/0xa55a/0x5aa5`, and trace `0x52fe`. The strict Word RAM rung
also passes now: `-Probe DualCpu` proves Sub writes `0xa55a/0x5aa5` at
`$0C0000`, clears RET in 1M mode, and Main reads the pattern at `$200000`
with MEM_MODE moving from `0x2a05` to `0x2a04`. Treat the remaining low-level
issue as a production double-buffer/frame-scheduler problem, not disc
recognition, Sub startup, first Word RAM return, or first visible framebuffer
display. The framebuffer rung now also has BlastEm internal screenshot proof
for the deterministic full-screen 4bpp pattern.

Follow-up display evidence on 2026-06-30: the default boot-safe desktop frame is
now captured with `BOOT_SAFE_VISUAL_PROBE=1` and
`tools\capture_blastem_internal_screenshot.ps1 -DebugAutoBoot`, which uses GDB
to prove the app reached `segaos_visual_probe_halt` phase `0x76ff` before
triggering BlastEm internal screenshotting. Treat this debugger-backed path as
the default visual proof route; START-key autoplay captures can land on the Sega
CD BIOS screen and should not be accepted as app-render evidence.

Later 2026-06-30 update: the current command protocol uses CFM only as a pending
signal and carries the opcode in `CMD0`. This matches the now-passing
`BOOT_PROBE=1` assembly path and the C desktop path. The latest accepted
boot-safe desktop screenshot is
`C:\tmp\segaos_screens_internal\segaos_repeat_20260630_231605.png`, which
shows the dirty-list root redraw plus direct startup window furniture with real
SGDK-font title/body text.

Repeated-frame update: `DESKTOP_REPEAT_PROBE=1` now proves the boot-safe
single-bank desktop path can complete two render/upload cycles in one run.
After Main releases the first returned frame, BlastEm reports MEM_MODE
`0x2a06`; the Sub CPU accepts that as writable ownership, completes the second
`CMD_RENDER_FRAME` with status `0x0003` and trace `0x7404`, and Main reads the
repeated title row from VDP as `0xf11f/0x1f11`. This is still a bring-up
single-bank proof; full alternating 1M double buffering is separate work.

Dirty-transfer update on 2026-07-01: `DR_TileRangeBudget()` now gives the first
host-tested bridge from dirty rectangles to Genesis VDP transfer planning. This
is clean-room SegaOS code, not Megadev code: Megadev remains the boot/CD/BIOS
reference, while this helper applies the measured Genesis VDP budget constraint
to SegaOS dirty regions. `make host-tests` proves a small 2x2-tile dirty range
is 4 tiles / 128 bytes and fits a 7,524-byte NTSC VBlank budget, while a full
40x28 tile frame is 1,120 tiles / 35,840 bytes and does not.

Dirty-queue update on 2026-07-01: `DR_QueueTileRange()` and
`DR_BuildTileQueueFromDirtyList()` now turn those dirty ranges into explicit
upload spans using caller-owned static storage. The queue planner splits
partial-width dirty ranges by row, treats full-width ranges as contiguous tile
spans, slices oversized spans to the caller's byte budget, and reports
budget-exceeded separately from queue-storage overflow. Reference-code-first
record: SGDK repo `https://github.com/Stephane-D/SGDK`, commit
`ef9292c03fe33a2f8af3a2589ab856a53dcef35c`, MIT license, inspected files
`inc/dma.h`, `src/dma.c`, and `src/sys.c`, reuse mode pattern-only /
clean-room. No SGDK queue source was copied or closely ported.

Framebuffer-span update on 2026-07-01: `FB_ConvertTileSpan()` now exposes the
Main CPU linear-framebuffer-to-VDP-tile conversion as a host-tested seam. The
test covers a single arbitrary tile, a contiguous span crossing the 40-tile row
boundary, and invalid span rejection. The Main CPU build still uses 16-bit Word
RAM reads for the hardware path; the host test path uses normal byte reads so
the same tile-index math can be verified on the development machine. This
prepares the live VBlank queue consumer without changing the default full-frame
upload path yet.

Dirty-upload consumer update on 2026-07-01: `FB_FlushTileQueueWithCallback()`
now consumes `DirtyTileQueue` spans, chunks them to the caller's scratch buffer,
converts each chunk through `FB_ConvertTileSpan()`, and emits VRAM address /
word-count metadata to an upload sink. Host tests prove the 7,520-byte
NTSC-budgeted full-frame slice becomes two uploads through the 5,120-byte strip
buffer: 160 tiles then 75 tiles. `FB_UpdateTileQueue()` is the Main-side DMA
wrapper.

Dirty-upload emulator update on 2026-07-01: `DESKTOP_DIRTY_QUEUE_PROBE=1` now
proves the wrapper in BlastEm/GDB. The diagnostic Main path poisons the sampled
title tile row in VRAM, builds one public `DirtyTileQueue` entry for tile
`0x0147`, calls `FB_UpdateTileQueue()`, reaches phase `0x85ff`, and reads
`0xf11f/0x1f11` back from VRAM matching Word RAM. This is a narrow hardware
proof of the queue-to-DMA path, not a final VBlank scheduling policy.

Storage-policy update on 2026-07-01: `STG_PlanSave()` now gives the first
host-tested persistence decision seam. It is clean-room SegaOS policy code, not
a BRAM hardware driver and not copied from Megadev: CD-ROM remains read-only,
external Backup RAM cart storage is preferred for user documents/BASIC/text,
internal BRAM is allowed only for preferences and tiny text/BASIC fallback
saves, and image documents require the external cart path.

BASIC-buffer update on 2026-07-01: `BAS_StoreSourceLine()` now gives the first
host-tested BASIC tooling seam. It is clean-room SegaOS code with no copied or
closely ported GEOS, GEM/TOS, CP/M-68K, Megadev, or SGDK interpreter source.
The implemented scope is deliberately below an interpreter: numbered-line
parsing, small keyword tokenization, sorted insert/replace/delete, compaction
of caller-owned storage, and decode for later `LIST`/desktop UI work.

BASIC-shell update on 2026-07-01: `BAS_SubmitConsoleLine()` now adds the first
REPL-facing command seam over that buffer. Host tests prove numbered line input
through the shell, callback-based `LIST` output in sorted program order, `NEW`
clearing program storage, and explicit rejection of `RUN` until an evaluator
exists.
