#!/usr/bin/env python3
"""Verify SegaOS cooked ISO/CUE boot-disc layout."""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


SECTOR_SIZE = 2048
MIN_SECTORS = 150
BOOT_SIZE = 16 * SECTOR_SIZE

IP_HEADER_OFFSET = 0x0800
IP_HEADER_SIZE = 0x0800
IP_PHYSICAL_OFFSET = 0x0200
SP_PHYSICAL_OFFSET = 0x1000
IP_MAX_SIZE = SP_PHYSICAL_OFFSET - IP_PHYSICAL_OFFSET
SP_MAX_SIZE = BOOT_SIZE - SP_PHYSICAL_OFFSET

SECURITY_SIGNATURES = {
    "JP": {
        "prefix": bytes.fromhex("21FC00000280FD024BF900A1"),
        "marker": b"PRODUCED BY OR UNDER LICENCE",
    },
    "US": {
        "prefix": bytes.fromhex("43FA000A4EB803646000057A"),
        "marker": b"PRODUCED BY OR UNDER LICENSE",
    },
    "EU": {
        "prefix": bytes.fromhex("43FA000A4EB8036460000564"),
        "marker": b"PRODUCED BY OR UNDER LICENSE",
    },
}

HARDWARE_ID_BY_REGION = {
    "JP": b"SEGA MEGA DRIVE ",
    "US": b"SEGA GENESIS    ",
    "EU": b"SEGA MEGA DRIVE ",
}


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", nargs="?", default="build/segaos.iso")
    parser.add_argument("--cue", default="build/segaos.cue")
    parser.add_argument("--ip", default="build/main_cpu.bin")
    parser.add_argument("--sp", default="build/sub_cpu.bin")
    parser.add_argument("--region", default="US", choices=sorted(SECURITY_SIGNATURES))
    return parser.parse_args(argv[1:])


def read_optional(path: Path) -> bytes | None:
    return path.read_bytes() if path.exists() else None


def add_result(results: list[tuple[bool, str]], ok: bool, message: str) -> None:
    results.append((ok, message))


def check_bytes(results: list[tuple[bool, str]], actual: bytes, expected: bytes, label: str) -> None:
    add_result(results, actual == expected, f"{label}: expected {expected!r}, got {actual!r}")


