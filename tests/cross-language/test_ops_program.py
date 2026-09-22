#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Hold the scenario compiler to the forms it exists to refuse.

Nothing else can. The mutation vectors only contain scenarios that
compile, and the comparison they drive comes out green whenever the two
runners agree — which they do on every program the compiler emits. If
this parser started accepting `12junk` or a partial hex pair again, both
runners would still agree about the program it produced and the
cross-language test would keep passing, while the forms that caused #119
crept back in.

So the rejections are the assertions. Each case below is a form that
once made the two hand-written parsers disagree, or a limit the runners
depend on the compiler to enforce.

Run directly; exits non-zero on the first failure.
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ops_program import MAX_DATA, MAX_NAME, ScenarioError, compile_scenario  # noqa: E402

PROLOGUE = "init zeros 8192\nformat 4\n"

# Each entry is a scenario line that must not compile, and why it matters.
REJECTED = [
    # Numbers: every one of these was read differently by strtoul and by
    # Rust's parser, and the difference reached the medium.
    ("poke 12junk ff", "trailing text after a number"),
    ("poke  ff", "missing argument"),
    ("poke 0x ff", "a prefix with no digits"),
    ("poke 0x+10 ff", "a sign inside a number"),
    ("poke -1 ff", "a negative offset"),
    ("poke 4294967296 ff", "an offset past the field"),
    ("poke 99999999999999999999 ff", "an offset past every field"),
    # Byte values: strtoul took 1ff as 511 and the cast made it 0xFF, while
    # u8::from_str_radix overflowed and wrote 0x00.
    ("poke 300 1ff", "a byte value that is not a byte"),
    ("poke 300 zz", "a byte value that is not hex"),
    ("poke 300 ", "a missing byte value"),
    # Payloads: %2x accepted a partial pair where Rust refused it.
    ("add a 0g", "a payload that is not hex"),
    ("add a 000", "an odd number of hex digits"),
    ("add a fill:1:4junk", "trailing text in a fill count"),
    ("add a fill:-0:4", "a signed fill byte"),
    ("add a fill:256:4", "a fill byte that is not a byte"),
    ("add a fill:1", "a malformed fill spec"),
    ("add a " + "00" * (MAX_DATA + 1), "a payload past the ceiling"),
    ("add a fill:1:%d" % (MAX_DATA + 1), "a fill count past the ceiling"),
    # Structure.
    ("init zeros 8192junk", "trailing text in an image size"),
    ("init zeros 0", "an empty image"),
    ("init zeros 65536", "an image past the address space"),
    ("init sideways 512", "an unknown image kind"),
    ("format 4junk", "trailing text in a version"),
    ("read a -1", "a negative capacity"),
    ("list extra", "an argument the operation does not take"),
    ("walk", "a missing argument"),
    ("fly a", "an unknown operation"),
    # Names: the runners cannot agree on these, so they never reach them.
    ("add " + "a" * (MAX_NAME + 1) + " 00", "a name past the format's limit"),
]

# Forms that must keep compiling: a rejection that swallowed these would
# silently shrink what the vectors can express.
ACCEPTED = [
    ("poke 300 ff", "a byte in lowercase hex"),
    ("poke 300 FF", "a byte in uppercase hex"),
    ("poke 300 0xff", "a byte with an explicit prefix"),
    ("poke 300 f", "a single hex digit"),
    ("poke 0x1f ff", "an offset in hex"),
    ("poke 010 ff", "a leading zero, read as decimal"),
    ("add a 00112233", "an even hex payload"),
    ("add a fill:255:16", "a fill at the byte ceiling"),
    ("add " + "a" * MAX_NAME + " 00", "a name at the format's limit"),
    ("add имя 00", "a name outside ASCII, for the ports to refuse"),
    ("add abcdefghijklmnop 00", "a name past the field, for the ports to refuse"),
    ("walk my file", "a name with a non-breaking space"),
]


def main() -> int:
    failures = 0

    for line, why in REJECTED:
        try:
            compile_scenario(PROLOGUE + line + "\n", "test")
        except ScenarioError:
            continue
        print(f"FAIL: compiled {line[:60]!r} — {why} must be refused")
        failures += 1

    for line, why in ACCEPTED:
        try:
            compile_scenario(PROLOGUE + line + "\n", "test")
        except ScenarioError as exc:
            print(f"FAIL: refused {line[:60]!r} — {why} must still compile ({exc})")
            failures += 1

    # A NUL cannot be written as a token, so it is checked on the whole text.
    try:
        compile_scenario(PROLOGUE + "poke 300 f\x00f\n", "test")
        print("FAIL: compiled a scenario containing a NUL byte")
        failures += 1
    except ScenarioError:
        pass

    # The program a good scenario produces is a program, not an accident.
    program = compile_scenario(PROLOGUE + "add a 00\nlist\n", "test")
    if not program.startswith(b"JOPS\x01"):
        print("FAIL: the program does not carry the magic and version")
        failures += 1

    total = len(REJECTED) + len(ACCEPTED) + 2
    if failures:
        print(f"FAIL: {failures}/{total} compiler cases")
        return 1
    print(f"OK: the compiler refuses {len(REJECTED)} malformed forms and accepts {len(ACCEPTED)} good ones")
    return 0


if __name__ == "__main__":
    sys.exit(main())
