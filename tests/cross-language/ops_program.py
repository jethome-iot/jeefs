#!/usr/bin/env python3
# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Compile a .ops scenario into the binary program the runners execute.

A .ops file is authored by people; a program is read by machines. Every
runner used to parse the text itself, which meant one hand-written parser
per language for a format with no specification — and they disagreed.
`strtoul` read `12junk` as 12 while Rust's parser refused it; `%2x` took
`0g` as a byte while Rust rejected the pair; an offset of 4294967296
became 0 in C and stayed out of range in Rust; a NUL ended a token on one
side and not the other. Each disagreement made the comparison report a
divergence between the *ports* that was really a divergence between the
*parsers* — a failure of the one thing the comparison exists to do.

So the text is parsed exactly once, here, and every runner receives the
same program. A runner reads fixed-width integers and length-prefixed
blobs; there is nothing left for it to interpret differently, and a new
port (Go, #110) implements a reader rather than re-deriving C's sscanf
quirks.

Program format, little-endian throughout:

    "JOPS" magic, u8 version (1)
    then, until end of file, one operation each:
        u8 opcode, followed by that opcode's fields
    u32 field:  4 bytes
    blob field: u32 length, then that many bytes

Opcodes and fields:

    0  init         u8 kind (0 zeros, 1 erased, 2 garbage), u32 size
    1  format       u32 version
    2  add          blob name, blob data
    3  write        blob name, blob data
    4  delete       blob name
    5  read         blob name, u32 capacity
    6  list         —
    7  walk         blob name
    8  poke         u32 offset, u32 value
    9  reseal       u32 offset
    10 consistency  —

The compiler is strict: a scenario it cannot read is an error here, in
one place, rather than an outcome two runners might disagree about.
"""

from __future__ import annotations

import struct

MAGIC = b"JOPS"
VERSION = 1

OP_INIT, OP_FORMAT, OP_ADD, OP_WRITE, OP_DELETE = 0, 1, 2, 3, 4
OP_READ, OP_LIST, OP_WALK, OP_POKE, OP_RESEAL, OP_CONSISTENCY = 5, 6, 7, 8, 9, 10

IMAGE_KINDS = {"zeros": 0, "erased": 1, "garbage": 2}

MAX_IMAGE = 65535
MAX_DATA = 32767
# A name field is 16 bytes on the wire. A scenario has to be able to exceed
# that — testing that a port refuses an over-long name is the point — but
# not without limit: the C runner copies a name into a fixed buffer and a
# blob past it would abort that runner while the Rust one answered
# name_invalid. Capping here, in the one parser, keeps both runners
# answering the same thing for every scenario the compiler accepts.
MAX_NAME = 255


class ScenarioError(ValueError):
    """A scenario that cannot be compiled. The vector is wrong, not a port."""


def _u32(value: int, what: str) -> bytes:
    if not 0 <= value <= 0xFFFFFFFF:
        raise ScenarioError(f"{what} out of range: {value}")
    return struct.pack("<I", value)


def _blob(data: bytes) -> bytes:
    return struct.pack("<I", len(data)) + data


def _number(token: str, what: str) -> int:
    """A decimal or 0x-prefixed integer, whole token, no sign.

    Deliberately narrower than any language's library parser: those are
    where the runners used to differ.
    """
    text = token
    base = 10
    if text[:2].lower() == "0x":
        text, base = text[2:], 16
    if not text:
        raise ScenarioError(f"{what}: empty number {token!r}")
    digits = "0123456789abcdef"[:base]
    if any(c.lower() not in digits for c in text):
        raise ScenarioError(f"{what}: not a base-{base} number: {token!r}")
    return int(text, base)


def _hex_byte(token: str, what: str) -> int:
    """A byte value, always hexadecimal — `poke 284 ff`, as the vectors write it.

    Its own reader because the radix differs from every other number in the
    format, and because the runners used to disagree here too: `strtoul`
    read `1ff` as 511 and the cast made it 0xFF, while Rust overflowed `u8`
    and wrote 0x00.
    """
    text = token[2:] if token[:2].lower() == "0x" else token
    if not text or len(text) > 2 or any(c not in "0123456789abcdefABCDEF" for c in text):
        raise ScenarioError(f"{what}: not a hex byte: {token!r}")
    return int(text, 16)


def _name(token: str, what: str) -> bytes:
    """A file name, as the bytes the medium will carry.

    Encoded UTF-8 so a scenario can name something outside the format's
    printable-ASCII domain (#116) and watch every port refuse it.
    """
    data = token.encode("utf-8")
    if b"\x00" in data:
        raise ScenarioError(f"{what}: NUL in a name")
    if len(data) > MAX_NAME:
        raise ScenarioError(f"{what}: name of {len(data)} bytes, over the {MAX_NAME}-byte scenario limit")
    return data


def _payload(token: str, what: str) -> bytes:
    """`fill:<byte>:<count>`, or an even-length hex string."""
    if token.startswith("fill:"):
        parts = token.split(":")
        if len(parts) != 3:
            raise ScenarioError(f"{what}: malformed fill spec {token!r}")
        byte = _number(parts[1], f"{what} fill byte")
        count = _number(parts[2], f"{what} fill count")
        if byte > 0xFF:
            raise ScenarioError(f"{what}: fill byte out of range: {byte}")
        if count > MAX_DATA:
            raise ScenarioError(f"{what}: fill count out of range: {count}")
        return bytes([byte]) * count
    if len(token) % 2 or not token:
        raise ScenarioError(f"{what}: hex payload must be an even number of digits: {token!r}")
    try:
        data = bytes.fromhex(token)
    except ValueError as exc:
        raise ScenarioError(f"{what}: not hex: {token!r}") from exc
    # The same ceiling the fill form gets: a payload past it is one the
    # runners refuse, and a scenario the runners refuse has to fail here
    # instead — that is the whole point of there being one parser.
    if len(data) > MAX_DATA:
        raise ScenarioError(f"{what}: payload of {len(data)} bytes, over the {MAX_DATA}-byte limit")
    return data


def compile_scenario(text: str, source: str = "<scenario>") -> bytes:
    """Compile scenario text into a program, or raise ScenarioError."""
    if "\x00" in text:
        raise ScenarioError(f"{source}: NUL byte in a text scenario")

    out = bytearray(MAGIC + bytes([VERSION]))
    for lineno, raw in enumerate(text.splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        op, args = parts[0], parts[1:]
        where = f"{source}:{lineno} ({op})"

        def need(count: int) -> None:
            if len(args) != count:
                raise ScenarioError(f"{where}: expects {count} argument(s), got {len(args)}")

        if op == "init":
            need(2)
            if args[0] not in IMAGE_KINDS:
                raise ScenarioError(f"{where}: unknown image kind {args[0]!r}")
            size = _number(args[1], f"{where} size")
            if not 0 < size <= MAX_IMAGE:
                raise ScenarioError(f"{where}: image size out of range: {size}")
            out += bytes([OP_INIT, IMAGE_KINDS[args[0]]]) + _u32(size, "size")
        elif op == "format":
            need(1)
            out += bytes([OP_FORMAT]) + _u32(_number(args[0], f"{where} version"), "version")
        elif op in ("add", "write"):
            need(2)
            out += bytes([OP_ADD if op == "add" else OP_WRITE])
            out += _blob(_name(args[0], where)) + _blob(_payload(args[1], where))
        elif op == "delete":
            need(1)
            out += bytes([OP_DELETE]) + _blob(_name(args[0], where))
        elif op == "read":
            need(2)
            out += bytes([OP_READ]) + _blob(_name(args[0], where))
            out += _u32(_number(args[1], f"{where} capacity"), "capacity")
        elif op == "walk":
            need(1)
            out += bytes([OP_WALK]) + _blob(_name(args[0], where))
        elif op == "poke":
            need(2)
            out += bytes([OP_POKE]) + _u32(_number(args[0], f"{where} offset"), "offset")
            out += _u32(_hex_byte(args[1], f"{where} value"), "value")
        elif op == "reseal":
            need(1)
            out += bytes([OP_RESEAL]) + _u32(_number(args[0], f"{where} offset"), "offset")
        elif op == "list":
            need(0)
            out += bytes([OP_LIST])
        elif op == "consistency":
            need(0)
            out += bytes([OP_CONSISTENCY])
        else:
            raise ScenarioError(f"{where}: unknown operation")
    return bytes(out)
