#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Cross-language verification: read a .bin header, verify fields match JSON.

Usage: verify_py.py <bin_file> <json_file>
"""

from __future__ import annotations

import binascii
import json
import struct
import sys
from pathlib import Path

# Add the python package to sys.path so we can import jeefs
REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "python"))

from jeefs.constants_generated import (  # noqa: E402
    EEPROM_FIELDS,
    EEPROM_FIELDS_V1,
    EEPROM_FIELDS_V2,
)

VERSION_FIELDS = {
    1: EEPROM_FIELDS_V1,
    2: EEPROM_FIELDS_V2,
    3: EEPROM_FIELDS,
}

VERSION_HEADER_SIZES = {1: 512, 2: 256, 3: 256}


def _unpack_string(data: bytes) -> str:
    return data.split(b"\x00")[0].decode("utf-8", errors="replace")


def _parse_mac(data: bytes) -> str:
    return ":".join(f"{b:02X}" for b in data)


def verify(bin_path: str, json_path: str) -> int:
    bin_data = Path(bin_path).read_bytes()
    spec = json.loads(Path(json_path).read_text())

    version = spec["version"]
    expected_size = spec["header_size"]
    crc_coverage = spec["crc_coverage"]
    fields_map = VERSION_FIELDS[version]
    json_fields = spec["fields"]

    failures = 0

    print(f"Verifying: {bin_path} (version {version}, {len(bin_data)} bytes)")

    # Check magic
    magic = bin_data[0:8]
    if magic != b"JETHOME\x00":
        print(f"  FAIL: magic = {magic!r} (expected b'JETHOME\\x00')")
        failures += 1
    else:
        print("  OK: magic = JETHOME")

    # Check version byte
    actual_ver = bin_data[8]
    if actual_ver != version:
        print(f"  FAIL: version = {actual_ver} (expected {version})")
        failures += 1
    else:
        print(f"  OK: version = {version}")

    # Check size
    if len(bin_data) != expected_size:
        print(f"  FAIL: size = {len(bin_data)} (expected {expected_size})")
        failures += 1
    else:
        print(f"  OK: size = {expected_size}")

    # Check CRC32
    crc_off = expected_size - 4
    stored_crc = struct.unpack("<I", bin_data[crc_off : crc_off + 4])[0]
    calc_crc = binascii.crc32(bin_data[:crc_coverage]) & 0xFFFFFFFF
    if stored_crc != calc_crc:
        print(f"  FAIL: CRC32 stored=0x{stored_crc:08x} calculated=0x{calc_crc:08x}")
        failures += 1
    else:
        print(f"  OK: CRC32 = 0x{stored_crc:08x}")

    # Check string fields
    for field_name in ("boardname", "boardversion", "serial", "usid", "cpuid"):
        expected = json_fields.get(field_name, "")
        if field_name in fields_map:
            off, sz = fields_map[field_name]
            actual = _unpack_string(bin_data[off : off + sz])
            if actual != expected:
                print(f"  FAIL: {field_name} = {actual!r} (expected {expected!r})")
                failures += 1
            else:
                print(f"  OK: {field_name} = {actual!r}")

    # MAC
    mac_expected = json_fields.get("mac", "")
    if mac_expected and "mac" in fields_map:
        off, sz = fields_map["mac"]
        mac_actual = _parse_mac(bin_data[off : off + sz])
        mac_expected_norm = mac_expected.upper()
        if mac_actual != mac_expected_norm:
            print(f"  FAIL: mac = {mac_actual} (expected {mac_expected_norm})")
            failures += 1
        else:
            print(f"  OK: mac = {mac_actual}")

    # V3-specific
    if version == 3:
        sig_ver = json_fields.get("signature_version", 0)
        actual_sig_ver = bin_data[9]
        if actual_sig_ver != sig_ver:
            print(f"  FAIL: signature_version = {actual_sig_ver} (expected {sig_ver})")
            failures += 1
        else:
            print(f"  OK: signature_version = {sig_ver}")

        ts_expected = json_fields.get("timestamp", 0)
        ts_actual = struct.unpack("<q", bin_data[244:252])[0]
        if ts_actual != ts_expected:
            print(f"  FAIL: timestamp = {ts_actual} (expected {ts_expected})")
            failures += 1
        else:
            print(f"  OK: timestamp = {ts_actual}")

        sig_hex = json_fields.get("signature_hex", "")
        if sig_hex:
            expected_sig = bytes.fromhex(sig_hex)
            actual_sig = bin_data[180 : 180 + len(expected_sig)]
            if actual_sig != expected_sig:
                print(f"  FAIL: signature mismatch")
                failures += 1
            else:
                print(f"  OK: signature ({len(expected_sig)} bytes)")

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
