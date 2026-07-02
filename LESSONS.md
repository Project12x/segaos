# Lessons

This project is pre-alpha and the plan is allowed to change. These notes capture
facts learned from emulator evidence so future work does not re-litigate the
same failures.

## Reference Code First

- Treat Megadev 1.2.0 as the primary Sega CD boot-layout reference until a
  stronger maintained reference replaces it.
- Current pinned reference:
  `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`, MIT.
- When implementing boot, IP/SP layout, Gate Array, or BIOS-facing behavior,
  inspect the pinned upstream source before writing new code and record reuse
  mode in the implementation notes.
- For desktop GUI architecture, use the pinned GEM/TOS-family references in
  `docs/reference/68k_desktop_prior_art.md`: EmuTOS, FreeMiNT/XaAES, and
  OpenGEM. They are GPL-family references, so the allowed reuse mode is
  pattern-only / clean-room behavior specs.
- Treat GEOS, GEM/TOS, CP/M-68K, and related 68k-era OSes as golden references,
  but do not blur their licensing. PC/GEOS is a promising Apache-2.0 candidate
  reference; CP/M-68K and reverse-engineered 8-bit GEOS sources need a pinned
  source/license pass before any implementation work uses them. Until then,
  they are historical behavior references only.
- For concrete UI assets such as fonts, do not invent vague placeholder
  glyphs and then debug against them. The current system font is SGDK v2.11's
  MIT 8x8 `font_default.png`, pinned at
  `Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c` and converted
  into SegaOS' 1bpp glyph format with attribution in `third_party/sgdk_font/`.
- For VDP upload queue behavior, SGDK v2.11's MIT DMA queue is the current
  permissive pattern reference:
  `Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c`, inspected files
  `inc/dma.h`, `src/dma.c`, and `src/sys.c`. SegaOS' `DirtyTileQueue` uses
  pattern-only / clean-room reuse: caller-owned static storage, explicit
  byte-budget and capacity checks, and a later flush point. No SGDK queue code
  is copied or closely ported.

## Freestanding C Runtime

- This project has no hosted C library. Build both CPU C images with
  `-ffreestanding -fno-builtin`.
- The failure mode is real: without those flags, m68k GCC optimized SegaOS'
  local `memset()` implementation into a recursive call to `memset` itself.
  The first visible symptom was `DESKTOP_WM_PROBE=1` timing out inside
  `WM_Init()` while clearing the static `WindowManager`.
- When a Sub CPU path dies inside a simple memory primitive, disassemble before
  changing architecture. In this case, the WM data structure was not the bug;
  the generated libc code was.
- On this Windows/SGDK setup, use `C:\SDKS\SGDK\bin\make.exe -r -B -f Makefile`
  for forced rebuilds. `-B` without `-r` can invoke built-in implicit rules and
  may treat `crt0.s` as a case-insensitive `.S` source, rewriting the startup
  file instead of assembling it.

## Desktop Architecture

- The SegaOS GUI should be split like GEM:
  1. VDI-like drawing/text/clipping/tile-invalidation layer
  2. AES-like window/event/redraw ownership layer
  3. Desktop shell and apps on top
- Do not treat readable text as a window-manager feature. Prove a small
  fixed-font primitive first, then restore styled title composition.
- Dirty rectangles and clip ownership need their own static pools and tests
  before broad window-manager redraw. EmuTOS/FreeMiNT history says redraw bugs
  are a primary failure mode, not polish.
- `src/sub/dirty_rect.c` is the clean-room dirty-region contract. It clips to
  screen bounds, rejects empty rects, merges overlapping or edge-touching
  invalidations, keeps corner-only contact separate, subtracts one rect from
  another into deterministic strips, collapses overflow to one bounding rect,
  maps dirty pixels to 8x8 tile ranges, and now computes first-tile,
  tile-count, byte-count, row-span, budget-fit data, and bounded upload spans
  for the later VDP upload queue.
- Root desktop redraw is now a separate tested contract from window furniture
  and app content callbacks. `DR_PlanRootRedraw()` splits a dirty region at the
  menu boundary, and `WM_DrawDesktopInRect()` redraws menu, root desktop,
  separator, and screen border while preserving the caller's clip.
