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

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ops_program import ScenarioError, compile_scenario  # noqa: E402

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
            # walk carries its own weight: the curated vectors drive it over
            # images they build deliberately, and this half has to reach the
            # chains nobody wrote down — a poke away from valid, mid-delete,
            # or on an image a random format left gated.
            ["add", "write", "delete", "read", "list", "walk", "poke", "consistency"],
            weights=[35, 15, 15, 15, 10, 12, 8, 2],
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
        elif op == "walk":
            lines.append(f"walk {name}")
        else:
            lines.append(f"poke {rng.randrange(size)} {rng.randrange(256):02x}")
    lines.append("list")
    lines.append("consistency")
    return "\n".join(lines) + "\n"


def compile_to(scenario: Path, work_dir: Path) -> Path:
    """Parse the scenario once, here, and hand both runners the result.

    The runners used to parse the text themselves, one hand-written parser
    per language, and they disagreed on malformed input in half a dozen
    ways — each disagreement showing up as a divergence between the ports
    (#119). Now the text has exactly one reader.
    """
    try:
        # UTF-8 explicitly: a vector names files outside ASCII on purpose, and
        # read_text() would otherwise follow the process locale and reject a
        # perfectly good vector on a machine set to one.
        text = scenario.read_text(encoding="utf-8")
    except UnicodeDecodeError as exc:
        print(f"FAIL: {scenario.name}: not UTF-8 text: {exc}")
        sys.exit(1)
    try:
        program = compile_scenario(text, scenario.name)
    except ScenarioError as exc:
        print(f"FAIL: {scenario.name}: {exc}")
        sys.exit(1)
    path = work_dir / f"{scenario.stem}.jops"
    path.write_bytes(program)
    return path


def run(binary: Path, scenario: Path, out: Path) -> list[bytes]:
    """Run one runner over a program, returning its journal as raw lines.

    Bytes, not text: a journal carries stored file names, and a medium can
    hold a name outside the format's printable-ASCII domain. Decoding here
    made the comparison crash on exactly the images that most deserve to be
    compared (#119).
    """
    proc = subprocess.run(
        [str(binary), str(scenario), str(out)],
        capture_output=True,
        check=False,
    )
    if proc.returncode != 0:
        print(f"FAIL: {binary.name} exited {proc.returncode} on {scenario.name}")
        print(proc.stderr.decode("utf-8", "replace").strip())
        sys.exit(1)
    return proc.stdout.splitlines()


def _show(line: bytes | None) -> str:
    return "<missing>" if line is None else line.decode("utf-8", "backslashreplace")


def compare_journals(scenario: Path, c_log: list[bytes], rs_log: list[bytes]) -> bool:
    if c_log == rs_log:
        return True
    print(f"FAIL: {scenario.name}: journals differ")
    for i in range(max(len(c_log), len(rs_log))):
        c_line = c_log[i] if i < len(c_log) else None
        rs_line = rs_log[i] if i < len(rs_log) else None
        if c_line != rs_line:
            print(f"  line {i}:\n    C:    {_show(c_line)}\n    Rust: {_show(rs_line)}")
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


def damaged_programs(good: bytes) -> dict[str, bytes]:
    """Programs no compiler would emit, for checking the runners agree anyway.

    A program is a build artifact, so a damaged one means a broken build
    rather than a test outcome — but each runner decides that for itself,
    and two decoders that disagree about what is broken are the shape of
    problem this harness just spent a change removing. Nothing else
    exercises those paths.
    """
    cases = {
        "empty": b"",
        "magic only": good[:4],
        "bad magic": b"XOPS" + good[4:],
        "no version": good[:4],
        "unknown version": good[:4] + bytes([99]) + good[5:],
        "header only": good[:5],
        "unknown opcode": good[:5] + bytes([200]),
        "truncated mid-operation": good[:-1],
    }
    for cut in (6, 9, 12):
        if len(good) > cut:
            cases[f"cut at {cut}"] = good[:cut]
    return cases


def check_damaged(apply_c: Path, apply_rs: Path, work_dir: Path, good: bytes) -> int:
    failures = 0
    for label, program in damaged_programs(good).items():
        path = work_dir / "damaged.jops"
        path.write_bytes(program)
        results = []
        for binary in (apply_c, apply_rs):
            proc = subprocess.run(
                [str(binary), str(path), str(work_dir / "damaged.bin")],
                capture_output=True,
                check=False,
            )
            results.append((proc.returncode, proc.stdout))
        (c_code, c_out), (rs_code, rs_out) = results
        # Exit code and journal must match. A negative code means a signal:
        # a decoder that crashes on a damaged artifact is a decoder that
        # would crash on a damaged medium.
        if c_code < 0 or rs_code < 0 or c_code != rs_code or c_out != rs_out:
            print(f"FAIL: damaged program {label!r}: C exit {c_code}, Rust exit {rs_code}")
            failures += 1
    if not failures:
        print(f"OK: C and Rust agree on all {len(damaged_programs(good))} damaged programs")
    return failures


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
            path.write_text(random_scenario(rng), encoding="utf-8")
            scenarios.append(path)
        print(f"generated {random_count} random scenarios (seed {seed})")

    failures = 0
    first = compile_to(scenarios[0], work_dir)
    failures += check_damaged(apply_c, apply_rs, work_dir, first.read_bytes())

    for scenario in scenarios:
        program = compile_to(scenario, work_dir)
        c_bin = work_dir / f"{scenario.stem}.c.bin"
        rs_bin = work_dir / f"{scenario.stem}.rs.bin"
        c_log = run(apply_c, program, c_bin)
        rs_log = run(apply_rs, program, rs_bin)

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
