#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Build the example in an exported, pinned U-Boot tree; never edit the source checkout."""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXAMPLE = ROOT / "examples/uboot"


def stage_source(source: Path, stage: Path) -> dict:
    if stage.exists():
        raise FileExistsError(f"refuse existing stage: {stage}")
    if stage.resolve().is_relative_to(source.resolve()):
        raise ValueError("stage must be outside the source checkout")
    config = json.loads((EXAMPLE / "source.json").read_text())
    commit = config["commit"]
    probe = subprocess.run(["git", "-C", str(source), "cat-file", "-t", commit],
                           capture_output=True, text=True)
    if probe.returncode or probe.stdout.strip() != "commit":
        raise ValueError(f"source does not contain pinned U-Boot commit {commit}")
    # Export tracked content at the pin, not possibly modified working files.
    with tempfile.TemporaryFile() as archive:
        subprocess.run(["git", "-C", str(source), "archive", "--format=tar", commit],
                       stdout=archive, check=True)
        archive.seek(0)
        stage.mkdir(parents=True)
        with tarfile.open(fileobj=archive, mode="r|") as tar:
            tar.extractall(stage, filter="data")
    dest = stage / "lib/jeefs_demo"
    if dest.exists():
        raise ValueError("pinned source unexpectedly contains lib/jeefs_demo")
    dest.mkdir()
    for name in ["read_identity.h", "read_identity.c", "command.c", "args.h", "Makefile"]:
        shutil.copyfile(EXAMPLE / name, dest / name)
    for name in ["jeefs_header.c", "jeefs_devid.c", "jeefs_walk.c"]:
        shutil.copyfile(ROOT / "src" / name, dest / name)
    shutil.copytree(ROOT / "include", dest / "include")
    makefile = stage / "lib/Makefile"
    text = makefile.read_text()
    anchor = "obj-y += crc8.o\n"
    if text.count(anchor) != 1:
        raise ValueError("pinned U-Boot lib/Makefile integration anchor changed")
    makefile.write_text(text.replace(anchor, "obj-$(CONFIG_CMD_I2C) += jeefs_demo/\n" + anchor))
    return config


def build(source: Path, stage: Path, cross_compile: str, jobs: int) -> dict:
    config = stage_source(source, stage)
    output = stage / "build"
    command = ["make", "-C", str(stage), f"O={output}", f"CROSS_COMPILE={cross_compile}"]
    subprocess.run(command + [config["defconfig"]], check=True)
    subprocess.run(command + [f"-j{jobs}", "u-boot"], check=True)
    elf = output / "u-boot"
    symbols = subprocess.check_output([cross_compile + "nm", "--defined-only", str(elf)], text=True)
    for symbol in ["do_jeefs_identity", "jeefs_example_read_identity", "jeefs_walk_feed"]:
        if not any(line.split()[-1:] == [symbol] for line in symbols.splitlines()):
            raise ValueError(f"linked U-Boot ELF is missing {symbol}")
    machine = subprocess.check_output([cross_compile + "readelf", "-h", str(elf)], text=True)
    if "AArch64" not in machine:
        raise ValueError("expected an AArch64 U-Boot ELF")
    result = {"source": config, "elf": str(elf), "command": "jeefs_identity",
              "required_symbols": "present", "machine": "AArch64", "hardware_executed": False}
    (stage / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True, type=Path, help="Git repository containing the pinned commit")
    parser.add_argument("--stage", required=True, type=Path, help="new scratch directory, never an existing checkout")
    parser.add_argument("--cross-compile", default="aarch64-linux-gnu-")
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    try:
        result = build(args.source.resolve(), args.stage.resolve(), args.cross_compile, args.jobs)
    except (OSError, ValueError, subprocess.SubprocessError, tarfile.TarError) as error:
        print(f"U-Boot example build failed: {error}")
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
