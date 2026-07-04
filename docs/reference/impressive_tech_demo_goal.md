# Impressive Runtime Demo Goal

This project is still pre-alpha, but its direction should be concrete: a stock
Sega CD should behave like a small GUI operating environment, not like a game
mocking a desktop screenshot and not like a suite of built-in toys.

The stronger benchmark is GEOS/GEM/Contiki-style discipline: app boundaries,
system services, storage abstraction, resource loading, and a shell that can
launch more than one useful program. The detailed runtime target is
`loadable_app_runtime_goal.md`.

## References Checked

- Sega CD hardware and boot shape: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`,
  MIT, pattern reference for IP/SP, boot sector, Gate Array, BRAM, and CD-ROM
  structure; direct copied only where already attributed (`src/main/security.c`).
- VDP upload scheduling: `Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c`,
  MIT, pattern-only / clean-room reference for queued DMA discipline.
- Desktop architecture: EmuTOS, FreeMiNT/XaAES, and OpenGEM references recorded
  in `68k_desktop_prior_art.md`, GPL-family or mixed; pattern-only / clean-room
  behavior lessons for VDI/AES/Desktop layering, redraw ownership, and events.
- Runtime shape: `loadable_app_runtime_goal.md` records the SegaOS-CD decision
  that the impressive stock-hardware target is a loadable GUI app environment,
  not only a constrained GUI computer.

## Demo Bar

The impressive demo should prove operating-environment capability, not just
appearance:

- Boot directly into a readable Mac-like desktop shell.
- Show real windows with owned redraw, clipping, active/inactive state, and a
  stable pointer/input path.
- Show an app catalog or equivalent CD-backed app/resource selection path.
- Launch at least one app that is separate from the shell by descriptor,
  module, table boundary, or later CD load format.
- Deliver an input/event callback to that app through SegaOS services.
- Make that app draw only through SegaOS drawing/text services.
- Save or load a small user document through Backup RAM policy without the app
  knowing raw BRAM details.
- Exit the app cleanly and launch another app or return to the shell.
- Show at least one format users recognize: plain text, tokenized BASIC, or a
  small image asset.
- Leave durable evidence: BlastEm internal screenshot plus GDB/VRAM/BRAM proof
  for the low-level path.

## First Showpiece Script

1. Boot to the desktop with readable menu/title/body text.
2. Open a CD-backed app catalog or app list.
3. Launch `TEXT.APP` or an equivalent first app boundary.
4. The app requests a document window, receives an event, and draws text through
   SegaOS services.
5. Save or load a small text document through the SegaOS storage service.
6. Close the app, return to the shell, then launch `BASIC.APP` or a second
   utility app without rebooting.
7. `BASIC.APP` can `LIST`, `RUN`, `SAVE`, and `LOAD` a tiny program through the
   same storage service.
8. Later, add a small image viewer app with a preconverted asset first. GIF
   import can be added after a provenance pass for a compact decoder or a
   clean-room limited parser. The current framebuffer path is 16-color; a
   64-color-looking image demo needs either tile/palette partitioning or a
   documented downconversion rule.
9. Capture the final scene with debugger-backed BlastEm internal screenshotting.

## Engineering Filter

Invisible plumbing is justified only when it unlocks one of these demo effects:

- stable long-running frame transfer;
- app catalog, ABI, launch, and exit ownership;
- window/event ownership;
- text or image document loading;
- BASIC editor/runtime interaction;
- save/load persistence;
- input ergonomics;
- CD-ROM resource access;
- hardware compatibility evidence.

If a change does not move one of those outcomes closer, keep it out of the
critical path. Built-in tools are acceptable as temporary bring-up rungs, but
they should not be mistaken for the final runtime result.

## Immediate Proof Ladder

The next proof must connect emulator-visible pixels to the newest returned
frame, not just prove that the loop ran:

1. Render a changing marker or small text change in the Sub CPU frame.
2. Return Word RAM only after the compact pump finishes the current frame.
3. Select the correct Main-side Word RAM window or conversion path for the next
   upload.
4. Capture with BlastEm internal screenshotting after a GDB phase/frame-count
   check.
5. Accept only if the screenshot-visible marker matches the debugger counter.

After that passes, the showpiece can safely spend effort on the app-runtime
boundary because changing user-visible content will have a proven transport
path.

Current frame-transfer evidence: `DESKTOP_PUMP_PROBE=1` proves the compact
Main-side upload policy can consume a full Sub-rendered frame as five budgeted
tile uploads, return Word RAM only after the final slice, and repeat that
render/upload/return cycle four times in BlastEm/GDB. A fresh pump screenshot
now shows the visible marker reaching `Frame 4` through the bank-0 linear path:
`C:\tmp\segaos_screens_internal\segaos_pump_bank0_20260704_093959.png`. The
default boot-safe first frame now uses that compact pump path and has a
debugger-backed BlastEm screenshot at
`C:\tmp\segaos_screens_internal\segaos_pump_default_20260703_164252.png`.
`BOOT_SAFE_LIVE_PROBE=1` proves the default boot-safe `main_loop()` can drive
four post-boot render/upload/return cycles in GDB and now proves the visible
latest-frame policy for the boot-safe full-frame pump. The root cause of the
stale screenshots was a 1M RET handoff bug: Sub kept rendering into the bank
not being uploaded while Main kept using stale bank 0. The current probe toggles
RET on Sub return and makes Main upload the bank whose sentinel matches the
requested frame. The final proof reaches terminal phase `0x89ff`, frame count
`0x0004`, pre-upload phase `0x8904`, pre-upload frame count `0x0003`, and
sentinel `0x4444`, with a debugger-backed BlastEm internal screenshot at
`C:\tmp\segaos_screens_internal\segaos_live_current_20260703_193928.png`
showing `Frame 4`. The next demo-facing step is adopting this current-bank
policy in the production dirty/VBlank loop without treating bank 1 as linear
framebuffer data. `$220000` needs a bank-1 tile/pixel-window transfer policy
before app-owned BASIC/text/image windows can change safely under true
alternating 1M double buffering.
