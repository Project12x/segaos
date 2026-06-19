#!/usr/bin/env python3
"""
build_iso.py - Sega CD Boot Disc Image Builder

Builds a cooked ISO9660 data track with a 32KB Sega CD system area.
This matches the Megadev 1.2.0 `mkisofs -G boot.bin` model while avoiding
external Python packages.

Reference baseline:
  repo: https://github.com/drojaazu/megadev
  commit: 7a7246c14b845ad2f1bd3c7d73afb04cf67d83ef
  license: MIT
  reuse mode: pattern-only

Usage: python build_iso.py <ip.bin> <sp.bin> <output_base>
Produces: <output_base>.iso and <output_base>.cue
"""

from __future__ import annotations

import struct
import sys
from pathlib import Path


SECTOR_SIZE = 2048
BOOT_SECTORS = 16
BOOT_SIZE = BOOT_SECTORS * SECTOR_SIZE

PVD_SECTOR = 16
VDST_SECTOR = 17
ROOT_DIR_SECTOR = 18
PATH_TABLE_L_SECTOR = 19
PATH_TABLE_M_SECTOR = 20
MIN_SECTORS = 150

# Megadev-compatible header fields and physical placement.
IP_HEADER_OFFSET = 0x0800
IP_HEADER_SIZE = 0x0800
IP_PHYSICAL_OFFSET = 0x0200
SP_PHYSICAL_OFFSET = 0x1000
IP_MAX_SIZE = SP_PHYSICAL_OFFSET - IP_PHYSICAL_OFFSET
SP_MAX_SIZE = BOOT_SIZE - SP_PHYSICAL_OFFSET

HARDWARE_ID_BY_REGION = {
    "JP": b"SEGA MEGA DRIVE",
    "US": b"SEGA GENESIS",
    "EU": b"SEGA MEGA DRIVE",
}


def write_padded(buf: bytearray, offset: int, size: int, value: bytes) -> None:
    if len(value) > size:
        raise ValueError(f"{value!r} is longer than {size} bytes")
    buf[offset : offset + size] = value + bytes(size - len(value))


def write_space_padded(buf: bytearray, offset: int, size: int, value: bytes) -> None:
    if len(value) > size:
        raise ValueError(f"{value!r} is longer than {size} bytes")
    buf[offset : offset + size] = value + (b" " * (size - len(value)))


def pack_both_u16(value: int) -> bytes:
    return struct.pack("<H", value) + struct.pack(">H", value)


def pack_both_u32(value: int) -> bytes:
    return struct.pack("<I", value) + struct.pack(">I", value)


def check_payload_sizes(ip_size: int, sp_size: int) -> None:
    if ip_size > IP_MAX_SIZE:
        raise SystemExit(
            f"ERROR: IP is {ip_size} bytes, max is {IP_MAX_SIZE} bytes "
            f"(physical range ${IP_PHYSICAL_OFFSET:04X}-${SP_PHYSICAL_OFFSET - 1:04X})"
        )
    if sp_size > SP_MAX_SIZE:
        raise SystemExit(
            f"ERROR: SP is {sp_size} bytes, max is {SP_MAX_SIZE} bytes "
            f"(physical range ${SP_PHYSICAL_OFFSET:04X}-${BOOT_SIZE - 1:04X})"
        )


