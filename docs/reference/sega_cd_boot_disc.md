# Sega CD Boot Sector & Disc Building Reference

Primary source: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
(`MEGADEV 1.2.0`, MIT).

Files inspected:
- `lib/cd_boot.s`
- `cfg/ip.ld`
- `cfg/sp.ld`
- `megadev.make`
- `lib/security.c`
- `docs/boot.md`
- `docs/disc.md`

## Boot Sector Layout (boot.bin, 32KB)

The Sega CD boot sector occupies the first 16 sectors (32KB) of the ISO.
The BIOS reads this on startup to identify and boot the disc.

```
Offset  Size    Description
------  ----    -----------
$0000   16B     Disc Type: "SEGADISCSYSTEM  " (must match exactly)
$0010   12B     Volume Name, space-padded
$001C   4B      System ID / Type (0x100, 0x1)
$0020   12B     System Name, space-padded
$002C   4B      System Version / Type (0x100, 0)
$0030   4B      IP Offset field ($0800 in Megadev)
$0034   4B      IP Size field ($0800 in Megadev)
$0038   4B      IP Entry (0)
$003C   4B      IP Work RAM (0)
$0040   4B      SP Offset ($1000)
$0044   4B      SP Size (calculated from sp_end - sp_begin)
$0048   4B      SP Entry (0)
$004C   4B      SP Work RAM (0)
$0050   176B    Reserved system-header strings, space-filled
$0100   16B     Hardware type (`SEGA GENESIS` for US, `SEGA MEGA DRIVE` for JP/EU)
$0110   16B     Copyright
$0120   48B     Domestic title
$0150   48B     International title
$0180   16B     Software ID / checksum area
$0190   96B     I/O support and reserved header fields
$01F0   16B     Region code
$0200   <=3.5KB **IP Binary bytes** (Initial Program, loaded to $FF0000)
$1000   ~16KB   **SP Binary** (Sub CPU Program, loaded to $006000)
$8000   ---     End of boot sector (padded with .align)
```

Megadev's `$0030` IP offset field is intentionally **not** the same as the
physical offset where `ip.bin` is inserted. In `lib/cd_boot.s`, Megadev sets the
header field to `$0800` for the US/EU security-code compatibility quirk, then
physically includes `ip.bin` at `$0200`.

## IP (Initial Program) — Main CPU

- Loaded by BIOS to **$FF0000** (Work RAM)
- Max size: **3.5KB** ($E00, limited by sector 0-1 boundary)
- Purpose: Initialize Main CPU, load MMD modules, set up VDP
- Must include **regional security code** for the target region
- Linked with `ip.ld`: `ORIGIN = 0xFF0000, LENGTH = 0xE00`
- The security block must be the first code/data in the IP.
- Keep the IP tiny; use an extended IP/IPX-style resident module if the Main
  CPU kernel grows beyond the safe boot-sector envelope.

## SP (Sub CPU Program) — Sub CPU

- Loaded by BIOS to **$006000** (PRG-RAM, after BIOS system area)
- Max size in boot sector: **28KB** ($7000, from $1000 to $8000)
- Default size: **16KB** ($4000)
- Purpose: Initialize Sub CPU kernel, CD-ROM access, custom OS code
- **Must have SP header** with jump table at $006000:
  - `usercall0` ($005F28 → sp_init)
  - `usercall1` ($005F2E → sp_main)
  - `usercall2` ($005F34 → sp_int2)
  - `usercall3` ($005F3A → sp_user)
- Linked with `sp.ld`: `ORIGIN = 0x6000, LENGTH = 0x4000`

## Build Pipeline

```
1. ip.s      → assemble → ip.bin.elf → objcopy → ip.bin
2. sp_header.s + sp.s → assemble → sp.bin.elf → objcopy → sp.bin
3. boot.s    → .incbin "ip.bin" + .incbin "sp.bin" → boot.s.o → boot.bin
4. mkisofs -G boot.bin -iso-level 1 -sysid "MEGA_CD" -o game.iso disc/
```

Key `mkisofs` flags:
- **`-G boot.bin`** — Inject boot sector as ISO system area (first 16 sectors)
- `-iso-level 1` — ISO9660 Level 1 (8.3 filenames, required for BIOS compatibility)
- `-sysid "MEGA_CD"` — System identifier
- `-pad` — Pad to multiple of sector size

