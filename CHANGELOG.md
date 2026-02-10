# Changelog

All notable changes to this project will be documented in this file.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.1.0] - 2026-02-10

### Added
- Sub CPU build infrastructure (crt0.s, sub.ld, Makefile rules)
- Main CPU build infrastructure (crt0.s, main.ld, Makefile rules)
- Freestanding C headers (stdint.h, stddef.h, stdbool.h, string.h)
- Standalone VDP interface (vdp.h) -- no SGDK libmd dependency
- Strip-based framebuffer-to-tile conversion pipeline (framebuffer.c)
- Windows 3.1 16-color palette in MD 9-bit RGB
- Gate Array boot sequence for Sub CPU program loading
- Mega Mouse driver with absolute position tracking
- Input event system (Main -> Sub CPU via comm registers)
- Software blitter with 2bpp and 4bpp modes
- Window manager with drag, resize, z-order
- Applications: Notepad, Calculator, Virtual Keyboard, Paint
- Memory manager (handle-based, Mac OS style)
