#!/usr/bin/env python3
"""
build_iso.py - Sega CD Boot Disc Image Builder

Constructs a BIN/CUE disc image with proper Mode 1/2352 raw sectors.
No external tools required (no mkisofs/chdman).

Boot sector layout (32KB = 16 sectors of 2048 bytes):
  $0000-$00FF  Disc header (type, volume, system info)
  $0100-$01FF  Game header (hardware, copyright, title, region)
  $0800-$0FFF  IP binary (Initial Program, Main CPU, max 2KB US/EU)
  $1000-$7FFF  SP binary (Sub CPU Program, max 28KB)

Output: .bin (raw 2352-byte sectors) + .cue (track sheet)

Usage: python build_iso.py <ip.bin> <sp.bin> <output_base>
  Produces: <output_base>.bin and <output_base>.cue
"""

import struct
import sys
import os

# Sector sizes
COOKED_SECTOR = 2048   # User data per sector (ISO9660 Mode 1)
RAW_SECTOR = 2352      # Full raw sector (sync + header + data + EDC + ECC)

# Boot sector
BOOT_SECTORS = 16
BOOT_SIZE = BOOT_SECTORS * COOKED_SECTOR  # 0x8000

# IP/SP offsets within boot sector
IP_OFFSET = 0x0800
IP_MAX_SIZE = 0x0800  # 2KB for US/EU
SP_OFFSET = 0x1000
SP_MAX_SIZE = 0x7000  # 28KB

# Mode 1 sync pattern (12 bytes at start of each raw sector)
SYNC_PATTERN = bytes([0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00])

# Minimum disc size: 300 sectors (~2 seconds, avoids emulator edge cases)
MIN_SECTORS = 300


