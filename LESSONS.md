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
- For concrete UI assets such as fonts, do not invent vague placeholder
  glyphs and then debug against them. The current system font is SGDK v2.11's
  MIT 8x8 `font_default.png`, pinned at
  `Stephane-D/SGDK@ef9292c03fe33a2f8af3a2589ab856a53dcef35c` and converted
  into SegaOS' 1bpp glyph format with attribution in `third_party/sgdk_font/`.

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
- Root desktop redraw should be a separate contract from window furniture and
  app content callbacks.
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
- A visible screenshot is useful but not sufficient. Pair it with GDB symbols or
  VRAM readback so the result is not confused with the Sega CD BIOS screen or an
  old boot pattern.
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
- Setting RET from Main returns bank 0 to Sub for the next render attempt.
- Do not assume full alternating 1M double buffering is solved. Only the first
  Sub `$0C0000` to Main `$200000` handoff is proven.

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
- The default boot-safe command wait loop should stay aligned with the
  probe-proven polling path until repeated-frame behavior is understood.
- Keep command breadcrumbs in COMSTAT registers while debugging, but avoid
  overwriting the trace value that a probe is asserting.

## Visual Target

- The immediate product goal remains a 68k Mac-like experience on Sega CD.
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
- Latest accepted default internal screenshot, from the older block-canary
  frame:
  `C:\tmp\segaos_screens_internal\segaos_default_20260629_211333.png`.
- Latest accepted opt-in desktop-composited SGDK-font text probe screenshot:
  `C:\tmp\segaos_screens_internal\segaos_desktop_text_opaque_20260630_183441.png`.
  The earlier `C:\tmp\segaos_screens_internal\segaos_text_probe_20260630_114924.png`
  and `C:\tmp\segaos_screens_internal\segaos_desktop_text_probe_20260630_182543.png`
  captures are known-bad references from before index-0 transparency was fixed.
- A default autoplay capture can still stop at the Sega CD BIOS screen if START
  is not delivered to BlastEm. Treat that as capture automation evidence, not a
  rendering regression.
- `WM_DrawDesktop()` can own the desktop/menu shell, but the boot-safe first
  render should stay compact until each added WM feature has a probe.
- Moving `WM_NewWindow()` into the boot render path regressed command-loop
  consumption before the first command was handled. Treat full WM allocation and
  z-order traversal as later rungs. The next isolated rungs are fixed-font text,
  dirty rectangles/clipping, root desktop redraw, and then minimal window
  furniture.
