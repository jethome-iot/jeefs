#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Generate the golden reference EEPROM binary from eeprom_full.json.

Creates an 8192-byte EEPROM image with:
  - JEEPROMHeaderv3 at offset 0 (256 bytes)
  - JEEFS files as a linked list starting at offset 256

Each file entry is:
  - JEEFSFileHeaderv1 (24 bytes): name[16], dataSize(u16 LE), crc32(u32 LE), nextFileAddress(u16 LE)
  - Followed by dataSize bytes of file data

Usage:
    python generate_reference.py
"""

from __future__ import annotations

import binascii
import json
import struct
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
JSON_PATH = SCRIPT_DIR / "eeprom_full.json"
BIN_PATH = SCRIPT_DIR / "eeprom_full.bin"


def _pack_string(value: str, size: int) -> bytes:
    encoded = value.encode("utf-8")[: size - 1]
    return encoded + b"\x00" * (size - len(encoded))


def _parse_mac(mac_str: str) -> bytes:
    return bytes.fromhex(mac_str.replace(":", "").replace("-", ""))


def generate() -> bytes:
    spec = json.loads(JSON_PATH.read_text())
    eeprom_size = spec["eeprom_size"]
    header_spec = spec["header"]
    files_spec = spec["files"]

    buf = bytearray(eeprom_size)

    # === Build header (v3, 256 bytes) ===
    header_size = header_spec["header_size"]
    buf[0:8] = b"JETHOME\x00"
    buf[8] = header_spec["version"]
    buf[9] = header_spec.get("signature_version", 0)

    buf[12:44] = _pack_string(header_spec["boardname"], 32)
    buf[44:76] = _pack_string(header_spec["boardversion"], 32)
    buf[76:108] = _pack_string(header_spec["serial"], 32)
    buf[108:140] = _pack_string(header_spec["usid"], 32)
    buf[140:172] = _pack_string(header_spec["cpuid"], 32)
    buf[172:178] = _parse_mac(header_spec["mac"])

    timestamp = header_spec.get("timestamp", 0)
    struct.pack_into("<q", buf, 244, timestamp)

    # CRC32 of header (bytes 0-251)
    crc = binascii.crc32(bytes(buf[:252])) & 0xFFFFFFFF
    struct.pack_into("<I", buf, 252, crc)

    # === Build filesystem linked list ===
    offset = header_size  # first file starts right after header

    for i, file_spec in enumerate(files_spec):
        name = file_spec["name"]
        data = bytes.fromhex(file_spec["data_hex"])
        data_size = len(data)

        # Calculate next file address
        file_header_size = 24
        next_offset = offset + file_header_size + data_size

        # Is there a next file?
        is_last = i == len(files_spec) - 1
        next_file_addr = 0 if is_last else next_offset

        # Write file header: name[16] + dataSize(u16) + crc32(u32) + nextFileAddress(u16)
        name_bytes = _pack_string(name, 16)
        buf[offset : offset + 16] = name_bytes
        struct.pack_into("<H", buf, offset + 16, data_size)
        file_crc = binascii.crc32(data) & 0xFFFFFFFF
        struct.pack_into("<I", buf, offset + 18, file_crc)
        struct.pack_into("<H", buf, offset + 22, next_file_addr)

        # Write file data
        buf[offset + 24 : offset + 24 + data_size] = data

        offset = next_offset

    return bytes(buf)


def main() -> None:
    data = generate()
    BIN_PATH.write_bytes(data)
    print(f"Generated: {BIN_PATH} ({len(data)} bytes)")

    # Print layout summary
    spec = json.loads(JSON_PATH.read_text())
    header_size = spec["header"]["header_size"]
    offset = header_size
    eeprom_size = spec["eeprom_size"]
    for f in spec["files"]:
        file_data = bytes.fromhex(f["data_hex"])
        print(f"  [{offset:4d}-{offset+23:4d}] FileHeader: {f['name']!r}")
        print(f"  [{offset+24:4d}-{offset+23+len(file_data):4d}] Data: {len(file_data)} bytes")
        offset += 24 + len(file_data)
    print(f"  [{offset:4d}-{eeprom_size-1:4d}] Free space")


if __name__ == "__main__":
    main()