- Window redraw clipping is now a tested dirty-rect contract too:
  `DR_PlanWindowRedraw()` clips dirty regions to window bounds. The boot-safe
  first frame still uses a compact direct BLT window renderer by default.
  `DESKTOP_WM_PROBE=1` now proves the exact minimal `WM_Init()` +
  `WM_NewWindow()` allocation/z-order/render path, but that is still an opt-in
  rung, not permission to re-enable the whole desktop/app loop.
- No maintained native Mega Drive desktop OS reference was found in this pass;
  that pushes SegaOS toward Sega CD hardware references plus 68k GEM/TOS
  desktop architecture, not toward inventing a GUI stack from scratch.

## Bring-Up Discipline

- Do not advance broad desktop features without a repeatable probe for the
  lower-level rung.
- The stable ladder is:
  1. Disc layout verification
  2. Main IP breakpoint
  3. Sub `sp_init` / `sp_main`
  4. Gate Array command/status
  5. Word RAM handoff
  6. Main framebuffer upload and VRAM readback
  7. Main-only direct VDP text canary
  8. Visible BlastEm internal screenshot
  9. Repeated-frame Word RAM return/reacquire probe
  10. Short multi-frame single-bank render/upload loop probe
  11. Full-frame upload HV/status timing probe
  12. Minimal `WM_NewWindow()` allocation/z-order render probe
- A visible screenshot is useful but not sufficient. Pair it with GDB symbols or
  VRAM readback so the result is not confused with the Sega CD BIOS screen or an
  old boot pattern.
- For default boot-safe desktop screenshots, prefer the deterministic visual
  probe path: build `BOOT_SAFE_VISUAL_PROBE=1`, launch BlastEm with
  `tools\capture_blastem_internal_screenshot.ps1 -DebugAutoBoot`, require GDB
  to hit `segaos_visual_probe_halt` with phase `0x76ff`, then let BlastEm's
  internal `ui.screenshot` binding capture the resumed app frame.
- When switching between probe builds and the normal build, use `-B` or clean
  first. The current Makefile reuses shared object paths, so a plain
  `make iso` after `BOOT_SAFE_VISUAL_PROBE=1` can keep probe-compiled Main CPU
  objects. The normal default Main CPU size observed after a forced rebuild is
  `2776` bytes.
- Probe scripts that issue a raw GDB `continue` need bounded failure behavior
  inside the script, not only an outer shell timeout. `probe_blastem_boot.ps1`
  now accepts `-GdbTimeoutSeconds`, reports `probe_gdb_timeout=True` if GDB
  does not return, and has a fake-GDB host regression under `make host-tests`.
  A fresh real `DesktopTiming` run now reaches phase `0x84ff` with
  `probe_gdb_timeout=False`.
- Probe sample coordinates must stay tied to the rendered primitive. A stale
  text probe sampled the white body row at `y=73` after the sysfont probe text
  moved to `y=86`. The SGDK-font probe now checks both the tile-aligned "S"
  row at `x=64`, `y=83` (`0xffff/0xff11`) and the full 24x24 scaled first
  glyph signature (`0xd2dd`) in Word RAM and VDP tile data.

## Word RAM Ownership

- In the observed 1M boot state, `MEM_MODE` starts around `0x2a05`: RET is set
  and Sub can write its `$0C0000` bank.
- Clearing RET from Sub exposes that same bank to Main at `$200000`; this is the
  proven first-frame handoff.
- Main releases the displayed bank by writing RET from the Main side. In
  BlastEm, the released/repeated-frame state observed by the probe is
  `MEM_MODE=0x2a06` (DMNA set, RET clear), not the initial `0x2a05` state.
- Because the initial BIOS/Sub-owned state and the Main-released state present
  differently, Sub-side "can write Word RAM" checks in the boot-safe 1M path
  must accept either RET or DMNA.
- `DESKTOP_REPEAT_PROBE=1` proves a two-frame single-bank ping-pong: first
  render, Main upload, Main release, second render, Main upload, second title
  row still visible in VDP as `0xf11f/0x1f11`, with terminal trace `0x7404`.
