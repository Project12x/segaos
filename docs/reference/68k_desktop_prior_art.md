# 68k Desktop Prior Art

This note records the June 2026 reference pass triggered by the corrupted
boot-safe desktop/title output. The current goal is unchanged: build a 68k
Mac-like desktop experience on Sega CD. The implementation path should change:
stop adding broad window-manager features until text, clipping, redraw
ownership, and the desktop shell are split into smaller probeable contracts.

## Scope

Sega CD references such as Megadev remain the authority for boot layout, IP/SP
shape, Gate Array behavior, CD-ROM ownership, and Word RAM constraints. They do
not answer how to structure a small desktop GUI.

For that GUI architecture, the best available references are 68000-era desktop
systems in the GEM/TOS family. "Gemos" is treated here as GEM/GEMDOS/TOS unless
a different project is identified later. GEOS is useful as historical UX
context. PC/GEOS is also a plausible permissive source candidate for later
study, but it targets x86/DOS-era assumptions, not Sega CD. CP/M-68K is useful
for the BDOS/BIOS separation and small-machine discipline, but source and
license provenance still need a dedicated pinning pass before any
implementation uses it.

No maintained native Mega Drive desktop operating-system reference was found in
this pass. That is not a blocker: SegaOS should take hardware bring-up and
rendering constraints from Sega CD references, while taking desktop structure
from 68k GEM/TOS prior art.

## References Inspected

| Project | Repo | Pinned Commit | License | SegaOS Reuse Mode |
|---|---|---|---|---|
| EmuTOS | https://github.com/emutos/emutos | `bc34e0b7e7850c5c45a909409d11f7c75ecdc881` | GPL-2.0-or-later style headers / GPL text in `doc/license.txt` | pattern-only, clean-room behavior specs |
| FreeMiNT | https://github.com/freemint/freemint | `3172633539fb4281bc3b23c322892f565f303c16` | mixed; `COPYING` maps major trees including GPL and MiNT terms | pattern-only, clean-room behavior specs |
| OpenGEM | https://github.com/shanecoughlan/OpenGEM | `ac06b1a3fec3f3e8defcaaf7ea0338c38c3cef46` | GPL for GEM/FreeGEM/OpenGEM sections per SDK README | historical/API pattern-only |

## Candidate Golden References Not Yet Adopted

These are explicitly on the source-reference list because they answer real
questions about small OS structure or desktop UX, but they are not yet adopted
implementation references for SegaOS:

| Project | Repo / Source | Pinned Point | Current Licensing Read | SegaOS Status |
|---|---|---|---|---|
| PC/GEOS | https://github.com/bluewaysw/pcgeos | `867154f966314155fdc2ee04593b21c0a5f6e724` | GitHub reports Apache-2.0; file-level inspection still required before reuse | candidate reference for desktop/app architecture and docs; no code inspected for porting yet |
| GEOS 2.0 C64/C128 reverse-engineered source | https://github.com/mist64/geos | `617efdc7191eda14eaf699d317e66d5bda68c039` | license not pinned in this pass | historical UX/resource-constraint reference only |
| CP/M / CP/M-68K | Digital Research/CP/M archives and historical source mirrors | not pinned | licensing/source provenance not pinned in this pass | behavior reference for BDOS/BIOS split and file/process discipline only |

Do not copy, closely port, or mechanically translate code from these GPL-family
references into SegaOS. For candidate non-GPL references, do not copy or closely
port until a future pass records the exact repo, commit, license file, files
inspected, and reuse mode.

## EmuTOS

Files inspected:

- `aes/gemwmlib.c`
- `aes/gemwrect.c`
- `aes/gemevlib.c`
- `aes/gemgsxif.c`
- `vdi/vdi_text.c`
- `vdi/vdi_line.c`
- `desk/deskwin.c`
- `desk/deskmain.c`
- `desk/deskobj.c`
- `doc/emudesk.txt`
- `doc/osmemory.txt`
- `doc/license.txt`

