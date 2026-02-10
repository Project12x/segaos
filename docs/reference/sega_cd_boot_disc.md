# Sega CD Boot Sector & Disc Building Reference
Source: Megadev boot.s, ip.ld, sp.ld, makefile_global

## Boot Sector Layout (boot.bin, 32KB)

The Sega CD boot sector occupies the first 16 sectors (32KB) of the ISO.
The BIOS reads this on startup to identify and boot the disc.

```
Offset  Size    Description
------  ----    -----------
$0000   16B     Disc Type: "SEGADISCSYSTEM  " (must match exactly)
$0010   11B     Volume Name + null terminator
$001C   4B      System ID / Type (0x100, 0x1)
$0020   12B     System Name: "SEGASYSTEM "
$002C   4B      System Version / Type (0, 0)
$0030   4B      IP Offset ($0800 for US/EU security compatibility)
$0034   4B      IP Size ($0800)
$0038   4B      IP Entry (0)
$003C   4B      IP Work RAM (0)
$0040   4B      SP Offset ($1000)
$0044   4B      SP Size (calculated from sp_end - sp_begin)
$0048   4B      SP Entry (0)
$004C   4B      SP Work RAM (0)
$0100   48B     Game Header (hardware type, copyright, names, disc ID)
$0148   80B     I/O support ("J" for joypad)
$0198   16B     Region code
$0200   ~3.5KB  **IP Binary** (Initial Program, loaded to $FF0000)
$1000   ~16KB   **SP Binary** (Sub CPU Program, loaded to $006000)
$8000   ---     End of boot sector (padded with .align)
```

## IP (Initial Program) — Main CPU

- Loaded by BIOS to **$FF0000** (Work RAM)
- Max size: **3.5KB** ($E00, limited by sector 0-1 boundary)
- Purpose: Initialize Main CPU, load MMD modules, set up VDP
- Must include **regional security code** for the target region
- Linked with `ip.ld`: `ORIGIN = 0xFF0000, LENGTH = 0xE00`

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

## Security Code

The IP binary must contain the correct regional security code:
- **US/EU security code** is larger than Japanese, requiring IP to start at $0800
  instead of fitting entirely in sector 0
- The security code is assembled as part of the IP and validates against the
  BIOS region check
- Without valid security code, the BIOS will reject the disc

## Tools Required
- **m68k-elf-gcc/as** — Cross-assembler (from SGDK or 32XDK)
- **m68k-elf-objcopy** — Binary extraction from ELF
- **mkisofs** (or **genisoimage**) — ISO9660 image creation with `-G` boot sector injection
- No other special Sega CD tools needed

## For Emulator Testing
Most emulators (Gens, BlastEm, Kega Fusion) accept:
- Raw ISO files directly
- BIN/CUE pairs for mixed-mode with CD audio
- CHD files (MAME/RetroArch, created with `chdman createcd`)