- `DESKTOP_LOOP_PROBE=1` proves that path can survive a short loop: after the
  first frame, four additional render/upload/return cycles complete with loop
  count `0x0004`, status `0x0003`, trace `0x7404`, MEM_MODE `0x2a06`, and
  final title-row VRAM `0xf11f/0x1f11`.
- Do not assume full alternating 1M double buffering is solved. The proven path
  is still the boot-safe single-bank `$0C0000`/`$200000` handoff, not a
  production long-running frame scheduler.

## Framebuffer Writes

- The Main framebuffer converter expects linear 4bpp bytes:
  160 bytes per row, high nibble = left pixel.
- The Sub boot-safe display path must use word-safe access when targeting Word
  RAM. BLT's original byte-oriented framebuffer writes produced unreliable
  boot-safe output.
- BLT framebuffer reads/writes now go through 16-bit Word RAM helpers while
  preserving the same 4bpp byte/nibble layout for Main's `FB_UpdateFrame()`.
- Main's framebuffer converter must also read returned Word RAM as 16-bit
  words, then unpack the bytes locally. Byte-copying the returned bank can make
  small glyph strokes appear corrupt even when the source glyph data is real.
- Keep the conversion logic testable without changing that hardware rule.
  `FB_ConvertTileSpan()` is now the shared seam: host tests exercise byte-order
  and tile-index math with normal byte reads, while the Main CPU build still
  uses 16-bit Word RAM reads before unpacking to VDP tile bytes.
- Dirty upload work should consume `DirtyTileQueue` spans through
  `FB_ConvertTileSpan()` before issuing any VRAM write. That separates three
  risks: dirty region ownership, linear-to-tile conversion, and live VDP timing.
- The queue consumer now exists as `FB_FlushTileQueueWithCallback()`. It is
  host-tested against the 5,120-byte strip buffer shape: a 235-tile
  VBlank-budgeted upload splits into 160 and 75 tile chunks with VRAM addresses
  derived directly from `firstTile * 32`.
- The Main-side wrapper is now emulator-proven only in a narrow diagnostic
  path. `DESKTOP_DIRTY_QUEUE_PROBE=1` constructs a one-entry queue for title
  tile `0x0147`, poisons the sampled VRAM row, calls `FB_UpdateTileQueue()`,
  reaches phase `0x85ff`, and reads `0xf11f/0x1f11` back from VRAM. This proves
  the queue-to-DMA path can work on hardware-shaped emulation; it does not yet
  prove the final VBlank scheduler or long-running frame policy.
- Source glyph/bitmap reads can remain normal byte reads because they come from
  PRG-RAM/rodata, not Word RAM.
- If `VDP_TEXT_PROBE=1` displays readable text but desktop text is corrupted,
  stop debugging the font. The remaining bug is in the scaled Word RAM
  framebuffer/compositor/upload path above the VDP tile primitive.
- VDP background-plane palette index 0 is transparent. Do not use color index 0
  as opaque framebuffer ink in a visible 4bpp desktop layer. Keep index 0 as
  transparent/backdrop black and use a nonzero black entry, currently palette
  index 1, for text strokes, borders, and other opaque black pixels. The
  symptom was GDB-proven correct Word RAM/VRAM bytes that still looked sparse
  or corrupted on screen because black glyph pixels were transparent.

## Command Loop

- READY must be published from the C runtime path after `sp_main` reaches the
  command loop, not from early BIOS callbacks.
- Treat CFM as a pending signal, not as the opcode. The emulator-proven
  protocol writes the opcode to `CMD0`, parameters to `CMD1`-`CMD4`, then sets
  CFM to `COMM_MAIN_PENDING` (`0x02`). `CMD_BOOT_PROBE` in the assembly SP and
  normal C commands both follow that rule.
- The default boot-safe command wait loop should stay aligned with the
  probe-proven polling path until the full long-running frame policy is
  understood.
- Keep command breadcrumbs in COMSTAT registers while debugging, but avoid
  overwriting the trace value that a probe is asserting.
- After `sub_done()`, do not let the idle loop overwrite terminal command
  traces such as `0x7404`; Main/GDB may capture status after the Sub command
  loop has already returned to idle.
