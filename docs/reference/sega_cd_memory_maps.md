# Sub CPU Memory Map & System Vectors
Source: Megadev lib/sub/memmap_def.h

## PRG-RAM Layout

| Address | Size | Description |
|---------|------|-------------|
| $000000 | 512KB total | PRG-RAM base |
| $000000-$01FFFF | 128KB | PRG-RAM Bank 0 (Main CPU window) |
| $020000-$03FFFF | 128KB | PRG-RAM Bank 1 |
| $040000-$05FFFF | 128KB | PRG-RAM Bank 2 |
| $060000-$07FFFF | 128KB | PRG-RAM Bank 3 |
| **$006000** | — | **User code starts here** (SP entry) |

## Word RAM

| Symbol | Address | Mode | Description |
|--------|---------|------|-------------|
| `_WRDRAM_2M` | **$080000** | 2M | Full 256KB Word RAM |
| `_WRDRAM_1M` | **$0C0000** | 1M | Word RAM Bank 0 (128KB) |
| — | $0E0000 | 1M | Word RAM Bank 1 (128KB, pixel-mapped) |

## BIOS System Variables

| Symbol | Address | Description |
|--------|---------|-------------|
| `_BOOTSTAT` | $005EA0 | Boot status |
| `_INT2FLAG` | $005EA4 | INT2 flag |
| `_USERMODE` | $005EA6 | User mode |
| `_CDSTAT` | $005E80 | CD status |

## BIOS System Jump Table

| Symbol | Address | Description |
|--------|---------|-------------|
| `_SETJMPTBL` | $005F0A | Set jump table |
| `_WAITVSYNC` | $005F10 | Wait for VSync |
| `_BURAM` | $005F16 | Backup RAM call |
| `_CDBOOT` | $005F1C | CD Boot call |
| `_CDBIOS` | $005F22 | BIOS call |
| **`_USERCALL0`** | **$005F28** | **SP Init (sp_init)** |
| **`_USERCALL1`** | **$005F2E** | **SP Main (sp_main)** |
| **`_USERCALL2`** | **$005F34** | **SP INT2 (sp_int2)** |
| **`_USERCALL3`** | **$005F3A** | **SP User Call (sp_user)** |

## Exception Vectors

| Symbol | Address | Description |
|--------|---------|-------------|
| `_ADRERR` | $005F40 | Address error |
| `_CODERR` | $005F46 | Illegal instruction |
| `_DIVERR` | $005F4C | Division by zero |
| `_CHKERR` | $005F52 | CHK instruction |
| `_TRPERR` | $005F58 | TRAPV instruction |
| `_SPVERR` | $005F5E | Privilege violation |
| `_TRACE` | $005F64 | Trace |
| `_NOCOD0` | $005F6A | Line 1010 emulator |
| `_NOCOD1` | $005F70 | Line 1111 emulator |

## Interrupt Vectors

| Symbol | Address | Level | Description |
|--------|---------|-------|-------------|
| `_SLEVEL1` | $005F76 | 1 | **Graphics operation complete (ASIC)** |
| `_SLEVEL2` | $005F7C | 2 | **INT2 from Main CPU** |
| `_SLEVEL3` | $005F82 | 3 | GA Timer interrupt |
| `_SLEVEL4` | $005F88 | 4 | CDD complete |
| `_SLEVEL5` | $005F8E | 5 | CDC complete |
| `_SLEVEL6` | $005F94 | 6 | Subcode buffer full |
| `_SLEVEL7` | $005F9A | 7 | NMI |

## Trap Vectors
`_TRAP00` through `_TRAP15` at $005FA0-$005FFA (6 bytes each)

---

# Main CPU Memory Map & System Vectors
Source: Megadev lib/main/memmap_def.h

## Memory Map

| Symbol | Address | Description |
|--------|---------|-------------|
| `_WRKRAM` | $FF0000 | Work RAM (64KB) |
| `_WRDRAM` | $200000 | Word RAM in 2M mode |
| `_WRDRAM_1M_0` | $200000 | Word RAM Bank 0 (1M mode, 128KB) |
| `_WRDRAM_1M_1` | $220000 | Word RAM Bank 1 (1M mode, VDP tiles) |
| `_PRGRAM` | $020000 | PRG-RAM 1M mapping base |
| `_IP_ENTRY` | $FF0000 | Initial Program entry point |

## Gate Array (Main CPU side)

| Symbol | Address | Description |
|--------|---------|-------------|
| `_GA_RESET` | $A12000 | Sub CPU Reset/Bus Request |
| `_GA_MEMMODE` | $A12002 | Memory Mode / Write Protect |
| `_GA_CDCMODE` | $A12004 | CDC Mode |
| `_GA_HINTVECT` | $A12006 | H-INT Vector |
| `_GA_CDCHOSTDATA` | $A12008 | CDC Host Data |
| `_GA_STOPWATCH` | $A1200C | Stopwatch |
| `_GA_COMFLAGS` | $A1200E | Communication Flags |
| `_GA_COMCMD0-7` | $A12010-$A1201E | Main→Sub Commands |
| `_GA_COMSTAT0-7` | $A12020-$A1202E | Sub→Main Status |

## GA_RESET Register Bits (Main CPU)

| Bit | Name | Description |
|-----|------|-------------|
| `GA_RESET_IFL2_BIT` (byte 0, bit 0) | IFL2 | Send INT2 trigger to Sub CPU |
| `GA_RESET_SRES_BIT` (byte 1, bit 0) | SRES | Sub CPU reset (0=reset, 1=run) |
| `GA_RESET_SBRQ_BIT` (byte 1, bit 1) | SBRQ | Sub CPU bus request (1=halt) |

## GA_MEMMODE Register Bits

| Bit | Name | Description |
|-----|------|-------------|
| 0 | RET | Word RAM return |
| 1 | DMNA | Main will not access Word RAM |
| 2 | MODE | 0=2M, 1=1M |
| 6 | BK0 | Bank select bit 0 (PRG-RAM bank) |
| 7 | BK1 | Bank select bit 1 (PRG-RAM bank) |
| 8-15 | WP0-WP7 | Write protect bits |

## System Jump Table (Main CPU)

| Symbol | Address | Description |
|--------|---------|-------------|
| `_RESET` | $FFFD00 | Reset vector |
| `_MLEVEL6` | $FFFD06 | **VBlank interrupt** |
| `_MLEVEL4` | $FFFD0C | HBlank interrupt |
| `_MLEVEL2` | $FFFD12 | External port interrupt |
| `_VINT_EX` | $FFFDA8 | VBlank extended handler |
| `_MBURAM` | $FFFDAE | Backup RAM call |

## Important Notes

1. **SRES and SBRQ are in different bytes** of the 16-bit GA_RESET register:
   - IFL2 is in the high byte ($A12000)
   - SRES and SBRQ are in the low byte ($A12001)
2. **PRG-RAM banking**: BK0/BK1 in GA_MEMMODE select which 1M bank the Main CPU sees
   at $020000 (via `_PRGRAM`)
3. **Boot ROM library** uses the $FFFD00+ area for its system jump table
