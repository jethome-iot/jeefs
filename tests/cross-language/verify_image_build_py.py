#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Image-API golden lock: jeefs.build_image must reproduce the committed
golden byte-for-byte from its JSON description, and jeefs.parse_image
must read it back to the same files.

Usage: verify_image_build_py.py <golden.bin> <golden.json>
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(REPO_ROOT / "python"))

from jeefs import EEPROMHeaderV3, EEPROMHeaderV4, build_image, parse_image


def main() -> int:
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <bin_file> <json_file>", file=sys.stderr)
        return 2
    golden = Path(sys.argv[1]).read_bytes()
    spec = json.loads(Path(sys.argv[2]).read_text())

    h = spec["header"]
    cls = EEPROMHeaderV4 if h["version"] == 4 else EEPROMHeaderV3
    serial_key = "board_serial" if h["version"] == 4 else "serial"
    header = cls(
        boardname=h["boardname"],
        boardversion=h["boardversion"],
        usid=h["usid"],
        cpuid=h["cpuid"],
        mac=h["mac"],
        timestamp=h.get("timestamp", 0),
        signature_algorithm=h.get("signature_version", 0),
        **{serial_key: h[serial_key]},
    )
    files = [(f["name"], bytes.fromhex(f["data_hex"])) for f in spec["files"]]
    built = build_image(header, files, spec["eeprom_size"])

    if built != golden:
        diff = next(i for i, (a, b) in enumerate(zip(built, golden)) if a != b)
        print(f"FAIL: built image differs from golden at byte {diff}", file=sys.stderr)
        return 1
    print(f"OK: build_image reproduces {sys.argv[1]} byte-for-byte")

    parsed = parse_image(golden)
    want = [(f["name"], bytes.fromhex(f["data_hex"])) for f in spec["files"]]
    got = [(f.name, f.data) for f in parsed.files]
    if got != want:
        print(f"FAIL: parse_image files mismatch: {[n for n, _ in got]}", file=sys.stderr)
        return 1
    print(f"OK: parse_image reads {len(got)} files back")
    return 0


if __name__ == "__main__":
    sys.exit(main())