Useful lessons:

- GEM's split maps well to SegaOS:
  - VDI: drawing, text, clipping, and device/screen interface.
  - AES: events, windows, object ownership, and redraw dispatch.
  - Desktop: the shell/application layer that uses AES and VDI.
- Window redraw is not just "draw a frame." EmuTOS keeps owner rectangle lists,
  walks visible window rectangles, clips before drawing, and redraws the desktop
  root separately from window furniture.
- Text is a first-class VDI subsystem, not a side effect of window drawing.
  Font metrics, width/height, alignment, scratch buffers, and direct screen
  writes are isolated concerns.
- Desktop objects and windows are separate: the desktop owns root icons/menu
  behavior while windows own content rectangles and app callbacks.
- Internal memory is pool-like and explicit. That matches SegaOS's no-heap
  hardware constraint.

Clean-room SegaOS translation:

- Introduce a `Gfx`/VDI-like layer for primitive drawing, text, clipping, and
  tile invalidation.
- Introduce an AES-like window/redraw layer with a static rectangle pool and
  owner lists before enabling z-ordered application windows.
- Keep the desktop shell as a client of those layers rather than the place where
  low-level rendering behavior lives.

## FreeMiNT / XaAES

Files inspected:

- `README.md`
- `COPYING`
- `sys/init.c`
- `xaaes/ChangeLog.Craig`

Useful lessons:

- FreeMiNT is heavier than SegaOS needs, but its history reinforces that redraw
  bugs dominate GUI bring-up.
- XaAES change notes repeatedly mention clipping, redraw lists, root-window
  redraw, window locking, message ordering, and input dispatch.
- A multitasking Atari kernel is not the target, but the redraw and event
  contracts are relevant because they document real failure modes in a GEM-like
  UI.

Clean-room SegaOS translation:

- Make dirty/owned rectangles a tested data structure instead of an incidental
  loop inside the window manager.
- Treat root desktop redraw as its own contract.
- Do not add app event routing until window ownership and redraw order are
  deterministic.

## OpenGEM / FreeGEM

Files inspected:

- `source/OpenGEM-7-RC3-SDK/README.TXT`
- top-level SDK/source layout

Useful lessons:

- OpenGEM is useful as historical GEM API/UX context and a reminder that GEM can
  be small and non-multitasking.
- It is less useful as source for SegaOS because it targets DOS/x86-era build
  assumptions and is GPL.

Clean-room SegaOS translation:

- Keep it as behavior/API vocabulary only: desktop, AES-style window calls, VDI
  drawing calls, accessories/apps.
- Prefer EmuTOS for 68k structure and FreeMiNT/XaAES for redraw failure modes.

## Architectural Decision

Do not continue by pushing the full `WM_NewWindow()` path into the boot frame.
The earlier visible block `OS` canary proved that the boot-safe path could draw
recognizable pixels, but the missing readable title text and prior corrupted
output proved that the graphics stack was not decomposed enough.

Status update, 2026-06-30: the first text primitive rung is complete. SegaOS now
uses SGDK v2.11's MIT 8x8 font, proves direct VDP tile text through
`VDP_TEXT_PROBE=1`, proves desktop-composited scaled text through
`BOOT_SAFE_TEXT_PROBE=1`, and captures the restored default menu/title/body
frame at
`C:\tmp\segaos_screens_internal\segaos_dirty_rect_final_20260630_194506.png`
through debugger-backed BlastEm internal screenshotting.

Status update, 2026-06-30 later pass: the dirty rectangle data-structure rung is
also complete. `src/sub/dirty_rect.c` is clean-room SegaOS code, not a port from
the GPL-family references. `make host-tests` covers half-open intersections,
bounds clipping, deterministic subtraction strips, edge-touch merge,
corner-touch separation, overflow collapse to a bounding rect, and 8x8
tile-range rounding.

