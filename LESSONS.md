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
  7. Visible BlastEm internal screenshot
- A visible screenshot is useful but not sufficient. Pair it with GDB symbols or
  VRAM readback so the result is not confused with the Sega CD BIOS screen or an
  old boot pattern.

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
- Source glyph/bitmap reads can remain normal byte reads because they come from
  PRG-RAM/rodata, not Word RAM.

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
  separator, and document-window outline.
- The current visual milestone is a Word RAM-safe BLT path drawing the starter
  frame through normal rectangle/pattern primitives.
- Next visual milestone is to move from hand-authored BLT starter drawing to
  normal window-manager objects, menu bar drawing, text, and cursor.
