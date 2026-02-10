#!/usr/bin/env python3
"""
build_iso.py - Sega CD Boot Disc Image Builder

Constructs a minimal ISO9660 image with the Sega CD boot sector.
No external tools required (no mkisofs).

Boot sector layout (32KB = 16 sectors of 2048 bytes):
  $0000-$00FF  Disc header (type, volume, system info, game header)
  $0100-$01FF  Game header (hardware, copyright, title, region)
  $0200-$07FF  IP binary (Initial Program, Main CPU, max 1.5KB)
               OR $0800-$0FFF for US/EU (security code at $0200)
  $1000-$7FFF  SP binary (Sub CPU Program, max 28KB)

The IP is loaded by BIOS to $FF0000 (Work RAM).
The SP is loaded by BIOS to $006000 (PRG-RAM).

Usage: python build_iso.py <ip.bin> <sp.bin> <output.iso>
"""

import struct
import sys
import os

# ISO9660 sector size
SECTOR_SIZE = 2048

# Boot sector is 16 sectors = 32KB
BOOT_SECTORS = 16
BOOT_SIZE = BOOT_SECTORS * SECTOR_SIZE  # 0x8000

# IP location (US/EU compatible: starts at $0800 to leave room for security)
IP_OFFSET = 0x0800
IP_MAX_SIZE = 0x0800  # 2KB for US/EU

# SP location
SP_OFFSET = 0x1000
SP_MAX_SIZE = 0x7000  # 28KB


def pad_to(data: bytearray, size: int, fill: int = 0x00) -> bytearray:
    """Pad data to the specified size."""
    if len(data) < size:
        data.extend(bytes([fill] * (size - len(data))))
    return data


def build_disc_header(ip_size: int, sp_size: int) -> bytearray:
    """Build the disc header (first $200 bytes)."""
    header = bytearray(0x200)

    # $0000: Disc type identifier (must match exactly)
    disc_type = b"SEGADISCSYSTEM  "
    header[0x00:0x10] = disc_type

    # $0010: Volume name
    volume = b"SEGAOS\x00"
    header[0x10:0x10 + len(volume)] = volume

    # $001C: System ID / Type
    struct.pack_into(">HH", header, 0x1C, 0x100, 0x01)

    # $0020: System name
    sys_name = b"SEGASYSTEM  "
    header[0x20:0x20 + len(sys_name)] = sys_name

    # $002C: System version / type
    struct.pack_into(">HH", header, 0x2C, 0, 0)

    # $0030: IP offset, size, entry, work RAM
    struct.pack_into(">IIII", header, 0x30,
                     IP_OFFSET,   # IP start offset in boot sector
                     ip_size,     # IP size in bytes
                     0,           # IP entry point (0 = start of IP)
                     0)           # IP work RAM size

    # $0040: SP offset, size, entry, work RAM
    struct.pack_into(">IIII", header, 0x40,
                     SP_OFFSET,   # SP start offset in boot sector
                     sp_size,     # SP size in bytes
                     0,           # SP entry point (0 = start of SP)
                     0)           # SP work RAM size

    # $0100: Game header
    # Hardware type (Genesis + MegaCD)
    hw_type = b"SEGA MEGA DRIVE "
    header[0x100:0x100 + len(hw_type)] = hw_type

    # Copyright / date
    copyright_str = b"(C)PROJ12 2026  "
    header[0x110:0x110 + len(copyright_str)] = copyright_str

    # Domestic name (48 bytes, space-padded)
    dom_name = b"SegaOS - Windowed Operating System               "
    header[0x120:0x120 + min(48, len(dom_name))] = dom_name[:48]

    # Overseas name (48 bytes, space-padded)
    ovs_name = b"SegaOS - Windowed Operating System               "
    header[0x150:0x150 + min(48, len(ovs_name))] = ovs_name[:48]

    # Game type + serial
    serial = b"GM 00000000-00"
    header[0x180:0x180 + len(serial)] = serial

    # Checksum (placeholder)
    struct.pack_into(">H", header, 0x18E, 0x0000)

    # I/O support ("J" for joypad, "6" for 6-button, "M" for mouse)
    io_support = b"JM              "
    header[0x190:0x190 + len(io_support)] = io_support

    # ROM address range
    struct.pack_into(">II", header, 0x1A0, 0x00000000, 0x003FFFFF)

    # RAM address range
    struct.pack_into(">II", header, 0x1A8, 0x00FF0000, 0x00FFFFFF)

    # SRAM info (none)
    header[0x1B0:0x1BC] = b"            "

    # Modem support (none)
    header[0x1BC:0x1C8] = b"            "

    # Memo (40 bytes)
    memo = b"                                        "
    header[0x1C8:0x1C8 + 40] = memo[:40]

    # Region code: "JUE" (Japan, US, Europe)
    region = b"JUE             "
    header[0x1F0:0x200] = region

    return header


def build_boot_sector(ip_data: bytes, sp_data: bytes) -> bytearray:
    """Build the 32KB boot sector."""
    ip_size = len(ip_data)
    sp_size = len(sp_data)

    # Validate sizes
    if ip_size > IP_MAX_SIZE:
        print(f"WARNING: IP binary ({ip_size} bytes) exceeds {IP_MAX_SIZE} byte limit!")
        print("         It may not boot on real hardware.")

    if sp_size > SP_MAX_SIZE:
        print(f"WARNING: SP binary ({sp_size} bytes) exceeds {SP_MAX_SIZE} byte limit!")
        print("         It may not boot on real hardware.")

    # Build header
    header = build_disc_header(ip_size, sp_size)

    # Construct boot sector
    boot = bytearray(BOOT_SIZE)

    # Place header at $0000
    boot[0x00:0x200] = header

    # Place IP at IP_OFFSET
    boot[IP_OFFSET:IP_OFFSET + ip_size] = ip_data

    # Place SP at SP_OFFSET
    boot[SP_OFFSET:SP_OFFSET + sp_size] = sp_data

    return boot