The resulting data track is a cooked ISO image. A simple CUE should describe it
as:

```
FILE "game.iso" BINARY
  TRACK 01 MODE1/2048
    INDEX 01 00:00:00
```

Raw `MODE1/2352` images may be useful for specific distribution/testing
workflows, but Megadev's maintained reference path is the cooked ISO system-area
injection model.

## Security Code

The IP binary must contain the correct regional security code:
- **US/EU security code** is larger than Japanese, requiring the header IP
  offset quirk described above
- The security code is assembled as part of the IP and validates against the
  BIOS region check
- Without valid security code, the BIOS will reject the disc

## SegaOS Status Notes

As of the June 2026 Megadev reconciliation pass:

- `tools/build_iso.py` writes a cooked `MODE1/2048` ISO/CUE using only Python's
  standard library.
- `src/main/security.c` is a direct MIT copy of Megadev `lib/security.c`; the
  Main CPU linker keeps `.security` first so BIOS execution starts at the
  regional security code before branching to SegaOS entry code.
- The builder records the Megadev-compatible IP header offset/size fields
  `$0800/$0800`, physically places IP bytes at `$0200`, and physically places SP
  bytes at `$1000`.
- The builder space-pads the system-header string fields and uses Megadev's
  region-specific hardware ID defaults; `tools/verify_disc.py` fails if these
  bytes drift.
- `src/sub/crt0.s` uses the Megadev SP module-header contract and relative
  jump table format for `sp_init`, `sp_main`, `sp_int2`, and `sp_user`.
- `src/sub/sub.ld` uses Megadev-style `SUBALIGN(2)` for SP sections. The
  current assembly probe SP places `sp_init` at `$602A`, `sp_main` at `$607E`,
  and reports `_TEXT_LENGTH = $03a2` (930 bytes) for the visible framebuffer
  probe.
- Earlier `BOOT_PROBE=1` experiments confirmed the BIOS-loaded SP payload bytes
  are visible in PRG-RAM bank 0 at the Main CPU alias `$020000 + $006000`.
- A permanent Megadev-derived dual-CPU control fixture built by SegaOS reports
  from Sub-side code in BlastEm, proving the ISO builder/security path can boot
  a Megadev-shaped SP.
- The current SegaOS assembly `BOOT_PROBE=1` path proves Sub `sp_init`, Sub
  `sp_main`, and Gate Array command/status exchange with `-Probe
  DualCpuStatus`.
- The Main CPU stack is kept below `$FFF700` so it does not collide with the
  Main BIOS work/system-use region.
- Oversized IP/SP payloads are rejected instead of truncated.
- `tools/verify_disc.py` fails on layout errors and selected JP/US/EU security
  prefix or license-marker mismatches; it is run by `make iso`.
- BlastEm 0.6.3-pre with a USA Sega CD BIOS and SGDK GDB reached `$00FF0000`
  and read the expected US security bytes from `$FF0000`.
- The strict `-Probe DualCpu` Word RAM check now passes: Main reads
  `0xa55a/0x5aa5` from `$200000` after Sub writes those words to `$0C0000` and
  clears RET in 1M mode. This completes the minimal dual-CPU probe rung.
- The `-Probe Framebuffer` check and visible framebuffer variant now pass: Main
  reads the expected Sub-written 4bpp pattern through Word RAM, reads the same
  tile rows back from VDP VRAM, and BlastEm internal screenshotting captures
  the expected full-screen pattern.
- A manual Megadev `hello_world` control image built through this Python ISO
  builder reaches the Megadev Main IP loop in BlastEm, so current runtime work
  is narrower than image building or regional security.
- Deeper emulator and hardware/ODE testing are still required before treating
  the runtime or disc format as release-safe.

## Tools Required
- **m68k-elf-gcc/as** — Cross-assembler (from SGDK or 32XDK)
- **m68k-elf-objcopy** — Binary extraction from ELF
- **Python 3** — current SegaOS stdlib ISO/CUE builder and verifier
- **mkisofs** (or **genisoimage**) — optional reference/future ISO9660 path with
  `-G` boot sector injection
- No other special Sega CD tools needed for the current local build

## For Emulator Testing
Most emulators (Gens, BlastEm, Kega Fusion) accept:
- Raw ISO files directly
- BIN/CUE pairs for mixed-mode with CD audio
- CHD files (MAME/RetroArch, created with `chdman createcd`)
