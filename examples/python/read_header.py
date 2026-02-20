#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or MIT)
"""Example: Read EEPROM header, print version and MAC address.

Usage: python read_header.py <eeprom.bin>
"""

import struct
import sys
from pathlib import Path

from jeefs.constants import EEPROM_FIELDS, EEPROM_MAGIC
from jeefs.header import EEPROMHeaderV3


def detect_version(data: bytes) -> int | None:
    """Detect header version from raw bytes (works for v1/v2/v3)."""
    if len(data) < 12:
        return None
    if data[0:8] != EEPROM_MAGIC:
        return None
    version = data[8]
    if version in (1, 2, 3):
        return version
    return None


def read_mac(data: bytes) -> str:
    """Read MAC address from raw header bytes (offset 172, 6 bytes)."""
    off, sz = EEPROM_FIELDS["mac"]
    mac_bytes = data[off : off + sz]
    return ":".join(f"{b:02X}" for b in mac_bytes)


def main() -> int:
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <eeprom.bin>", file=sys.stderr)
        return 1

    path = Path(sys.argv[1])
    data = path.read_bytes()

    # Detect version
    version = detect_version(data)
    if version is None:
        print("Error: invalid EEPROM header (bad magic or too short)", file=sys.stderr)
        return 1
    print(f"Header version: {version}")

    # Verify CRC (version-aware)
    crc_coverage = {1: 508, 2: 252, 3: 252}[version]
    header_size = {1: 512, 2: 256, 3: 256}[version]
    if len(data) < header_size:
        print("Error: file too short for detected version", file=sys.stderr)
        return 1

    import binascii

    stored_crc = struct.unpack_from("<I", data, crc_coverage)[0]
    calc_crc = binascii.crc32(data[:crc_coverage]) & 0xFFFFFFFF
    if stored_crc == calc_crc:
        print("CRC32: OK")
    else:
        print("Warning: CRC32 mismatch", file=sys.stderr)

    # Read board name (offset 12, 32 bytes, null-terminated)
    boardname = data[12:44].split(b"\x00")[0].decode("utf-8", errors="replace")
    print(f"Board name: {boardname}")

    # Read MAC address
    mac = read_mac(data)
    print(f"MAC address: {mac}")

    # For v3 headers, demonstrate the full dataclass API
    if version == 3:
        header = EEPROMHeaderV3.from_bytes(data)
        print(f"\n--- Full v3 header (via EEPROMHeaderV3) ---")
        print(f"  Board: {header.boardname} / {header.boardversion}")
        print(f"  Serial: {header.serial}")
        print(f"  Signature: {header.signature_algorithm.name}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
