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
- `lib/sub/bram.def.h`
- `lib/sub/bram.h`
- `lib/sub/cdboot.def.h`
- `lib/sub/sub.macro.s`
- `lib/main/bramcart.def.h`
- `examples/bram/README.md`
- `examples/bram/src/bram_demo.c`
- `examples/bram/src/spx.c`
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

BASIC-image update on 2026-07-02: `BAS_ExportProgramImage()` and
`BAS_ImportProgramImage()` now provide the first fixed binary payload for
BASIC `SAVE`/`LOAD` work. The format is explicit bytes rather than a packed C
struct: magic `SBAS`, version 1, line count, big-endian storage size, line
table, then compact token storage. Host tests prove round-trip list output,
required-size reporting for short export buffers, and bad-magic rejection
without clearing the caller's existing program. This is not a BRAM driver or
file manager. Reuse mode remains clean-room.

BASIC-storage-shell update on 2026-07-02:
`BAS_SubmitConsoleLineWithStorage()` now recognizes `SAVE` and `LOAD` using a
caller-supplied `BasicStorageIO` callback table plus caller-owned scratch image
buffer. `SAVE` exports the `SBAS` program image and hands it to the save
callback. `LOAD` reads bytes from the load callback, validates/imports the
image, and leaves the existing program intact when import fails. Host tests
prove successful save callback output, successful load replacement, and corrupt
load rejection without clearing the current program. This still deliberately
avoids direct BRAM or external-cart hardware calls; the callbacks still need
real BRAM/external-cart read/write drivers. Reuse mode remains clean-room; no
GEOS, GEM/TOS, CP/M-68K, Megadev, or SGDK interpreter source was copied or
closely ported.

BASIC-storage-policy update on 2026-07-02: `src/sub/basic_storage.c` now adds
the first policy bridge between BASIC `SAVE`/`LOAD` byte callbacks and
`STG_PlanSave()`. `BAS_StorageBindIO()` produces a `BasicStorageIO` table for
the shell. Its `SAVE` callback plans an `STG_DOC_BASIC` document, prefers the
external Backup RAM cart when there is usable free space, and only falls back
to internal BRAM inside the tiny BASIC limit. Its `LOAD` callback reads from
external cart when present and falls back to internal BRAM only when the cart
is absent. Host tests prove external-cart save preference, tiny internal
fallback, large-BASIC rejection without a cart, shell `SAVE` using the
selected target, and shell `LOAD` preferring the cart. This is still not a
hardware BRAM BIOS wrapper or file manager. Reuse mode remains clean-room.

BRAM-wrapper update on 2026-07-02: `src/sub/bram.c` now provides the first
host-tested internal Backup RAM BIOS contract wrapper. Reference-code-first
record: Megadev repo `https://github.com/drojaazu/megadev`, commit
`7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`, MIT license, inspected files
`lib/sub/bram.def.h`, `lib/sub/bram.h`, `lib/sub/sub.macro.s`,
`lib/main/bramcart.def.h`, `examples/bram/README.md`,
`examples/bram/src/bram_demo.c`, and `examples/bram/src/spx.c`. Reuse mode is
pattern-only / clean-room: SegaOS does not copy Megadev's inline assembly
wrappers. The wrapper records the `BURAM` vector `$005F16`, BIOS function
semantics for `BRMINIT`, `BRMSTAT`, `BRMREAD`, `BRMWRITE`, and `BRMDIR`, 4KB
init/stat blocks, normal 0x40-byte file blocks, 11-byte BIOS filenames, and
16-byte directory entries behind injectable `BramBiosOps`. Host tests prove
filename/pattern normalization, formatted/unformatted probe mapping, internal
`StorageVolumeInfo` mapping, write block construction, read size guarding, and
directory call-through. This is not the live inline Sub BIOS vector call layer
yet and does not probe an external Backup RAM cart.

BRAM-BIOS-adapter update on 2026-07-02: `src/sub/bram_bios.c` now binds a
`BramBiosContext` to `BramBiosOps`, and `src/sub/bram_bios_68k.s` contains the
raw Sub CPU `$005F16` calls for internal Backup RAM. Reference-code-first
record remains Megadev repo `https://github.com/drojaazu/megadev`, commit
`7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`, MIT license, inspected files
`lib/sub/bram.def.h`, `lib/sub/bram.h`, and `lib/sub/sub.macro.s`. Reuse mode
is pattern-only / clean-room for SegaOS source: the adapter follows the
documented register ABI and carry-flag semantics but does not copy Megadev's
inline C assembly wrappers. Host tests fake the raw call layer and prove
context buffer ownership plus callback routing for init/stat/search/read/write
and directory calls. The forced ISO build assembles the target raw-call file.
This still does not bind BASIC `SAVE`/`LOAD` or the desktop file manager to
BRAM, and it does not probe an external Backup RAM cartridge.

