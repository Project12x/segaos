# Impressive Tech Demo Goal

This project is still pre-alpha, but its direction should be concrete: a stock
Sega CD should look and behave like a small 68k GUI computer, not like a game
mocking a desktop screenshot.

## References Checked

- Sega CD hardware and boot shape: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`,
  MIT, pattern reference for IP/SP, boot sector, Gate Array, BRAM, and CD-ROM
  structure; direct copied only where already attributed (`src/main/security.c`).
- VDP upload scheduling: `Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c`,
  MIT, pattern-only / clean-room reference for queued DMA discipline.
- Desktop architecture: EmuTOS, FreeMiNT/XaAES, and OpenGEM references recorded
  in `68k_desktop_prior_art.md`, GPL-family or mixed; pattern-only / clean-room
  behavior lessons for VDI/AES/Desktop layering, redraw ownership, and events.
- Product shape: `full_stack_cleanroom_research.md` records the SegaOS-CD
  conclusion that the most impressive stock-hardware target is a constrained
  but real GUI computer.

## Demo Bar

The impressive demo should prove capability, not just appearance:

- Boot directly into a readable Mac-like desktop shell.
- Show real windows with owned redraw, clipping, active/inactive state, and a
  stable pointer/input path.
- Open at least one useful tool window, not only a decorative frame.
- Load something from CD-ROM or a fixed disc resource.
- Save or load a small user document through Backup RAM policy.
- Show at least one format users recognize: plain text, tokenized BASIC, or a
  small image asset.
- Leave durable evidence: BlastEm internal screenshot plus GDB/VRAM/BRAM proof
  for the low-level path.

## First Showpiece Script

1. Boot to the desktop with readable menu/title/body text.
2. Open a document window that shows a small text file or built-in README from
   CD/resource storage.
3. Open a BASIC tool window that can `LIST`, `RUN`, `SAVE`, and `LOAD` a tiny
   program through the current storage adapter.
4. Show a small image viewer with a preconverted asset first. GIF import can be
   added later after a provenance pass for a compact decoder or a clean-room
   limited parser. The current framebuffer path is 16-color; a 64-color-looking
   image demo needs either tile/palette partitioning or a documented
   downconversion rule.
5. Capture the final scene with debugger-backed BlastEm internal screenshotting.

## Engineering Filter

Invisible plumbing is justified only when it unlocks one of these demo effects:

- stable long-running frame transfer;
- window/event ownership;
- text or image document loading;
- BASIC editor/runtime interaction;
- save/load persistence;
- input ergonomics;
- CD-ROM resource access;
- hardware compatibility evidence.

If a change does not move one of those outcomes closer, keep it out of the
critical path.

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

After that passes, the showpiece can safely spend effort on the BASIC/text
tool window because changing user-visible content will have a proven transport
path.

Current frame-transfer evidence: `DESKTOP_PUMP_PROBE=1` proves the compact
Main-side upload policy can consume a full Sub-rendered frame as five budgeted
tile uploads, return Word RAM only after the final slice, and repeat that
render/upload/return cycle four times in BlastEm/GDB. The default boot-safe
first frame now uses that compact pump path and has a debugger-backed BlastEm
screenshot at
`C:\tmp\segaos_screens_internal\segaos_pump_default_20260703_164252.png`.
`BOOT_SAFE_LIVE_PROBE=1` proves the default boot-safe `main_loop()` can drive
four post-boot render/upload/return cycles in GDB, ending at phase `0x89ff` and
frame count `0x0004`. The visible latest-frame policy is still open: bank-0
screenshots remain stale (`Frame 0`/`Frame 1`), and blind bank-1 upload is
corrupt. The next demo-facing step is not more widgets; it is proving the
latest returned frame can be displayed reliably so BASIC/text/image windows can
change without falling back to one-shot full-frame uploads.
