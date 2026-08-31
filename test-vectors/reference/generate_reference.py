#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Generate the golden reference EEPROM binaries from their JSON specs.

Thin driver over the public jeefs image API (jeefs.build_image): the
format lives in one place — python/jeefs/image.py per
docs/format/filesystem-v1.md. The byte-identity of the committed goldens
across this migration is locked by python/tests/test_image.py.

Usage:
    python generate_reference.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent
sys.path.insert(0, str(SCRIPT_DIR.parent.parent / "python"))

from jeefs import EEPROMHeaderV3, EEPROMHeaderV4, build_image


def generate(json_path: Path) -> bytes:
    spec = json.loads(json_path.read_text())
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
    return build_image(header, files, spec["eeprom_size"])


def main() -> None:
    for stem in ("eeprom_full", "eeprom_full_v4"):
        json_path = SCRIPT_DIR / f"{stem}.json"
        bin_path = SCRIPT_DIR / f"{stem}.bin"
        data = generate(json_path)
        bin_path.write_bytes(data)
        print(f"Generated: {bin_path} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
