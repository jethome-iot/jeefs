#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Lock the Rust FS port to the C core on shared mutation vectors.

Each .ops scenario is applied by both runners; the two must agree on
every operation's outcome (the journal on stdout) and on the resulting
image byte for byte. A drift in chain layout, link rewriting, wiping or
error classification fails here.

With --random N, N generated scenarios are run on top of the committed
ones: the same operations in pseudo-random order against pseudo-random
media, seeded so a failure is reproducible. That is the differential
counterpart of the C fuzz harness — it hunts for a Rust panic or a
silent layout drift on inputs nobody wrote by hand.

Usage: verify_fs_mutation.py <apply_ops_c> <apply_ops_rs> <vector_dir> <work_dir>
                             [--random N] [--seed S]
"""

from __future__ import annotations

import random
import subprocess
import sys
from pathlib import Path

NAMES = ["a", "b", "c", "device.id", "cfg", "0123456789abcde", "x.bin"]
IMAGE_KINDS = ["zeros", "erased", "garbage"]


def random_scenario(rng: random.Random) -> str:
    """A scenario built from the same ops as the committed vectors."""
    size = rng.choice([512, 1024, 4096, 8192])
    lines = [f"init {rng.choice(IMAGE_KINDS)} {size}"]
    if rng.random() < 0.8:
        lines.append(f"format {rng.choice([1, 2, 3, 4, 4, 4])}")
    for _ in range(rng.randint(4, 20)):
        op = rng.choices(
            ["add", "write", "delete", "read", "list", "poke", "consistency"],
            weights=[35, 15, 15, 15, 10, 8, 2],
        )[0]
        name = rng.choice(NAMES)
        if op in ("add", "write"):
            lines.append(f"{op} {name} fill:{rng.randint(0, 255)}:{rng.randint(1, 400)}")
        elif op == "delete":
            lines.append(f"delete {name}")
        elif op in ("list", "consistency"):
            lines.append(op)
        elif op == "read":
            lines.append(f"read {name} {rng.choice([1, 16, 400, 8192])}")
        else:
            lines.append(f"poke {rng.randrange(size)} {rng.randrange(256):02x}")
    lines.append("list")
    lines.append("consistency")
    return "\n".join(lines) + "\n"


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
    args = sys.argv[1:]
    random_count, seed = 0, 20260903
    for flag, target in (("--random", "random_count"), ("--seed", "seed")):
        if flag in args:
            i = args.index(flag)
            try:
                value = int(args[i + 1])
            except (IndexError, ValueError):
                print(f"{flag} needs an integer value\n")
                print(__doc__)
                sys.exit(2)
            args = args[:i] + args[i + 2 :]
            if target == "random_count":
                random_count = value
            else:
                seed = value
    if len(args) != 4:
        print(__doc__)
        sys.exit(2)
    apply_c, apply_rs, vector_dir, work_dir = (Path(a) for a in args)
    work_dir.mkdir(parents=True, exist_ok=True)

    scenarios = sorted(vector_dir.glob("*.ops"))
    if not scenarios:
        print(f"FAIL: no .ops vectors in {vector_dir}")
        sys.exit(1)

    if random_count:
        rng = random.Random(seed)
        generated = work_dir / "generated"
        generated.mkdir(exist_ok=True)
        for n in range(random_count):
            path = generated / f"seed{seed}_{n:04d}.ops"
            path.write_text(random_scenario(rng))
            scenarios.append(path)
        print(f"generated {random_count} random scenarios (seed {seed})")

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
