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