- A short multi-frame loop is now proven only in the boot-safe single-bank
  probe harness. Keep production scheduler work behind its own measured
  timing/banking decision instead of assuming the probe loop is the final loop.

## VDP Timing

- The golden 68k OS sources help with GUI ownership, redraw discipline, and
  event layering; they do not answer Genesis VDP transfer timing. That policy
  must come from Sega CD hardware evidence.
- `DESKTOP_TIMING_PROBE=1` is the current first measurement rung. It profiles
  the boot-safe full-frame upload as 7 strip DMA transfers, reaches phase
  `0x84ff`, and passes `-Probe DesktopTiming`.
- The current BlastEm sample records HV `0xbc1d` before the profiled upload and
  `0xfdb2` after it, final VDP status `0x320c`, transition mask `0x007f`, and
  DMA-clear mask `0x007f`.
- Interpret that narrowly: every strip consumed measurable HV time and DMA was
  clear after each strip. This does not yet prove a production VBlank-only
  dirty-tile queue, acceptable active-display artifacts, or alternating
  double-buffer timing.
- `DR_TileRangeBudget()` and `DR_QueueTileRange()` are the first clean-room
  bridge from the GEM/TOS-style dirty-region model to Genesis VDP transfer
  limits. They do not copy or closely port GEOS, GEM/TOS, CP/M-68K, Megadev, or
  SGDK queue code. They turn dirty tile ranges into first-tile/tile-count/
  byte-count facts and explicit upload spans.
- The dirty tile queue is still a planner, not a hardware backend. Partial
  width ranges become one upload span per tile row; full-width ranges become a
  contiguous span; `maxBytes` slices a span to the caller's frame budget; and
  `budgetExceeded` is kept separate from queue-storage `overflow`.
- The next VDP implementation step should be a scheduler/frame-loop gate around
  `FB_UpdateTileQueue()`, not another proof that the wrapper can upload one
  tile. Do not reintroduce full-frame policy assumptions while moving from the
  diagnostic dirty upload to production scheduling.
- The concrete frame-policy warning is now host-tested: a full 40x28 tile
  4bpp frame is 1,120 tiles / 35,840 bytes, so it cannot fit the 7,524-byte
  NTSC VBlank budget recorded in the Mega Drive development notes. The queue
  planner slices that full-frame request to one 235-tile / 7,520-byte upload
  for a single budgeted pass, while a small 2x2-tile dirty range is 4 tiles /
  128 bytes and fits that budget.

## Visual Target

- The immediate product goal remains a 68k Mac-like experience on Sega CD.
- Plan product features as if an external Backup RAM cartridge-class writable
  store is available. That changes the realistic target from demo-only saves
  to a small but usable document workflow: text files, tokenized BASIC
  programs, tiny databases, settings, and imported/exported app data. Internal
  8 KB Backup RAM should remain the compatibility fallback, not the design
  ceiling.
- Use `STG_PlanSave()` before app save UI promises persistence. The current
  host-tested policy prefers external cart storage, allows internal BRAM only
  for preferences and tiny text/BASIC fallback saves, reserves free space on
  both targets, and rejects image saves without the external cart path.
- Build BASIC as an OS tool in narrow stages: program buffer plus shell line
  entry/`LIST`/`NEW` first, expression values second, minimal `RUN` for
  sequential `PRINT`/`END` third, literal-line `GOTO` with a step cap fourth,
  fixed A-Z integer `LET` variables fifth, integer `IF`/`THEN` branching sixth,
  callback-backed integer `INPUT` seventh, broader statement execution eighth,
  desktop I/O and storage last. The current BASIC core is clean-room SegaOS
  code. No GEOS,
  GEM/TOS, CP/M-68K, Megadev, or SGDK interpreter source is copied or closely
  ported; this first primitive is small enough that a direct interpreter
  reference would add licensing/runtime mismatch before it adds value.
- The current visible target is intentionally modest: checker desktop, menu
  separator, and a clean window starter frame with real SGDK-font menu, title,
  and body text.