def build_boot_sector(ip_data: bytes, sp_data: bytes, region: str) -> bytearray:
    check_payload_sizes(len(ip_data), len(sp_data))
    hardware_id = HARDWARE_ID_BY_REGION[region]

    boot = bytearray(BOOT_SIZE)

    # System header ($0000-$00FF)
    boot[0x0000:0x0010] = b"SEGADISCSYSTEM  "
    write_space_padded(boot, 0x0010, 0x000C, b"SEGAOS")
    struct.pack_into(">HH", boot, 0x001C, 0x0100, 0x0001)
    write_space_padded(boot, 0x0020, 0x000C, b"SEGAOS")
    struct.pack_into(">HH", boot, 0x002C, 0x0100, 0x0000)

    struct.pack_into(
        ">IIII",
        boot,
        0x0030,
        IP_HEADER_OFFSET,
        IP_HEADER_SIZE,
        0x00000000,
        0x00000000,
    )
    struct.pack_into(
        ">IIII",
        boot,
        0x0040,
        SP_PHYSICAL_OFFSET,
        len(sp_data),
        0x00000000,
        0x00000000,
    )

    for offset in range(0x0050, 0x0100, 0x0010):
        boot[offset : offset + 0x0010] = b" " * 0x0010

    # Mega Drive-style disc header ($0100-$01FF)
    write_space_padded(boot, 0x0100, 0x0010, hardware_id)
    boot[0x0110:0x0120] = b"(C)PROJ12 2026  "
    title = b"SEGAOS"
    write_padded(boot, 0x0120, 48, title)
    write_padded(boot, 0x0150, 48, title)
    boot[0x0180:0x018E] = b"GM 00000000-00"
    struct.pack_into(">H", boot, 0x018E, 0x0000)
    boot[0x0190:0x01A0] = b"JM              "
    struct.pack_into(">II", boot, 0x01A0, 0x00000000, 0x003FFFFF)
    struct.pack_into(">II", boot, 0x01A8, 0x00FF0000, 0x00FFFFFF)
    boot[0x01B0:0x01BC] = b"            "
    boot[0x01BC:0x01C8] = b"            "
    boot[0x01C8:0x01F0] = b"                                        "
    boot[0x01F0:0x0200] = b"JUE             "

    boot[IP_PHYSICAL_OFFSET : IP_PHYSICAL_OFFSET + len(ip_data)] = ip_data
    boot[SP_PHYSICAL_OFFSET : SP_PHYSICAL_OFFSET + len(sp_data)] = sp_data

    return boot


def iso_date() -> bytes:
    # 2026-02-10 15:00:00 GMT, matching the project initial release date.
    return bytes([126, 2, 10, 15, 0, 0, 0])


def directory_record(extent: int, data_len: int, file_id: bytes, flags: int) -> bytes:
    record_len = 33 + len(file_id)
    if record_len % 2:
        record_len += 1

    record = bytearray(record_len)
    record[0] = record_len
    record[1] = 0
    record[2:10] = pack_both_u32(extent)
    record[10:18] = pack_both_u32(data_len)
    record[18:25] = iso_date()
    record[25] = flags
    record[26] = 0
    record[27] = 0
    record[28:32] = pack_both_u16(1)
    record[32] = len(file_id)
    record[33 : 33 + len(file_id)] = file_id
    return bytes(record)


def build_root_dir() -> bytes:
    root = bytearray(SECTOR_SIZE)
    dot = directory_record(ROOT_DIR_SECTOR, SECTOR_SIZE, b"\x00", 0x02)
    dotdot = directory_record(ROOT_DIR_SECTOR, SECTOR_SIZE, b"\x01", 0x02)
    root[0 : len(dot)] = dot
    root[len(dot) : len(dot) + len(dotdot)] = dotdot
    return bytes(root)


def build_path_table(little_endian: bool) -> bytes:
    table = bytearray(SECTOR_SIZE)
    table[0] = 1
    table[1] = 0
    if little_endian:
        struct.pack_into("<I", table, 2, ROOT_DIR_SECTOR)
        struct.pack_into("<H", table, 6, 1)
    else:
        struct.pack_into(">I", table, 2, ROOT_DIR_SECTOR)
        struct.pack_into(">H", table, 6, 1)
    table[8] = 0
    table[9] = 0
    return bytes(table)


def build_pvd(total_sectors: int) -> bytes:
    pvd = bytearray(SECTOR_SIZE)
    pvd[0] = 0x01
    pvd[1:6] = b"CD001"
    pvd[6] = 0x01
    pvd[8:40] = b"MEGA_CD                         "
    pvd[40:72] = b"SEGAOS                          "
    pvd[80:88] = pack_both_u32(total_sectors)
    pvd[120:124] = pack_both_u16(1)
    pvd[124:128] = pack_both_u16(1)
    pvd[128:132] = pack_both_u16(SECTOR_SIZE)
    pvd[132:140] = pack_both_u32(10)
    struct.pack_into("<I", pvd, 140, PATH_TABLE_L_SECTOR)
    struct.pack_into("<I", pvd, 144, 0)
    struct.pack_into(">I", pvd, 148, PATH_TABLE_M_SECTOR)
    struct.pack_into(">I", pvd, 152, 0)
    root_record = directory_record(ROOT_DIR_SECTOR, SECTOR_SIZE, b"\x00", 0x02)
    pvd[156 : 156 + len(root_record)] = root_record
    pvd[881] = 0x01
    return bytes(pvd)