BASIC-BRAM-storage update on 2026-07-03: `src/sub/basic_bram_storage.c` now
connects BASIC's generic `SAVE`/`LOAD` byte callbacks to the internal BRAM
wrapper. Reuse mode is clean-room over SegaOS' own `BramBiosOps` contract;
Megadev remains the pinned pattern reference for the underlying Sub BIOS BRAM
ABI, but no Megadev BASIC or storage adapter source was copied or closely
ported. Host tests prove filename normalization, internal-volume probing for
the storage policy adapter, rejected external-cart targets, `BRM_WriteFile()`
routing with a fixed `BASIC` filename, `BRM_ReadFile()` routing, and a shell
`SAVE`/`LOAD` round trip through `BasicStorageAdapter` with no external cart
present. Because the BRAM BIOS reports file size in 0x40-byte normal blocks,
the bridge pads exported `SBAS` images from a static scratch buffer before
write; reads may report the padded block size, and the BASIC importer accepts
that because the image header carries the exact line table and storage length.
This is still not a named-file UI and does not implement the external Backup
RAM cartridge path.

BASIC-shell update on 2026-07-01: `BAS_SubmitConsoleLine()` now adds the first
REPL-facing command seam over that buffer. Host tests prove numbered line input
through the shell, callback-based `LIST` output in sorted program order, `NEW`
clearing program storage, and the command dispatch shape that later hosts
`RUN`.

BASIC-expression update on 2026-07-01: `BAS_EvaluateExpression()` now adds the
first value evaluator seam. It supports signed 16-bit integer literals with
left-to-right `+`/`-` arithmetic and quoted string literals, rejects empty,
unterminated, mixed, or overflowing expressions, and remains independent of
desktop I/O and storage.

BASIC-runner update on 2026-07-01: `BAS_RunProgram()` now adds the first
statement execution seam. It runs stored program lines sequentially, supports
`PRINT` of the current integer/string expression values, stops on `END`, routes
output through the same callback shape as `LIST`, and reports unsupported
statements or bad expressions with source line numbers. It is still clean-room
SegaOS code and does not implement desktop I/O, `LOAD`, or `SAVE`.

BASIC-GOTO update on 2026-07-01: `BAS_RunProgram()` now supports literal-line
`GOTO` by resolving targets against the sorted program table. Host tests prove
successful jumps, missing-target errors, and a hard `BAS_RUN_MAX_STEPS` guard
that stops self-jumping loops. This is still not desktop I/O, `LOAD`, or
`SAVE`.

BASIC-variable update on 2026-07-01: `BasicRuntime` now provides a fixed
A-Z signed 16-bit integer variable table. Host tests prove runtime set/get,
runtime-backed expression lookup, `LET A = <integer expression>` assignment,
undefined-variable errors, and string-assignment rejection. This is still a
small OS-tooling seam, not a complete BASIC: no string variables, arrays,
desktop I/O, `LOAD`, or `SAVE`. Reuse mode remains clean-room; no GEOS,
GEM/TOS, CP/M-68K, Megadev, or SGDK interpreter source was copied or closely
ported.

BASIC-IF update on 2026-07-01: `BAS_RunProgram()` now supports a narrow
`IF ... THEN ...` branch form. Conditions are integer-only: either truthiness
of an integer expression or comparisons using `=`, `<>`, `<`, `>`, `<=`, and
`>=` between integer expressions. `THEN` targets are literal line numbers, with
optional `GOTO` before the target. Host tests prove true jumps, false
fallthrough, `THEN GOTO`, missing-target errors, and bad-condition errors. This
still does not execute arbitrary statements after `THEN` and does not add
string variables, arrays, desktop I/O, `LOAD`, or `SAVE`.

BASIC-INPUT update on 2026-07-02: `BAS_RunProgramWithIO()` now adds the first
input seam. `BasicInputSource` is a caller-supplied callback that writes one
input line into the runner scratch buffer. `INPUT A` assigns an integer value
to a fixed A-Z runtime variable, reports `BAS_RUN_INPUT_UNAVAILABLE` when no
input source is present or the source declines a line, and reports
`BAS_RUN_BAD_INPUT` for non-integer values. This is still not desktop input UI,
prompt rendering, string input, arrays, `LOAD`, or `SAVE`. Reuse mode remains
clean-room.

BASIC-GOSUB update on 2026-07-02: `BAS_RunProgramWithIO()` now supports
literal-line `GOSUB` and `RETURN` with a local fixed-depth return stack
(`BAS_GOSUB_STACK_DEPTH`). Host tests prove normal return to the following
line, missing target errors, `RETURN` without `GOSUB`, and stack overflow for a
recursive self-call. This is still not arbitrary statement execution after
`THEN`, string variables, arrays, desktop I/O, `LOAD`, or `SAVE`. Reuse mode
remains clean-room.