- Plain body text and striped title-bar composition are separate risks. The old
  hand-authored 6x10 placeholder sysfont was not a defensible target. It has
  been replaced with SGDK v2.11's real 8x8 font, and
  `BOOT_SAFE_TEXT_PROBE=1` + `DESKTOP_INIT_PROBE=1` proves the first scaled "S"
  as a full-glyph signature (`0xd2dd`) in both Word RAM and VDP tile data.
- The same text probe now reads Plane A entries under the first scaled "S" and
  expects `0x0198/0x0199/0x019a`, proving the visible plane points at the
  verified text tiles in the GDB-driven run.
- A lower direct VDP canary now passes too: `VDP_TEXT_PROBE=1` draws `SEGAOS`
  and `TEXT OK` from SGDK-derived 8x8 tiles, `-Probe VdpText` verifies tile row
  `0x00ff/0xff00` and Plane A entries `0x0001/0x0002/0x0003`, and BlastEm
  internal screenshotting captured readable text at
  `C:\tmp\segaos_screens_internal\segaos_vdp_text_direct_20260630_181733.png`.
- Keep text probes separate from title-bar stripes. Plain body text should be
  visually accepted before the active Mac-style striped title composition is
  restored.
- The default boot-safe text/title canary is memory-proven separately:
  `BOOT_SAFE_TITLE_PROBE=1` + `DESKTOP_INIT_PROBE=1` verifies a sampled default
  body text row as `0xf11f/0x1f11` in both Word RAM and VDP tile data.
- Latest accepted default internal screenshot, captured with
  `BOOT_SAFE_VISUAL_PROBE=1` and `-DebugAutoBoot` after GDB proved phase
  `0x76ff`:
  `C:\tmp\segaos_screens_internal\segaos_repeat_20260630_231605.png`.
  The older block-canary frame at
  `C:\tmp\segaos_screens_internal\segaos_default_20260629_211333.png` is only a
  historical reference.
- Latest accepted opt-in desktop-composited SGDK-font text probe screenshot:
  `C:\tmp\segaos_screens_internal\segaos_desktop_text_opaque_20260630_183441.png`.
  The earlier `C:\tmp\segaos_screens_internal\segaos_text_probe_20260630_114924.png`
  and `C:\tmp\segaos_screens_internal\segaos_desktop_text_probe_20260630_182543.png`
  captures are known-bad references from before index-0 transparency was fixed.
- A default autoplay capture can still stop at the Sega CD BIOS screen if START
  is not delivered to BlastEm. Treat that as capture automation evidence, not a
  rendering regression. Do not use BIOS-screen captures as default desktop
  acceptance evidence.
- `WM_DrawDesktop()` can own the desktop/menu shell, but the boot-safe first
  render should stay compact until each added WM feature has a probe.
- `WM_DrawDesktopInRect()` is the root redraw gate: keep it clip-preserving so
  the Sub CPU dirty loop can draw root pixels and then continue with windows
  under the same dirty clip.
- Minimal boot-safe window furniture is now accepted through the dirty-list
  loop with fixed local rectangles, real SGDK-font title/body text, and no app
  callbacks. This is a startup renderer, not proof that the full window manager
  allocation/z-order path is safe.
- The earlier `WM_NewWindow()` boot-render regression was caused by the
  freestanding libc/builtin issue above, not by an inherent problem with
  allocation or z-order traversal. `DESKTOP_WM_PROBE=1` now proves one active,
  visible document window with flags `0x0007` at frame origin `40,34`, rendered
  through the dirty-window clip path. The accepted WM-backed internal screenshot
  is
  `C:\tmp\segaos_screens_internal\segaos_wm_probe_20260630_235603.png`. The
  long-running desktop loop still needs its own bank/timing policy before
  menu/cursor/app callbacks return.
- `DESKTOP_LOOP_PROBE=1` is the current evidence that the boot-safe direct
  renderer can run repeated single-bank frame cycles without losing the title
  text in VDP. It is not evidence that mouse/menu/app callbacks are ready.
- `DESKTOP_TIMING_PROBE=1` gives the first timing evidence for the same
  boot-safe direct renderer. It is not permission to skip the VDP transfer
  policy decision before reintroducing broad desktop/app rendering.
