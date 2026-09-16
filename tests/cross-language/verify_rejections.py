#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Prove the verifiers reject what they are supposed to reject.

The NxN matrix only ever feeds a verifier a freshly generated, valid
binary, so every rejection branch in it is unexercised: a verifier that
stopped checking the signature padding, the MAC or the timestamp would
still pass the whole matrix. This runs the opposite direction — take a
committed vector, break one thing about it, and require every verifier
to fail — so those branches cannot rot unnoticed.

Each mutation repairs the header CRC afterwards, so what is being tested
is the field check itself and not the CRC gate standing in front of it.

A verifier is given as one argument; if it needs an interpreter, separate
the parts with a tab so a path containing spaces stays intact.

Usage: verify_rejections.py <vectors_dir> <work_dir> <verifier> [verifier ...]
"""

from __future__ import annotations

import binascii
import json
import struct
import subprocess
import sys
from pathlib import Path


def reseal(buf: bytearray) -> bytes:
    """Recompute the header CRC so a mutation is not caught by it instead."""
    coverage = 508 if len(buf) == 512 else 252
    struct.pack_into("<I", buf, len(buf) - 4, binascii.crc32(bytes(buf[:coverage])) & 0xFFFFFFFF)
    return bytes(buf)


def mutate_bin(data: bytes, offset: int, value: bytes) -> bytes:
    buf = bytearray(data)
    buf[offset : offset + len(value)] = value
    return reseal(buf)


def spec_with(spec: dict, **fields) -> dict:
    out = json.loads(json.dumps(spec))
    out["fields"].update(fields)
    return out


def cases(vectors: Path) -> list[tuple[str, bytes, dict]]:
    """Every case is (name, binary, spec) that a verifier must reject."""
    v3 = json.loads((vectors / "v3_header_secp192r1.json").read_text())
    v3_bin = (vectors / "v3_header_secp192r1.bin").read_bytes()
    v4 = json.loads((vectors / "v4_header_mac_erased.json").read_text())
    v4_bin = (vectors / "v4_header_mac_erased.bin").read_bytes()
    v1 = json.loads((vectors / "v1_header_identifiers.json").read_text())
    v1_bin = (vectors / "v1_header_identifiers.bin").read_bytes()
    nosig = json.loads((vectors / "v3_header_nosig.json").read_text())
    nosig_bin = (vectors / "v3_header_nosig.bin").read_bytes()

    return [
        # The padding convention a short signature relies on.
        ("signature padding filled with garbage", mutate_bin(v3_bin, 228, b"\xDE" * 16), v3),
        ("signature body altered", mutate_bin(v3_bin, 180, b"\x00"), v3),
        # A declared algorithm must match the length it is given. Both the
        # binary and the spec agree here, so the only thing wrong is that
        # the algorithm does not permit that length — nothing else in the
        # verifier can catch these two.
        (
            "P-192 declared with a 64-byte signature",
            mutate_bin(v3_bin, 180, bytes(range(1, 65))),
            spec_with(v3, signature_version=1, signature_hex=bytes(range(1, 65)).hex()),
        ),
        (
            "no signature declared, bytes present",
            mutate_bin(mutate_bin(v3_bin, 9, b"\x00"), 180, bytes(range(1, 49))),
            spec_with(v3, signature_version=0, signature_hex=bytes(range(1, 49)).hex()),
        ),
        # The two empty domains are not interchangeable.
        ("erased MAC rewritten as zeros", mutate_bin(v4_bin, 172, b"\x00" * 6), v4),
        # Bounded strings use the whole field.
        ("full-width identifier truncated by a NUL", mutate_bin(v1_bin, 108 + 31, b"\x00"), v1),
        # Tail fields.
        ("timestamp altered", mutate_bin(v3_bin, 244, struct.pack("<q", 1)), v3),
        # Malformed expectations must be reported, not crash the verifier.
        ("signature_hex not hex", v3_bin, spec_with(v3, signature_hex="0g" * 24)),
        ("signature_hex longer than the field", v3_bin, spec_with(v3, signature_hex="00" * 65)),
        ("signature_hex is not a string", v3_bin, spec_with(v3, signature_hex=17)),
        # Metadata a verifier must not quietly normalise or ignore: the four
        # ports have to agree on what a malformed expectation is.
        ("signature_hex padded with spaces", v3_bin, spec_with(v3, signature_hex="a1 a2 " + "00" * 46)),
        ("timestamp is not an integer", v3_bin, spec_with(v3, timestamp="garbage")),
        (
            "no-signature header with a non-string signature_hex",
            mutate_bin(v3_bin, 9, b"\x00"),
            spec_with(v3, signature_version=0, signature_hex=17),
        ),
        # The v4 arm is separate code in every port: a check dropped from it
        # alone would pass every v3 case above.
        ("v4 signature padding filled with garbage", mutate_bin(v4_bin, 200, b"\xDE" * 40), v4),
        ("v4 timestamp altered", mutate_bin(v4_bin, 244, struct.pack("<q", 1)), v4),
        # An absent signature means all 64 bytes are zero, not just the ones
        # a shorter algorithm would have used.
        ("no-signature field carrying bytes", mutate_bin(nosig_bin, 180, b"\x7F" * 64), nosig),
        # An explicit null is malformed metadata, not an omitted field.
        ("signature_hex is null", v3_bin, spec_with(v3, signature_hex=None)),
        ("timestamp is null", v3_bin, spec_with(v3, timestamp=None)),
        ("timestamp is a float", v3_bin, spec_with(v3, timestamp=1755300000.5)),
        # Numeric metadata: the ports must agree on what an integer is.
        ("signature_version as a float", v3_bin, spec_with(v3, signature_version=1.0)),
        ("timestamp as a float", v3_bin, spec_with(v3, timestamp=float(v3["fields"]["timestamp"]))),
        ("timestamp out of 64-bit range", v3_bin, spec_with(v3, timestamp=2**63)),
        # Truncated media.
        ("file truncated to 12 bytes", v3_bin[:12], v3),
        ("file truncated to 5 bytes", v3_bin[:5], v3),
        ("file empty", b"", v3),
    ]


def main() -> None:
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(2)
    vectors, work = Path(sys.argv[1]), Path(sys.argv[2])
    verifiers = sys.argv[3:]
    work.mkdir(parents=True, exist_ok=True)

    failures = 0
    for name, data, spec in cases(vectors):
        stem = name.replace(" ", "_").replace(",", "")
        bin_path, json_path = work / f"{stem}.bin", work / f"{stem}.json"
        bin_path.write_bytes(data)
        json_path.write_text(json.dumps(spec, indent=2))

        for verifier in verifiers:
            parts = verifier.split("\t")
            argv = parts + [str(bin_path), str(json_path)]
            proc = subprocess.run(argv, capture_output=True, text=True, check=False)
            label = Path(parts[-1]).name

            # Exactly 1 is "I checked this and it is wrong". Anything else —
            # 0 for acceptance, 2 for a usage error, 101 for a Rust panic, a
            # negative value for a signal — means the verifier did not do the
            # job, and counting those as rejections would hide a crash.
            if proc.returncode == 1 and "Traceback" not in proc.stderr:
                continue
            if proc.returncode == 0:
                print(f"FAIL: {label} accepted a header with: {name}")
            elif proc.returncode < 0:
                print(f"FAIL: {label} died on signal {-proc.returncode} with: {name}")
            elif "Traceback" in proc.stderr:
                print(f"FAIL: {label} raised instead of reporting with: {name}")
            else:
                print(f"FAIL: {label} exited {proc.returncode}, not a verification failure, with: {name}")
            failures += 1

    total = len(cases(vectors)) * len(verifiers)
    if failures:
        print(f"\nFAIL: {failures}/{total} rejection checks did not hold")
        sys.exit(1)
    print(f"OK: every verifier rejected all {len(cases(vectors))} mutations ({total} checks)")


if __name__ == "__main__":
    main()
