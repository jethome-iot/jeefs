#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Lock the Rust FS port to the C core on shared mutation vectors.

Each .ops scenario is applied by both runners; the two must agree on
every operation's outcome (the journal on stdout) and on the resulting
image byte for byte. A drift in chain layout, link rewriting, wiping or
error classification fails here.

Usage: verify_fs_mutation.py <apply_ops_c> <apply_ops_rs> <vector_dir> <work_dir>
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def run(binary: Path, scenario: Path, out: Path) -> list[str]:
    proc = subprocess.run(
        [str(binary), str(scenario), str(out)],
        capture_output=True,
        text=True,
        check=False,
    )
    if proc.returncode != 0:
        print(f"FAIL: {binary.name} exited {proc.returncode} on {scenario.name}")
        print(proc.stderr.strip())
        sys.exit(1)
    return proc.stdout.splitlines()


def compare_journals(scenario: Path, c_log: list[str], rs_log: list[str]) -> bool:
    if c_log == rs_log:
        return True
    print(f"FAIL: {scenario.name}: journals differ")
    for i in range(max(len(c_log), len(rs_log))):
        c_line = c_log[i] if i < len(c_log) else "<missing>"
        rs_line = rs_log[i] if i < len(rs_log) else "<missing>"
        if c_line != rs_line:
            print(f"  line {i}:\n    C:    {c_line}\n    Rust: {rs_line}")
    return False


def compare_images(scenario: Path, c_bin: Path, rs_bin: Path) -> bool:
    c_bytes, rs_bytes = c_bin.read_bytes(), rs_bin.read_bytes()
    if c_bytes == rs_bytes:
        return True
    if len(c_bytes) != len(rs_bytes):
        print(f"FAIL: {scenario.name}: image sizes differ ({len(c_bytes)} vs {len(rs_bytes)})")
        return False
    diff = next(i for i, (a, b) in enumerate(zip(c_bytes, rs_bytes)) if a != b)
    print(
        f"FAIL: {scenario.name}: images differ at byte {diff} "
        f"(C 0x{c_bytes[diff]:02x}, Rust 0x{rs_bytes[diff]:02x})"
    )
    return False


def main() -> None:
    if len(sys.argv) != 5:
        print(__doc__)
        sys.exit(2)
    apply_c, apply_rs, vector_dir, work_dir = (Path(a) for a in sys.argv[1:])
    work_dir.mkdir(parents=True, exist_ok=True)

    scenarios = sorted(vector_dir.glob("*.ops"))
    if not scenarios:
        print(f"FAIL: no .ops vectors in {vector_dir}")
        sys.exit(1)

    failures = 0
    for scenario in scenarios:
        c_bin = work_dir / f"{scenario.stem}.c.bin"
        rs_bin = work_dir / f"{scenario.stem}.rs.bin"
        c_log = run(apply_c, scenario, c_bin)
        rs_log = run(apply_rs, scenario, rs_bin)

        ok = compare_journals(scenario, c_log, rs_log)
        ok = compare_images(scenario, c_bin, rs_bin) and ok
        if ok:
            print(f"OK: {scenario.name} ({len(c_log)} ops, {len(c_bin.read_bytes())} bytes identical)")
        else:
            failures += 1

    if failures:
        print(f"FAIL: {failures}/{len(scenarios)} vectors diverge")
        sys.exit(1)
    print(f"OK: C and Rust agree on all {len(scenarios)} mutation vectors")


if __name__ == "__main__":
    main()
