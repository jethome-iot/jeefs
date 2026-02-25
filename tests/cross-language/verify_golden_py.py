#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Golden reference EEPROM binary verification (Python).

Reads the 8192-byte reference image, verifies:
1. Header v3 fields match expected values
2. CRC32 is valid
3. File system linked list contains 3 files
4. File data CRC32 matches

Usage: verify_golden_py.py <eeprom_full.bin> <eeprom_full.json>
"""

from __future__ import annotations

import binascii
import json
import struct
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "python"))

from jeefs.constants_generated import EEPROM_FIELDS  # noqa: E402


def verify(bin_path: str, json_path: str) -> int:
    eeprom = bytearray(Path(bin_path).read_bytes())
    spec = json.loads(Path(json_path).read_text())
    header_spec = spec["header"]
    files_spec = spec["files"]

    failures = 0

    print(f"Verifying golden image: {bin_path} ({len(eeprom)} bytes)")

    # === Header ===
    print("\n=== Header verification ===")

    # Magic
    if eeprom[0:8] != b"JETHOME\x00":
        print(f"  FAIL: magic = {eeprom[0:8]!r}")
        failures += 1
    else:
        print("  OK: magic = JETHOME")

    # Version
    if eeprom[8] != header_spec["version"]:
        print(f"  FAIL: version = {eeprom[8]} (expected {header_spec['version']})")
        failures += 1
    else:
        print(f"  OK: version = {eeprom[8]}")

    # CRC32
    stored_crc = struct.unpack("<I", eeprom[252:256])[0]
    calc_crc = binascii.crc32(bytes(eeprom[:252])) & 0xFFFFFFFF
    if stored_crc != calc_crc:
        print(f"  FAIL: CRC32 stored=0x{stored_crc:08x} calc=0x{calc_crc:08x}")
        failures += 1
    else:
        print(f"  OK: CRC32 = 0x{stored_crc:08x}")

    # String fields
    for field in ("boardname", "boardversion", "serial", "usid", "cpuid"):
        off, sz = EEPROM_FIELDS[field]
        actual = eeprom[off : off + sz].split(b"\x00")[0].decode("utf-8")
        expected = header_spec[field]
        if actual != expected:
            print(f"  FAIL: {field} = {actual!r} (expected {expected!r})")
            failures += 1
        else:
            print(f"  OK: {field} = {actual!r}")

    # MAC
    mac_off, mac_sz = EEPROM_FIELDS["mac"]
    mac_actual = ":".join(f"{b:02X}" for b in eeprom[mac_off : mac_off + mac_sz])
    mac_expected = header_spec["mac"].upper()
    if mac_actual != mac_expected:
        print(f"  FAIL: mac = {mac_actual} (expected {mac_expected})")
        failures += 1
    else:
        print(f"  OK: mac = {mac_actual}")

    # Timestamp
    ts = struct.unpack("<q", eeprom[244:252])[0]
    if ts != header_spec.get("timestamp", 0):
        print(f"  FAIL: timestamp = {ts}")
        failures += 1
    else:
        print(f"  OK: timestamp = {ts}")

    # === Filesystem ===
    print("\n=== Filesystem verification ===")

    offset = header_spec["header_size"]
    file_count = 0

    while offset != 0 and offset < len(eeprom):
        # Read file header
        name = eeprom[offset : offset + 16].split(b"\x00")[0].decode("utf-8")
        if not name:
            break

        data_size = struct.unpack("<H", eeprom[offset + 16 : offset + 18])[0]
        file_crc = struct.unpack("<I", eeprom[offset + 18 : offset + 22])[0]
        next_addr = struct.unpack("<H", eeprom[offset + 22 : offset + 24])[0]

        # Verify name
        if file_count < len(files_spec):
            expected_name = files_spec[file_count]["name"]
            if name != expected_name:
                print(f"  FAIL: file {file_count} name = {name!r} (expected {expected_name!r})")
                failures += 1
            else:
                print(f"  OK: file {file_count} name = {name!r}")

            # Verify data
            expected_data = bytes.fromhex(files_spec[file_count]["data_hex"])
            actual_data = bytes(eeprom[offset + 24 : offset + 24 + data_size])
            if actual_data != expected_data:
                print(f"  FAIL: file {name!r} data mismatch")
                failures += 1
            else:
                print(f"  OK: file {name!r} data ({data_size} bytes)")

        # Verify CRC
        actual_data = bytes(eeprom[offset + 24 : offset + 24 + data_size])
        calc_crc = binascii.crc32(actual_data) & 0xFFFFFFFF
        if calc_crc != file_crc:
            print(f"  FAIL: file {name!r} CRC stored=0x{file_crc:08x} calc=0x{calc_crc:08x}")
            failures += 1
        else:
            print(f"  OK: file {name!r} CRC32 = 0x{file_crc:08x}")

        print(f"  File {file_count}: {name!r} size={data_size} next={next_addr}")
        file_count += 1
        offset = next_addr

    if file_count != len(files_spec):
        print(f"  FAIL: file_count = {file_count} (expected {len(files_spec)})")
        failures += 1
    else:
        print(f"  OK: file_count = {file_count}")

    print(f"\nResult: {failures} failure(s)")
    return failures


def main() -> None:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <bin_file> <json_file>", file=sys.stderr)
        sys.exit(2)
    failures = verify(sys.argv[1], sys.argv[2])
    sys.exit(1 if failures > 0 else 0)


if __name__ == "__main__":
    main()