def build_vdst() -> bytes:
    vdst = bytearray(SECTOR_SIZE)
    vdst[0] = 0xFF
    vdst[1:6] = b"CD001"
    vdst[6] = 0x01
    return bytes(vdst)


def write_iso(iso_path: Path, boot_sector: bytes) -> int:
    sectors = [bytes(boot_sector[i : i + SECTOR_SIZE]) for i in range(0, BOOT_SIZE, SECTOR_SIZE)]
    sectors.append(build_pvd(MIN_SECTORS))
    sectors.append(build_vdst())
    sectors.append(build_root_dir())
    sectors.append(build_path_table(little_endian=True))
    sectors.append(build_path_table(little_endian=False))

    while len(sectors) < MIN_SECTORS:
        sectors.append(bytes(SECTOR_SIZE))

    iso_path.parent.mkdir(parents=True, exist_ok=True)
    with iso_path.open("wb") as f:
        for sector in sectors:
            if len(sector) != SECTOR_SIZE:
                raise AssertionError(f"internal sector length error: {len(sector)}")
            f.write(sector)

    return len(sectors)


def write_cue(cue_path: Path, iso_path: Path) -> None:
    cue_path.write_text(
        f'FILE "{iso_path.name}" BINARY\n'
        "  TRACK 01 MODE1/2048\n"
        "    INDEX 01 00:00:00\n",
        encoding="ascii",
    )


def strip_known_extension(out_base: str) -> str:
    for ext in (".iso", ".bin", ".cue"):
        if out_base.lower().endswith(ext):
            return out_base[: -len(ext)]
    return out_base


def read_payload(path: Path, name: str) -> bytes:
    if not path.exists():
        raise SystemExit(f"ERROR: {name} binary not found: {path}")
    return path.read_bytes()


def main(argv: list[str]) -> int:
    if len(argv) not in (4, 5):
        print(f"Usage: {argv[0]} <ip.bin> <sp.bin> <output_base> [region]")
        return 1

    ip_path = Path(argv[1])
    sp_path = Path(argv[2])
    out_base = Path(strip_known_extension(argv[3]))
    region = argv[4].upper() if len(argv) == 5 else "US"
    if region not in HARDWARE_ID_BY_REGION:
        print(f"ERROR: unsupported region {region!r}; expected one of JP, US, EU")
        return 1
    iso_path = out_base.with_suffix(".iso")
    cue_path = out_base.with_suffix(".cue")

    ip_data = read_payload(ip_path, "IP")
    sp_data = read_payload(sp_path, "SP")

    print(f"IP (Main CPU): {len(ip_data)} bytes ({len(ip_data):#06x})")
    print(f"SP (Sub CPU):  {len(sp_data)} bytes ({len(sp_data):#06x})")
    print("NOTE: this builder only packs the supplied IP bytes.")
    print("      `make iso` runs tools/verify_disc.py to validate the selected security block.")

    boot_sector = build_boot_sector(ip_data, sp_data, region)
    total_sectors = write_iso(iso_path, boot_sector)
    write_cue(cue_path, iso_path)

    total_size = iso_path.stat().st_size
    print(f"ISO written: {iso_path} ({total_size} bytes, {total_sectors} sectors)")
    print(f"CUE written: {cue_path}")
    print(f"Boot sector: {BOOT_SIZE} bytes injected at sectors 0-15")
    print(f"  IP header offset field: ${IP_HEADER_OFFSET:04X}")
    print(f"  IP physical bytes:      ${IP_PHYSICAL_OFFSET:04X}-${IP_PHYSICAL_OFFSET + len(ip_data) - 1:04X}")
    print(f"  SP physical bytes:      ${SP_PHYSICAL_OFFSET:04X}-${SP_PHYSICAL_OFFSET + len(sp_data) - 1:04X}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