def verify(args: argparse.Namespace) -> int:
    iso_path = Path(args.iso)
    cue_path = Path(args.cue)
    ip_path = Path(args.ip)
    sp_path = Path(args.sp)

    if not iso_path.exists():
        print(f"FAIL: ISO not found: {iso_path}")
        return 1

    data = iso_path.read_bytes()
    results: list[tuple[bool, str]] = []
    warnings: list[str] = []

    add_result(results, len(data) % SECTOR_SIZE == 0, "ISO size is a multiple of 2048 bytes")
    add_result(results, len(data) >= MIN_SECTORS * SECTOR_SIZE, "ISO has at least 150 sectors")
    add_result(results, len(data) >= BOOT_SIZE + SECTOR_SIZE, "ISO contains boot area and PVD")

    check_bytes(results, data[0x0000:0x0010], b"SEGADISCSYSTEM  ", "Disc ID")
    check_bytes(results, data[0x0010:0x001C], b"SEGAOS      ", "Volume name")
    check_bytes(results, data[0x0020:0x002C], b"SEGAOS      ", "System name")
    add_result(
        results,
        struct.unpack_from(">HH", data, 0x002C) == (0x0100, 0x0000),
        "System version/type matches Sega CD boot header ($0100/$0000)",
    )
    add_result(
        results,
        data[0x0050:0x0100] == b" " * 0x00B0,
        "System header reserved area is space-filled",
    )
    check_bytes(results, data[0x0100:0x0110], HARDWARE_ID_BY_REGION[args.region], "Hardware type")
    check_bytes(results, data[0x01F0:0x0200], b"JUE             ", "Region")

    ip_off, ip_size, ip_entry, ip_work = struct.unpack_from(">IIII", data, 0x0030)
    sp_off, sp_size, sp_entry, sp_work = struct.unpack_from(">IIII", data, 0x0040)
    add_result(results, ip_off == IP_HEADER_OFFSET, f"IP header offset is ${IP_HEADER_OFFSET:04X}")
    add_result(results, ip_size == IP_HEADER_SIZE, f"IP header size is ${IP_HEADER_SIZE:04X}")
    add_result(results, ip_entry == 0 and ip_work == 0, "IP entry/work fields are zero")
    add_result(results, sp_off == SP_PHYSICAL_OFFSET, f"SP offset is ${SP_PHYSICAL_OFFSET:04X}")
    add_result(results, sp_size <= SP_MAX_SIZE, f"SP size fits in boot sector ({sp_size} bytes)")
    add_result(results, sp_entry == 0 and sp_work == 0, "SP entry/work fields are zero")

    ip_data = read_optional(ip_path)
    sp_data = read_optional(sp_path)

    if ip_data is None:
        warnings.append(f"IP binary not found for byte comparison: {ip_path}")
        add_result(results, any(data[IP_PHYSICAL_OFFSET:IP_PHYSICAL_OFFSET + 16]), "IP physical area is nonzero")
        ip_for_security = data[IP_PHYSICAL_OFFSET:SP_PHYSICAL_OFFSET]
    else:
        add_result(results, len(ip_data) <= IP_MAX_SIZE, f"IP binary fits before SP area ({len(ip_data)} bytes)")
        actual_ip = data[IP_PHYSICAL_OFFSET : IP_PHYSICAL_OFFSET + len(ip_data)]
        add_result(results, actual_ip == ip_data, "IP physical bytes match input binary at $0200")
        ip_for_security = ip_data

    security = SECURITY_SIGNATURES[args.region]
    add_result(
        results,
        ip_for_security.startswith(security["prefix"]),
        f"IP starts with {args.region} Sega CD security block prefix",
    )
    add_result(
        results,
        security["marker"] in ip_for_security[:IP_MAX_SIZE],
        f"IP contains {args.region} security license marker",
    )

    if sp_data is None:
        warnings.append(f"SP binary not found for byte comparison: {sp_path}")
        add_result(results, any(data[SP_PHYSICAL_OFFSET:SP_PHYSICAL_OFFSET + 16]), "SP physical area is nonzero")
    else:
        add_result(results, len(sp_data) == sp_size, "SP header size matches input binary")
        actual_sp = data[SP_PHYSICAL_OFFSET : SP_PHYSICAL_OFFSET + len(sp_data)]
        add_result(results, actual_sp == sp_data, "SP physical bytes match input binary at $1000")

    pvd = 16 * SECTOR_SIZE
    add_result(results, data[pvd] == 0x01, "PVD type is primary volume descriptor")
    check_bytes(results, data[pvd + 1 : pvd + 6], b"CD001", "PVD standard ID")
    add_result(results, data[pvd + 6] == 0x01, "PVD version is 1")
    check_bytes(results, data[pvd + 8 : pvd + 40], b"MEGA_CD                         ", "PVD system ID")
    check_bytes(results, data[pvd + 40 : pvd + 72], b"SEGAOS                          ", "PVD volume ID")

    if cue_path.exists():
        cue_text = cue_path.read_text(encoding="ascii")
        add_result(results, f'FILE "{iso_path.name}" BINARY' in cue_text, "CUE references ISO filename")
        add_result(results, "TRACK 01 MODE1/2048" in cue_text, "CUE track is MODE1/2048")
    else:
        add_result(results, False, f"CUE exists: {cue_path}")

    print("=== SegaOS Disc Verification ===")
    print(f"ISO: {iso_path} ({len(data)} bytes, {len(data) // SECTOR_SIZE} sectors)")
    print(f"Region: {args.region}")
    print(f"IP header: offset=0x{ip_off:04X}, size=0x{ip_size:04X}")
    print(f"SP header: offset=0x{sp_off:04X}, size={sp_size} bytes")
    print()

    failures = 0
    for ok, message in results:
        if ok:
            print(f"PASS: {message}")
        else:
            failures += 1
            print(f"FAIL: {message}")

    for warning in warnings:
        print(f"WARN: {warning}")

    return 1 if failures else 0


def main(argv: list[str]) -> int:
    return verify(parse_args(argv))


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
