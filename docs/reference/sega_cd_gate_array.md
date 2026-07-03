# Sega CD Gate Array Register Reference

Primary source: `drojaazu/megadev@7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef`
(`MEGADEV 1.2.0`, MIT), especially `lib/sub/gate_arr.def.h`,
`lib/sub/gate_arr.h`, `lib/main/gate_arr.def.h`, and `lib/main/gate_arr.h`.

Base Address: $FF8000 (Sub CPU), $A12000 (Main CPU)

## GA Reg 00 — Reset / Bus Request
Sub CPU address: $FF8000

| F| E| D| C| B| A| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
|  |  |  |  |  |  |LEDG|LEDR|Ver3|Ver2|Ver1|Ver0|  |  |  |RES0|

- **RES0** — W: 0=Reset / 1=No effect; R: 0=Reset in progress / 1=Reset possible
- **LEDR** — Red LED control (0=Off, 1=On)
- **LEDG** — Green LED control (0=Off, 1=On)
- **Ver** — ROM Version (read-only)

## GA Reg 01 — Memory Mode / Write Protect
Sub CPU address: $FF8002

| F| E| D| C| B| A| 9| 8| 7| 6| 5| 4| 3| 2| 1| 0|
|WP7|WP6|WP5|WP4|WP3|WP2|WP1|WP0|  |  |  |PM1|PM0|MODE|DMNA|RET|

- **WP** — Write protect Sub CPU PRG RAM
- **PM** — Priority Mode (for ASIC graphics)
- **MODE** — Word RAM layout: 0=2M, 1=1M
- **DMNA** — Main CPU will not access Word RAM (bank swap trigger)
- **RET** — 2M mode: Give Word RAM to Main CPU; 1M mode: Change bank ownership

SegaOS probe evidence: after BIOS boot into 1M mode, MEM_MODE was `0x2a05`.
Sub could write/read `$0C0000`, and clearing RET changed MEM_MODE to `0x2a04`;
Main then read the same words at `$200000`. Setting RET and waiting was wrong
for this initial bank-0 return because RET was already set.

Follow-up `DESKTOP_REPEAT_PROBE=1` evidence: Main releases the displayed
single-bank frame by writing RET, but BlastEm reports the released/repeated
state as `0x2a06` (DMNA set, RET clear). The boot-safe Sub ownership predicate
therefore accepts either RET or DMNA in 1M mode. This proves a two-frame
single-bank render/upload loop in BlastEm, not a full alternating double-buffer
policy.

2026-07-03 live-frame fix: the stale screenshot root cause was not the VDP text
or font path. In 1M mode, Megadev's Main Gate Array reference records RET as
the bank attachment bit: RET clear exposes bank 0 to Main, RET set exposes bank
1. SegaOS' old Sub return helper always cleared RET, so repeated frames could
render into the other bank while Main kept uploading stale bank 0. The boot-safe
live probe now toggles RET when Sub returns its current work bank and has Main
select the bank whose frame sentinel matches the requested frame. The accepted
proof reaches pre-upload phase `0x8904`, sentinel `0x4444`, terminal phase
`0x89ff`, and a `Frame 4` screenshot at
`C:\tmp\segaos_screens_internal\segaos_live_current_20260703_193928.png`.

## GA Reg 02 — CDC Mode
Sub CPU address: $FF8004

CDC Device Destination (DD field):
| DD2 | DD1 | DD0 | Destination |
|-----|-----|-----|-------------|
|  0  |  1  |  0  | Main CPU    |
|  0  |  1  |  1  | Sub CPU     |
|  1  |  0  |  0  | PCM DMA     |
|  1  |  1  |  1  | 2M: Word RAM / 1M: Sub CPU controlled Word RAM |

## GA Reg 05 — CDC DMA Address
Sub CPU address: $FF800A

DMA destination address bits used:
- PCM DMA: bits up to A12
- 1M Word RAM: bits up to A16
- 2M Word RAM: bits up to A17
- PRG-RAM: all bits

## GA Reg 06 — Stopwatch
Sub CPU address: $FF800C
Timer tick = 30.72 microseconds.

## Comm Registers (Sub CPU perspective)
- **GA_COMFLAGS** ($FF800E) — 16-bit: upper byte=Main flags, lower byte=Sub flags
  - Sub CPU writes lower byte (CFS)
  - Sub CPU reads upper byte (CFM) — read only
- **GA_COMCMD0-7** ($FF8010-$FF801E) — Main→Sub command params (read-only for Sub)
- **GA_COMSTAT0-7** ($FF8020-$FF802E) — Sub→Main status returns (writable by Sub)

SegaOS protocol evidence, 2026-06-30: use CFM only as a Main-to-Sub pending
signal. The opcode lives in `GA_COMCMD0`; payload words live in
`GA_COMCMD1`-`GA_COMCMD4`. The current pending value is `0x02` because that path
is verified by both the C command loop and the assembly `BOOT_PROBE` path.

## ASIC / Stamp/Trace Registers
- GA_STAMPSIZE ($FF8058)
- GA_STAMPMAPBASE ($FF805A)
- GA_IMGBUFVSIZE ($FF805C)
- GA_IMGBUFSTART ($FF805E)
- GA_IMGBUFOFFSET ($FF8060)
- GA_IMGBUFHDOTSIZE ($FF8062)
- GA_IMGBUFVDOTSIZE ($FF8064)
- GA_TRACEVECTBASE ($FF8066)

## Comm Flag Semantics (Boot ROM)
The GA_COMFLAGS register doesn't have inherent semantic meaning. Boot ROM library assigns:
- Bit 0 SET = comm registers have been updated (fresh copy needed)
- Bit 1 SET = CPU is ready for copy; CLEAR = CPU is busy

## Sub CPU Memory Map

| Range | Size | Description |
|-------|------|-------------|
| $000000-$005FFF | 24KB | BIOS vectors/system program |
| $006000-$07FFFF | 488KB | PRG-RAM (user code + data) |
| $080000-$0BFFFF | 256KB | Word RAM (2M mode, entire buffer) |
| $0C0000-$0DFFFF | 128KB | Word RAM Bank 0 (1M mode) |
| $0E0000-$0FFFFF | 128KB | Word RAM Bank 1 (1M mode, pixel mapped) |
| $FE0001-$FE3FFF | 8KB | Backup RAM (odd addresses only) |
| $FF0000-$FF3FFF | 16KB | PCM wave RAM |
| $FF8000-$FF807F | 128B | Gate Array registers |

## Main CPU Memory Map (Sega CD additions)

| Range | Size | Description |
|-------|------|-------------|
| $200000-$23FFFF | 256KB | Word RAM (2M mode) |
| $200000-$21FFFF | 128KB | Word RAM Bank 0 (1M mode) |
| $220000-$23FFFF | 128KB | Word RAM Bank 1 (1M mode) |
| $400000-$41FFFF | 128KB | PRG-RAM window (banked, bank select in GA) |
| $A12000-$A1202F | 48B | Gate Array registers |
