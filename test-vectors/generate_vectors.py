#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Generate reference .bin files from .json test vector descriptions.

Each .json in vectors/ describes a header; this script produces the
matching .bin (raw binary header) next to it.

Usage:
    python generate_vectors.py              # generate all
    python generate_vectors.py v3_header_nosig  # generate one
"""

from __future__ import annotations

import binascii
import json
import struct
import sys
from pathlib import Path

VECTORS_DIR = Path(__file__).parent / "vectors"

# Field layout per version: list of (name, offset, size)
# Matches docs/format/header-v*.md exactly.
FIELDS_V3 = [
    ("magic", 0, 8),
    ("version", 8, 1),
    ("signature_version", 9, 1),
    ("header_reserved", 10, 2),
    ("boardname", 12, 32),
    ("boardversion", 44, 32),
    ("serial", 76, 32),
    ("usid", 108, 32),
    ("cpuid", 140, 32),
    ("mac", 172, 6),
    ("reserved2", 178, 2),
    ("signature", 180, 64),
    ("timestamp", 244, 8),
    ("crc32", 252, 4),
]

FIELDS_V2 = [
    ("magic", 0, 8),
    ("version", 8, 1),
    ("reserved1", 9, 3),
    ("boardname", 12, 32),
    ("boardversion", 44, 32),
    ("serial", 76, 32),
    ("usid", 108, 32),
    ("cpuid", 140, 32),
    ("mac", 172, 6),
    ("reserved2", 178, 2),
    ("reserved3", 180, 72),
    ("crc32", 252, 4),
]

FIELDS_V1 = [
    ("magic", 0, 8),
    ("version", 8, 1),
    ("reserved1", 9, 3),
    ("boardname", 12, 32),
    ("boardversion", 44, 32),
    ("serial", 76, 32),
    ("usid", 108, 32),
    ("cpuid", 140, 32),
    ("mac", 172, 6),
    ("reserved2", 178, 2),
    ("modules", 180, 32),
    ("reserved3", 212, 296),
    ("crc32", 508, 4),
]

VERSION_FIELDS = {1: FIELDS_V1, 2: FIELDS_V2, 3: FIELDS_V3}


def _pack_string(value: str, size: int) -> bytes:
    encoded = value.encode("utf-8")[: size - 1]
    return encoded + b"\x00" * (size - len(encoded))


def _parse_mac(mac_str: str) -> bytes:
    return bytes.fromhex(mac_str.replace(":", "").replace("-", ""))


def generate_binary(spec: dict) -> bytes:
    version = spec["version"]
    header_size = spec["header_size"]
    crc_coverage = spec["crc_coverage"]
    fields_layout = VERSION_FIELDS[version]
    data = spec["fields"]

    buf = bytearray(header_size)

    # Magic + version (common to all)
    buf[0:8] = b"JETHOME\x00"
    buf[8] = version

    # Fill string fields
    for field_name in ("boardname", "boardversion", "serial", "usid", "cpuid"):
        value = data.get(field_name, "")
        for name, offset, size in fields_layout:
            if name == field_name:
                buf[offset : offset + size] = _pack_string(value, size)
                break

    # MAC
    mac_str = data.get("mac", "")
    if mac_str:
        for name, offset, size in fields_layout:
            if name == "mac":
                buf[offset : offset + size] = _parse_mac(mac_str)
                break

    # V3-specific fields
    if version == 3:
        buf[9] = data.get("signature_version", 0)

        sig_hex = data.get("signature_hex", "")
        if sig_hex:
            sig_bytes = bytes.fromhex(sig_hex)
            buf[180 : 180 + len(sig_bytes)] = sig_bytes

        timestamp = data.get("timestamp", 0)
        struct.pack_into("<q", buf, 244, timestamp)

    # CRC32 (last 4 bytes of header, covering bytes 0..crc_coverage-1)
    crc_value = binascii.crc32(bytes(buf[:crc_coverage])) & 0xFFFFFFFF
    crc_offset = header_size - 4
    struct.pack_into("<I", buf, crc_offset, crc_value)

    return bytes(buf)


def main() -> None:
    filter_name = sys.argv[1] if len(sys.argv) > 1 else None

    json_files = sorted(VECTORS_DIR.glob("*.json"))
    if not json_files:
        print(f"No .json files found in {VECTORS_DIR}")
        sys.exit(1)

    for json_path in json_files:
        stem = json_path.stem
        if filter_name and stem != filter_name:
            continue

        with open(json_path) as f:
            spec = json.load(f)

        bin_data = generate_binary(spec)
        bin_path = json_path.with_suffix(".bin")
        bin_path.write_bytes(bin_data)
        print(f"  {stem}: {len(bin_data)} bytes -> {bin_path.name}")


if __name__ == "__main__":
    main()