BASIC-BRAM-runtime update on 2026-07-03: `BASIC_BRAM_PROBE=1` now exercises
that stack in BlastEm against the live Sub BIOS internal BRAM vector adapter.
The opt-in probe initializes `BRM_InitInternalBiosOps()`, runs
`BAS_RunBramSmoke()` through a non-user-facing `BASPROBE` file, and reports the
result through Main CPU GDB symbols checked by
`tools/probe_blastem_boot.ps1 -Probe BasicBram`. The current BlastEm run
reported formatted internal BRAM (`0x0003`), two total/free 4K blocks before
the write, `SAVE`/`LOAD` summary `0x0101`, loaded line/target summary
`0x0211`, and terminal trace `0x75ff`. The probe intentionally does not call
`BRMFORMAT`; unformatted BRAM remains an explicit future UX/tooling problem.

External-cart-probe update on 2026-07-03: `include/external_cart.h` and
`src/sub/external_cart.c` now add a host-tested external Backup RAM cartridge
probe boundary. Reference-code-first record: Megadev repo
`https://github.com/drojaazu/megadev`, commit
`7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`, MIT license, inspected files
`lib/main/bramcart.def.h`, `examples/bram/README.md`,
`examples/bram/src/bram_demo.c`, and `examples/bram/src/spx.c`. Reuse mode is
pattern-only / clean-room. SegaOS does not copy or closely port Megadev code
for this seam. The inspected Megadev Main-side file exposes only the
`BRAM_CART`/`_MBURAM` vector address `$FFFDAE`, while the inspected BRAM
example performs file operations through the Sub BIOS BRAM helper layer.
Therefore SegaOS does not call the raw Main vector yet. The new seam accepts an
injected probe result, maps present/readonly/absent states into
`StorageVolumeInfo`, clamps impossible free-byte counts to total capacity, and
lets `STG_PlanSave()` consume a detected external cart for large saves. This is
the storage-policy integration contract, not live external cartridge hardware
proof; the target adapter still needs a separate ABI/probe pass.

Frame-scheduler update on 2026-07-03: `include/frame_scheduler.h` and
`src/main/frame_scheduler.c` now add a host-tested Main-side upload scheduling
cursor. Reference-code-first record remains the SGDK DMA queue source noted
above: `Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c`, MIT,
inspected files `inc/dma.h`, `src/dma.c`, and `src/sys.c`, reuse mode
pattern-only / clean-room. No SGDK source is copied or closely ported. The new
cursor advances a pending tile span into a `DirtyTileQueue` under a caller's
byte budget; host tests prove a full 1,120-tile frame becomes four 235-tile
NTSC-budgeted frames plus a final 180-tile frame. This is the frame-policy
planning rung before the live desktop loop calls `FB_UpdateTileQueue()` at a
measured VBlank/update point. `DESKTOP_SCHEDULER_PROBE=1` now proves the first
target integration step in BlastEm/GDB: after a real Sub `CMD_RENDER_FRAME`,
Main begins a compact `FrameUploadPump`, plans two 235-tile queues from
pump-owned state, uploads both through `FB_UpdateTileQueue()`, reaches phase
`0x87ff`, and verifies a poisoned VRAM word in the second slice changes from
`0x0ee0` back to the Word RAM value `0xf11f`. The probe uses a narrow Main path
to fit the Megadev-compatible 3,584-byte IP window. `DESKTOP_PUMP_PROBE=1` now
extends that evidence across the whole frame and across repeated frames: Main
uploads four 235-tile queues plus one final 180-tile queue per frame, returns
Word RAM only after each final slice, and proves four complete
render/upload/return cycles with terminal phase `0x88ff` and frame count
`0x0004`. The repeated size-fit pump build measured 3,460 bytes, while the
direct callback-pump target attempt measured 3,948 bytes and an attempt to
force the compact pump through the older `DESKTOP_LOOP_PROBE` scaffolding
measured 4,332 bytes. Both overflow cases exceed the physical IP range, so the
lean `DesktopPump` harness remains the target pump evidence path for now. This
is target evidence for the scheduler/upload contract, not the final
long-running VBlank policy. `src/main/frame_upload_pump.c` now adds the
host-tested callback state machine for that future policy: it starts a rendered
frame cursor, advances one budgeted upload queue per tick, waits to signal Word
RAM return until the final slice completes, rejects overlapping frame starts,
and rewinds the cursor on upload failure. Reuse mode remains pattern-only /
clean-room against the SGDK DMA queue reference noted above.

Default boot update on 2026-07-03: the boot-safe first frame now uses the
compact `FUP_BeginFrame()` + `FUP_PlanNextQueueCompact()` upload path instead
of one-shot `FB_UpdateFrame()`. The forced normal `make iso` build measures
Main IP 3,504 bytes / SP 11,808 bytes and the visual-probe build measures Main
IP 3,524 bytes, both within the 3,584-byte IP window. To fit, boot-safe Main
code uses `-Os` and skips Mega Mouse initialization until the live input loop
has its own probe. The debugger-backed BlastEm internal screenshot is
`C:\tmp\segaos_screens_internal\segaos_pump_default_20260703_164252.png`.
