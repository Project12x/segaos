# Sega CD Reference Documentation Index

## Megadev: Sega Mega CD Framework (drojaazu)
Primary source: https://github.com/drojaazu/megadev

Current reference point: **MEGADEV 1.2.0**, released 2026-05-10.

Pinned source for this repo's notes:
`drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`

License: MIT. Current SegaOS reuse modes: direct-copy for Megadev
`lib/security.c` with attribution in `src/main/security.c`, close-port or
pattern-only for the SP header/linker/control-fixture shape, and clean-room for
SegaOS-specific probe/control logic unless a file says otherwise.

### Architecture & Design
- [sega_cd_homebrew_2026.md](sega_cd_homebrew_2026.md) — June 2026 toolchain/homebrew update and SegaOS implications
- [megadev_ip_sp.md](megadev_ip_sp.md) — SP header format, BIOS jump table requirements
- [megadev_program_design.md](megadev_program_design.md) — Memory architecture overview (Work RAM, PRG-RAM, Word RAM)
- [megadev_dev_in_c.md](megadev_dev_in_c.md) — C development caveats (no global init, no stdlib, stack warnings)

### Hardware Reference
- [sega_cd_gate_array.md](sega_cd_gate_array.md) — GA register bit fields and comm flag semantics
- [sega_cd_memory_maps.md](sega_cd_memory_maps.md) — Definitive Sub/Main CPU memory maps with BIOS vectors
- [sega_cd_bootrom_library.md](sega_cd_bootrom_library.md) — Boot ROM DMA/VDP functions, default register values

### Module & Disc System
- [megadev_modules_and_cdrom.md](megadev_modules_and_cdrom.md) — MMD format, CD-ROM access API, disc mastering
- [sega_cd_boot_disc.md](sega_cd_boot_disc.md) — Sega CD boot-sector and cooked ISO build reference

## 32XDK: Chilly Willy's MD/CD/32X Devkit (viciious)
Source: https://github.com/viciious/32XDK/releases

Latest release: 20220418 devkit
- gcc 12.1.0, binutils 2.38, newlib 4.2.0, zasm 4.4
- Cross-compilers for M68K and SH-2
- Includes complete Sega CD toolchain

**NOTE**: This devkit includes `newlib` which provides `stdint.h` and other
freestanding C headers. May be useful if the SGDK toolchain lacks these.

## Current Tooling Position

- SGDK remains useful as the local m68k toolchain provider for SegaOS.
- Megadev is now the primary permissive Sega CD framework reference for boot
  layout, IP/SP structure, CD-ROM access, modules, and ISO generation.
- A Megadev-derived dual-CPU control fixture is kept under
  `tools/controls/megadev_dualcpu` as the current boot/startup baseline.
- The canonical data track format for the current SegaOS boot-disc work is a
  cooked ISO9660 track (`MODE1/2048`) with a 32KB Sega CD system area,
  equivalent to `mkisofs -G boot.bin`. Raw `MODE1/2352` BIN/CUE output can be
  a distribution format later, but it should not drive the boot-sector layout.
- GPL/AGPL projects such as `MCD_BOOT` and emulator cores may inform behavior
  and testing only. Do not copy source from them into SegaOS.

## Key Address Quick Reference

### Sub CPU
| What | Address | Notes |
|------|---------|-------|
| PRG-RAM user start | `$006000` | SP header goes here |
| Word RAM (2M) | `$080000` | 256KB, one CPU at a time |
| Word RAM (1M Bank 0) | `$0C0000` | 128KB, our framebuffer |
| Word RAM (1M Bank 1) | `$0E0000` | 128KB, pixel-mapped |
| Gate Array | `$FF8000` | 128 bytes of registers |
| PCM wave RAM | `$FF0000` | 8KB |

### Main CPU
| What | Address | Notes |
|------|---------|-------|
| Word RAM (2M) | `$200000` | 256KB |
| Word RAM (1M Bank 0) | `$200000` | 128KB |
| Word RAM (1M Bank 1) | `$220000` | 128KB |
| PRG-RAM window | `$020000` | 128KB banked window |
| Work RAM | `$FF0000` | 64KB |
| Gate Array | `$A12000` | 48 bytes of registers |
| System vectors | `$FFFD00` | Boot ROM jump table |