def to_bcd(val: int) -> int:
    """Convert integer to BCD (e.g., 42 -> 0x42)."""
    return ((val // 10) << 4) | (val % 10)


def lba_to_msf(lba: int) -> tuple:
    """Convert LBA sector number to MSF (minute, second, frame).
    Sega CD data starts at 00:02:00 (LBA 150 = 2-second pregap)."""
    lba += 150  # Add 2-second pregap offset
    m = lba // (60 * 75)
    s = (lba // 75) % 60
    f = lba % 75
    return m, s, f


def compute_edc(data: bytes) -> int:
    """Compute EDC (Error Detection Code) CRC-32 for Mode 1 sectors.
    Polynomial: x^32 + x^31 + x^16 + 1 (reversed: 0xD8018001)."""
    # Use lookup table for speed
    edc = 0
    for byte in data:
        edc = ((edc >> 8) ^ _EDC_TABLE[(edc ^ byte) & 0xFF]) & 0xFFFFFFFF
    return edc


# Pre-compute EDC lookup table
def _build_edc_table():
    table = []
    for i in range(256):
        edc = i
        for _ in range(8):
            if edc & 1:
                edc = (edc >> 1) ^ 0xD8018001
            else:
                edc >>= 1
        table.append(edc & 0xFFFFFFFF)
    return table


_EDC_TABLE = _build_edc_table()


def make_raw_sector(lba: int, user_data: bytes) -> bytes:
    """Build a complete Mode 1 raw sector (2352 bytes).

    Layout:
      [0..11]    Sync pattern (12 bytes)
      [12..14]   MSF address in BCD (3 bytes)
      [15]       Mode byte (0x01 for Mode 1)
      [16..2063] User data (2048 bytes)
      [2064..2067] EDC (4 bytes, CRC-32 of bytes 0..2063)
      [2068..2075] Zero (8 bytes, reserved)
      [2076..2351] ECC P and Q parity (276 bytes)
    """
    assert len(user_data) == COOKED_SECTOR

    sector = bytearray(RAW_SECTOR)

    # Sync
    sector[0:12] = SYNC_PATTERN

    # MSF address
    m, s, f = lba_to_msf(lba)
    sector[12] = to_bcd(m)
    sector[13] = to_bcd(s)
    sector[14] = to_bcd(f)

    # Mode 1
    sector[15] = 0x01

    # User data
    sector[16:16 + COOKED_SECTOR] = user_data

    # EDC over bytes 0..2063
    edc = compute_edc(bytes(sector[0:2064]))
    struct.pack_into("<I", sector, 2064, edc)

    # Bytes 2068..2075: reserved zeros (already 0)

    # ECC P and Q parity: set to zero for now.
    # Most emulators don't verify ECC. Real hardware uses it for
    # error correction but homebrews commonly skip it.

    return bytes(sector)


def build_disc_header(ip_size: int, sp_size: int) -> bytearray:
    """Build the disc header (first $200 bytes)."""
    header = bytearray(0x200)

    # $0000: Disc type identifier (must match exactly, 16 bytes)
    header[0x00:0x10] = b"SEGADISCSYSTEM  "

    # $0010: Volume name
    vol = b"SEGAOS\x00"
    header[0x10:0x10 + len(vol)] = vol

    # $001C: System ID / Type
    struct.pack_into(">HH", header, 0x1C, 0x100, 0x01)

    # $0020: System name (11 bytes + null)
    header[0x20:0x2C] = b"SEGASYSTEM  "

    # $002C: System version / type
    struct.pack_into(">HH", header, 0x2C, 0, 0)

    # $0030: IP offset, size, entry, work RAM
    struct.pack_into(">IIII", header, 0x30,
                     IP_OFFSET, ip_size, 0, 0)

    # $0040: SP offset, size, entry, work RAM
    struct.pack_into(">IIII", header, 0x40,
                     SP_OFFSET, sp_size, 0, 0)

    # --- Game header at $0100 ---

    # Hardware type (16 bytes)
    header[0x100:0x110] = b"SEGA MEGA DRIVE "

    # Copyright / date (16 bytes)
    header[0x110:0x120] = b"(C)PROJ12 2026  "

    # Domestic name (48 bytes, space-padded)
    dom = b"SegaOS - Windowed Operating System               "
    header[0x120:0x150] = dom[:48]

    # Overseas name (48 bytes, space-padded)
    header[0x150:0x180] = dom[:48]

    # Game type + serial (14 bytes)
    header[0x180:0x18E] = b"GM 00000000-00"

    # Checksum placeholder
    struct.pack_into(">H", header, 0x18E, 0x0000)

    # I/O support (16 bytes: J=joypad, M=mouse)
    header[0x190:0x1A0] = b"JM              "

    # ROM start/end
    struct.pack_into(">II", header, 0x1A0, 0x00000000, 0x003FFFFF)

    # RAM start/end
    struct.pack_into(">II", header, 0x1A8, 0x00FF0000, 0x00FFFFFF)

    # SRAM (12 bytes, spaces = none)
    header[0x1B0:0x1BC] = b"            "

    # Modem (12 bytes)
    header[0x1BC:0x1C8] = b"            "

    # Memo (40 bytes)
    header[0x1C8:0x1F0] = b"                                        "

    # Region code (16 bytes)
    header[0x1F0:0x200] = b"JUE             "

    return header


def build_boot_sector(ip_data: bytes, sp_data: bytes) -> bytearray:
    """Build the 32KB boot sector (16 cooked sectors)."""
    ip_size = len(ip_data)
    sp_size = len(sp_data)

    if ip_size > IP_MAX_SIZE:
        print(f"WARNING: IP ({ip_size} bytes) > {IP_MAX_SIZE} limit!")
    if sp_size > SP_MAX_SIZE:
        print(f"WARNING: SP ({sp_size} bytes) > {SP_MAX_SIZE} limit!")

    boot = bytearray(BOOT_SIZE)
    boot[0x00:0x200] = build_disc_header(ip_size, sp_size)
    boot[IP_OFFSET:IP_OFFSET + ip_size] = ip_data
    boot[SP_OFFSET:SP_OFFSET + sp_size] = sp_data
    return boot


def build_pvd(total_sectors: int) -> bytearray:
    """Build ISO9660 Primary Volume Descriptor."""
    pvd = bytearray(COOKED_SECTOR)
    pvd[0] = 0x01         # Type: PVD
    pvd[1:6] = b"CD001"   # Standard ID
    pvd[6] = 0x01         # Version

    # System Identifier (32 bytes)
    pvd[8:40] = b"MEGA_CD                         "

    # Volume Identifier (32 bytes)
    pvd[40:72] = b"SEGAOS                          "

    # Volume Space Size (both-endian)
    struct.pack_into("<I", pvd, 80, total_sectors)
    struct.pack_into(">I", pvd, 84, total_sectors)

    # Volume Set Size
    struct.pack_into("<H", pvd, 120, 1)
    struct.pack_into(">H", pvd, 122, 1)

    # Volume Sequence Number
    struct.pack_into("<H", pvd, 124, 1)
    struct.pack_into(">H", pvd, 126, 1)

    # Logical Block Size
    struct.pack_into("<H", pvd, 128, COOKED_SECTOR)
    struct.pack_into(">H", pvd, 130, COOKED_SECTOR)

    # Root directory record at offset 156
    root = bytearray(34)
    root[0] = 34      # Record length
    # Extent location (sector 18 = after PVD + terminator)
    struct.pack_into("<I", root, 2, 18)
    struct.pack_into(">I", root, 6, 18)
    # Data length
    struct.pack_into("<I", root, 10, COOKED_SECTOR)
    struct.pack_into(">I", root, 14, COOKED_SECTOR)
    # Date: 2026-02-10
    root[18:25] = bytes([126, 2, 10, 15, 0, 0, 0])
    root[25] = 0x02   # Directory flag
    struct.pack_into("<H", root, 28, 1)
    struct.pack_into(">H", root, 30, 1)
    root[32] = 1      # ID length
    root[33] = 0x00   # Root ID
    pvd[156:190] = root

    # File Structure Version
    pvd[881] = 0x01

    return pvd


def build_vdst() -> bytearray:
    """Build Volume Descriptor Set Terminator."""
    vdst = bytearray(COOKED_SECTOR)
    vdst[0] = 0xFF
    vdst[1:6] = b"CD001"
    vdst[6] = 0x01
    return vdst


def build_root_dir() -> bytearray:
    """Build empty root directory (self + parent entries)."""
    root = bytearray(COOKED_SECTOR)

    # "." entry (self)
    dot = bytearray(34)
    dot[0] = 34
    struct.pack_into("<I", dot, 2, 18)
    struct.pack_into(">I", dot, 6, 18)
    struct.pack_into("<I", dot, 10, COOKED_SECTOR)
    struct.pack_into(">I", dot, 14, COOKED_SECTOR)
    dot[18:25] = bytes([126, 2, 10, 15, 0, 0, 0])
    dot[25] = 0x02
    struct.pack_into("<H", dot, 28, 1)
    struct.pack_into(">H", dot, 30, 1)
    dot[32] = 1
    dot[33] = 0x00
    root[0:34] = dot

    # ".." entry (parent = self for root)
    dotdot = bytearray(dot)
    dotdot[33] = 0x01
    root[34:68] = dotdot

    return root


def write_cue(cue_path: str, bin_filename: str):
    """Write a CUE sheet for a single Mode 1 data track."""
    cue = (
        f'FILE "{bin_filename}" BINARY\n'
        f'  TRACK 01 MODE1/2352\n'
        f'    INDEX 01 00:00:00\n'
    )
    with open(cue_path, "w") as f:
        f.write(cue)


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <ip.bin> <sp.bin> <output_base>")
        print("  Produces: <output_base>.bin and <output_base>.cue")
        sys.exit(1)

    ip_path = sys.argv[1]
    sp_path = sys.argv[2]
    out_base = sys.argv[3]

    # Strip extension if user passed one
    if out_base.endswith((".iso", ".bin", ".cue")):
        out_base = out_base.rsplit(".", 1)[0]

    bin_path = out_base + ".bin"
    cue_path = out_base + ".cue"

    # Read binaries
    for path, name in [(ip_path, "IP"), (sp_path, "SP")]:
        if not os.path.exists(path):
            print(f"ERROR: {name} binary not found: {path}")
            sys.exit(1)

    with open(ip_path, "rb") as f:
        ip_data = f.read()
    with open(sp_path, "rb") as f:
        sp_data = f.read()

    print(f"IP (Main CPU): {len(ip_data)} bytes ({len(ip_data):#06x})")
    print(f"SP (Sub CPU):  {len(sp_data)} bytes ({len(sp_data):#06x})")

    if len(ip_data) > IP_MAX_SIZE:
        print(f"WARNING: IP exceeds {IP_MAX_SIZE} byte limit!")
    if len(sp_data) > SP_MAX_SIZE:
        print(f"WARNING: SP exceeds {SP_MAX_SIZE} byte limit!")

    # Build cooked sectors
    boot_data = build_boot_sector(ip_data, sp_data)
    pvd_data = build_pvd(MIN_SECTORS)
    vdst_data = build_vdst()
    root_data = build_root_dir()

    # Assemble all cooked sectors
    cooked_sectors = []

    # Sectors 0-15: boot sector (16 sectors)
    for i in range(BOOT_SECTORS):
        offset = i * COOKED_SECTOR
        cooked_sectors.append(bytes(boot_data[offset:offset + COOKED_SECTOR]))

    # Sector 16: PVD
    cooked_sectors.append(bytes(pvd_data))

    # Sector 17: VDST
    cooked_sectors.append(bytes(vdst_data))

    # Sector 18: Root directory
    cooked_sectors.append(bytes(root_data))

    # Pad to minimum sector count with empty sectors
    while len(cooked_sectors) < MIN_SECTORS:
        cooked_sectors.append(bytes(COOKED_SECTOR))

    # Convert to raw Mode 1 sectors and write BIN
    with open(bin_path, "wb") as f:
        for lba, cooked in enumerate(cooked_sectors):
            raw = make_raw_sector(lba, cooked)
            f.write(raw)

    # Write CUE sheet
    bin_filename = os.path.basename(bin_path)
    write_cue(cue_path, bin_filename)

    total_raw = len(cooked_sectors) * RAW_SECTOR
    print(f"BIN written: {bin_path} ({total_raw} bytes, {len(cooked_sectors)} sectors)")
    print(f"CUE written: {cue_path}")
    print(f"Load {cue_path} in your emulator.")


if __name__ == "__main__":
    main()