Status update, 2026-06-30 final pass: the root desktop redraw rung is complete.
`DR_PlanRootRedraw()` is clean-room SegaOS code that splits dirty root regions
into menu and desktop portions, and `WM_DrawDesktopInRect()` redraws root pixels
while preserving the caller's clip. The reference basis remains pattern-only
from the GPL-family GEM/TOS notes above; no upstream source was copied or
closely ported. `make host-tests`, `make iso`, and debugger-backed BlastEm
internal screenshotting passed with the accepted frame at
`C:\tmp\segaos_screens_internal\segaos_root_redraw_20260630_211404.png`.

Status update, 2026-06-30 late pass: the minimal boot-safe window-furniture rung
is complete for the direct startup renderer. `DR_PlanWindowRedraw()` is
host-tested clean-room SegaOS code for dirty-window clipping, and the boot-safe
first frame draws a compact window frame/title/body under the dirty-list clip
without app callbacks or `WM_NewWindow()` allocation. DesktopInit proves the
first render/upload path with terminal trace `0x7404`; the accepted visual frame
is
`C:\tmp\segaos_screens_internal\segaos_repeat_20260630_231605.png`.

Status update, 2026-06-30 repeated-frame pass: `DESKTOP_REPEAT_PROBE=1` proves
the same direct startup renderer can return Word RAM to Sub and complete a
second render/upload cycle. The second command reaches status `0x0003`, trace
`0x7404`, terminal phase `0x82ff`, and repeated title-row VRAM
`0xf11f/0x1f11`. This does not make the full window-manager/app loop safe; it
removes the repeated-frame handoff as the next immediate blocker.

Status update, 2026-06-30 WM probe pass: `DESKTOP_WM_PROBE=1` proves the first
real window-manager allocation/z-order boot render rung. The probe runs
`WM_Init()`, creates one document window through `WM_NewWindow()`, traverses it
bottom-to-top, clips redraw with `DR_PlanWindowRedraw()`, and reports window
count `0x0001`, active flags `0x0007`, frame origin `0x2822`, and terminal
trace `0x7404`. No GEM/TOS/GEOS/CP/M code was copied or closely ported; the
implementation remains clean-room. The blocker turned out to be toolchain
contract, not desktop architecture: m68k GCC had optimized SegaOS' local
`memset()` into a recursive self-call until the Makefile added
`-ffreestanding -fno-builtin`. The same WM-backed boot frame is visually
accepted through debugger-backed BlastEm internal screenshotting at
`C:\tmp\segaos_screens_internal\segaos_wm_probe_20260630_235603.png`.

Status update, 2026-07-01 loop probe pass: `DESKTOP_LOOP_PROBE=1` proves the
boot-safe single-bank direct renderer can complete a short repeated frame loop
without reintroducing text corruption. After the first frame, the probe drives
four more render/upload/return cycles, reaches phase `0x83ff`, reports loop
count `0x0004`, status `0x0003`, trace `0x7404`, MEM_MODE `0x2a06`, and final
title-row VRAM `0xf11f/0x1f11`. This is still clean-room SegaOS probe work; no
GEM/TOS/GEOS/CP/M code was copied or closely ported. It removes the immediate
"can the single-bank path repeat more than once?" blocker, but it is not the
measured VBlank scheduler or alternating double-buffer policy.

The remaining implementation rungs are:

1. Long-running frame policy:
   - keep full-frame upload as bring-up until the VDP timing policy is measured;
   - decide whether production uses single-bank bring-up, alternating 1M
     double buffering, or another transfer strategy.
2. Application content and event routing:
   - add app-owned content rectangles;
   - route mouse/key events only after visible ownership and redraw are stable.

Acceptance for each rung must include either a host test, a GDB/VRAM probe, a
BlastEm internal screenshot, or a narrow combination of those. A screenshot
alone is not sufficient for low-level correctness.