def build_iso9660_descriptors() -> bytearray:
    """Build minimal ISO9660 volume descriptors (sectors 16-17)."""
    # Primary Volume Descriptor (PVD) at sector 16
    pvd = bytearray(SECTOR_SIZE)
    pvd[0] = 0x01  # Type: Primary Volume Descriptor
    pvd[1:6] = b"CD001"  # Standard Identifier
    pvd[6] = 0x01  # Version
    pvd[7] = 0x00  # Unused

    # System Identifier (32 bytes, a-chars)
    sys_id = b"MEGA_CD                         "
    pvd[8:40] = sys_id

    # Volume Identifier (32 bytes, d-chars)
    vol_id = b"SEGAOS                          "
    pvd[40:72] = vol_id

    # Volume Space Size (both-endian 32-bit)
    # We'll set this to 18 sectors (boot + descriptors)
    total_sectors = 18
    struct.pack_into("<I", pvd, 80, total_sectors)
    struct.pack_into(">I", pvd, 84, total_sectors)

    # Volume Set Size (both-endian 16-bit)
    struct.pack_into("<H", pvd, 120, 1)
    struct.pack_into(">H", pvd, 122, 1)

    # Volume Sequence Number
    struct.pack_into("<H", pvd, 124, 1)
    struct.pack_into(">H", pvd, 126, 1)

    # Logical Block Size (both-endian 16-bit)
    struct.pack_into("<H", pvd, 128, SECTOR_SIZE)
    struct.pack_into(">H", pvd, 130, SECTOR_SIZE)

    # Root directory record (minimal, empty — we have no files on disc)
    # At offset 156, 34 bytes for root directory record
    root_dir = bytearray(34)
    root_dir[0] = 34  # Length of directory record
    root_dir[1] = 0   # Extended attribute record length
    # Location of extent (sector 17, both-endian)
    struct.pack_into("<I", root_dir, 2, 17)
    struct.pack_into(">I", root_dir, 6, 17)
    # Data length (both-endian)
    struct.pack_into("<I", root_dir, 10, SECTOR_SIZE)
    struct.pack_into(">I", root_dir, 14, SECTOR_SIZE)
    # Recording date/time (7 bytes)
    root_dir[18:25] = bytes([126, 2, 10, 15, 0, 0, 0])  # 2026-02-10 15:00:00
    # File flags: directory
    root_dir[25] = 0x02
    # File unit size, interleave gap
    root_dir[26] = 0
    root_dir[27] = 0
    # Volume sequence number (both-endian)
    struct.pack_into("<H", root_dir, 28, 1)
    struct.pack_into(">H", root_dir, 30, 1)
    # File identifier length
    root_dir[32] = 1
    # File identifier (root = 0x00)
    root_dir[33] = 0x00

    pvd[156:156 + 34] = root_dir

    # Volume set/publisher/etc identifiers (space-filled, optional)
    pvd[190:318] = b" " * 128   # Volume Set Identifier
    pvd[318:446] = b" " * 128   # Publisher Identifier
    pvd[446:574] = b" " * 128   # Data Preparer Identifier
    pvd[574:702] = b" " * 128   # Application Identifier

    pvd[881] = 0x01  # File Structure Version

    # Volume Descriptor Set Terminator at sector 17
    vdst = bytearray(SECTOR_SIZE)
    vdst[0] = 0xFF  # Type: Terminator
    vdst[1:6] = b"CD001"
    vdst[6] = 0x01

    return pvd + vdst


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <ip.bin> <sp.bin> <output.iso>")
        sys.exit(1)

    ip_path = sys.argv[1]
    sp_path = sys.argv[2]
    out_path = sys.argv[3]

    # Read binaries
    if not os.path.exists(ip_path):
        print(f"ERROR: IP binary not found: {ip_path}")
        sys.exit(1)
    if not os.path.exists(sp_path):
        print(f"ERROR: SP binary not found: {sp_path}")
        sys.exit(1)

    with open(ip_path, "rb") as f:
        ip_data = f.read()
    with open(sp_path, "rb") as f:
        sp_data = f.read()

    print(f"IP (Main CPU): {len(ip_data)} bytes ({len(ip_data):#x})")
    print(f"SP (Sub CPU):  {len(sp_data)} bytes ({len(sp_data):#x})")

    # Check sizes
    if len(ip_data) > IP_MAX_SIZE:
        print(f"WARNING: IP ({len(ip_data)} bytes) > {IP_MAX_SIZE} byte limit!")
    if len(sp_data) > SP_MAX_SIZE:
        print(f"WARNING: SP ({len(sp_data)} bytes) > {SP_MAX_SIZE} byte limit!")

    # Build boot sector (sectors 0-15)
    boot_sector = build_boot_sector(ip_data, sp_data)
    assert len(boot_sector) == BOOT_SIZE

    # Build ISO9660 volume descriptors (sectors 16-17)
    iso_descriptors = build_iso9660_descriptors()
    assert len(iso_descriptors) == 2 * SECTOR_SIZE

    # Write ISO
    with open(out_path, "wb") as f:
        f.write(boot_sector)
        f.write(iso_descriptors)

    total_size = len(boot_sector) + len(iso_descriptors)
    print(f"ISO written: {out_path} ({total_size} bytes, {total_size // SECTOR_SIZE} sectors)")


if __name__ == "__main__":
    main()
