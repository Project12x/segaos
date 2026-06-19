# Megadev Module System (MMD Format)

Primary source: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
(`MEGADEV 1.2.0`, MIT), especially `docs/modules.md`, `docs/cdrom.md`,
`docs/disc.md`, `lib/main/mmd.macros.s`, and `lib/sub/cdrom.s`.

## Overview
Modules are compiled binaries (small "ROMs") on disc. Each represents one piece of the game:
title screen, options, gameplay stage, etc. They are loaded, executed, and unloaded.

## MMD Header Format (0x100 bytes)
```
$00.w  Flags (bit 6: return Word RAM to Sub before running)
$02.l  Runtime location (MMD_DEST: 0 = run in place)
$06.w  Module size (in longs, auto-calculated)
$08.l  Pointer to main routine
$0C.l  Pointer to HINT handler (optional)
$10.l  Pointer to VINT handler (optional)
$14-$100  Padding (available for metadata/debug text)
```

## Required Symbols
- `main` — Global function (entry point): `void main()`
- `MMD_DEST` — Global symbol: runtime destination address (0 = run in place)

## Optional Symbols
- `hint` / `vint` — Global functions for interrupt handlers
- `MMD_FLAGS` — Global symbol for flags word

## Memory Layout Symbols
```
MODULE_ROM_ORIGIN  — Where module code begins
MODULE_ROM_LENGTH  — Size of ROM (code+data) section
MODULE_RAM_ORIGIN  — Where runtime variables live
MODULE_RAM_LENGTH  — Size of RAM section
```

## Common Patterns

**Run from Word RAM (2M mode):**
- `MMD_DEST = 0` (run in place)
- `MODULE_ROM_ORIGIN = 0x200100` (Main CPU, after 0x100 header)
- `MODULE_ROM_ORIGIN = 0x080100` (Sub CPU, after 0x100 header)

**Copy to Work RAM then run:**
- `MMD_DEST = 0xFF0000` (target address)
- `MODULE_ROM_ORIGIN = 0xFF0000` (same as dest)

## Object Sections
- `.text`, `.rodata` → ROM space
- `.data`, `.bss` → RAM space
- `.init` → Beginning of ROM (entry point guaranteed)

## Word RAM Pitfalls
**CRITICAL**: Never put interrupt handlers in Word RAM! If Word RAM ownership changes
while a VINT handler points there, the system will crash. Keep interrupt handlers in
Work RAM or PRG-RAM (memory that won't change ownership).

## CD-ROM Access Wrapper
Sub CPU only. INT2-driven access loop:
1. Include `sub/cdrom.s` or `sub/cdrom.h` in SP/Sub-side code
2. Call `INIT_ACC_LOOP` in sp_init (usercall0)
3. Call `PROCESS_ACC_LOOP` in sp_int2 (usercall2)
4. Load directory cache first: set `CDROM_LOAD_FILE_LIST`, wait for completion

**File Loading Steps:**
1. Set `filename` pointer to the filename string
2. Set destination (GA DMA register or `file_dest_ptr`)
3. Set access operation
4. Wait for completion

CDC device destination is set automatically based on the chosen access operation.

Supported load operations in Megadev 1.2.0:

- `CDROM_LOAD_CDC`
- `CDROM_LOAD_CDC_DMA`
- `CDROM_LOAD_PRG_DMA`
- `CDROM_LOAD_PCM_DMA`

Megadev does not currently support the Main CPU read option; use the Sub CPU as
the disc I/O owner and hand data to the Main CPU through Word RAM or PRG-RAM
windows.

## Disc Mastering
- ISO9660 filesystem for data track
- Mixed-mode for CD audio (CD-DA)
- Data track is always Track 01 (MODE1/2048)
- Audio tracks follow with 2-second pregaps
- Files in `disc/` directory are included in final ISO
- CUE sheet specifies track layout

Megadev's maintained build rule is equivalent to:

```
mkisofs -quiet -iso-level 1 -G boot.bin -pad -V PROJECT_ID \
  -sysid "MEGA_CD" -o game.iso disc/
```

For SegaOS, this reinforces the cooked ISO (`MODE1/2048`) path as canonical.
