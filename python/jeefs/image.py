# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Whole-image build and parse for the JEEFS filesystem (filesystem-v1.md).

The library performs no I/O: :func:`build_image` returns the bytes the
environment writes to the medium, :func:`parse_image` consumes the bytes
the environment read. Semantics follow docs/format/filesystem-v1.md
(28-byte file header, absolute nextFileAddress, double CRC, the
fs_version gate at header offset 10) and header-v4.md (fs_version is
stamped when a filesystem is written, refreshing the header CRC).
"""

from __future__ import annotations

import binascii
import struct
from dataclasses import dataclass, field

from .constants_generated import (
    EEPROM_DEVICE_ID_FILENAME,
    EEPROM_FILE_NAME_LENGTH,
    EEPROM_FS_VERSION,
    EEPROM_FS_VERSION_OFFSET,
)
from .header import EEPROMHeaderV3, EEPROMHeaderV4, detect_version

DEVICE_ID_FILENAME = EEPROM_DEVICE_ID_FILENAME

_FHDR = 28  # sizeof(JEEFSFileHeaderv1)
_HEADER_SIZES = {1: 512, 2: 256, 3: 256, 4: 256}
_MAX_DATA = 32767  # INT16_MAX, filesystem-v1.md constraints
_MAX_IMAGE = 65535  # uint16_t addressing


@dataclass(frozen=True)
class ImageFile:
    """One file of the chain: a name (<= 15 chars) and its payload."""

    name: str
    data: bytes


@dataclass
class ParsedImage:
    """The parse_image result.

    ``header`` is a typed model for v3/v4 headers and ``None`` for the
    obsolete v1/v2 layouts (``header_bytes`` always carries the raw
    header). ``files`` is empty when ``fs_version`` is 0 — the file area
    of such an image is empty regardless of content.
    """

    version: int
    header_bytes: bytes
    header: EEPROMHeaderV3 | None
    fs_version: int
    files: list[ImageFile] = field(default_factory=list)


def _seal_file_header(name: str, data: bytes, next_addr: int) -> bytes:
    raw = bytearray(_FHDR)
    encoded = name.encode("ascii")
    raw[0 : len(encoded)] = encoded
    struct.pack_into("<H", raw, 16, len(data))
    struct.pack_into("<I", raw, 18, binascii.crc32(data) & 0xFFFFFFFF)
    struct.pack_into("<H", raw, 22, next_addr)
    struct.pack_into("<I", raw, 24, binascii.crc32(bytes(raw[:24])) & 0xFFFFFFFF)
    return bytes(raw)


def build_image(
    header: EEPROMHeaderV3, files: list[ImageFile | tuple[str, bytes]], image_size: int
) -> bytes:
    """Build a complete EEPROM image: board header + JEEFS file chain.

    The header is serialized via its own ``to_bytes`` and stamped with
    ``fs_version = 1`` (the image carries a — possibly empty —
    filesystem), refreshing the header CRC; the caller's object is not
    mutated. Files are laid out contiguously in the given order with one
    exception: the reserved ``device.id`` file always takes the first
    slot (filesystem-v1.md). The free space is zero-filled.

    Raises ValueError on invalid names/data, duplicates, an image size
    outside the uint16 range or too small for header + chain (capacity).
    """
    if not 0 < image_size <= _MAX_IMAGE:
        raise ValueError(f"image_size {image_size} outside 1..{_MAX_IMAGE}")

    normalized: list[ImageFile] = []
    for f in files:
        item = f if isinstance(f, ImageFile) else ImageFile(f[0], f[1])
        if not 0 < len(item.name) <= EEPROM_FILE_NAME_LENGTH:
            raise ValueError(f"file name {item.name!r} must be 1..{EEPROM_FILE_NAME_LENGTH} chars")
        if not 0 < len(item.data) <= _MAX_DATA:
            raise ValueError(f"file {item.name!r} data must be 1..{_MAX_DATA} bytes")
        normalized.append(item)
    names = [f.name for f in normalized]
    if len(set(names)) != len(names):
        raise ValueError("duplicate file names in the chain")

    # The reserved device-identity file always occupies the first slot.
    normalized.sort(key=lambda f: f.name != DEVICE_ID_FILENAME)

    header_bytes = bytearray(header.to_bytes())
    header_bytes[EEPROM_FS_VERSION_OFFSET] = EEPROM_FS_VERSION
    struct.pack_into("<I", header_bytes, 252, binascii.crc32(bytes(header_bytes[:252])) & 0xFFFFFFFF)

    chain = bytearray()
    offset = len(header_bytes)
    for i, f in enumerate(normalized):
        end = offset + _FHDR + len(f.data)
        next_addr = 0 if i == len(normalized) - 1 else end
        chain += _seal_file_header(f.name, f.data, next_addr)
        chain += f.data
        offset = end

    total = len(header_bytes) + len(chain)
    if total > image_size:
        raise ValueError(f"capacity exceeded: header + chain = {total} bytes, image_size = {image_size}")

    return bytes(header_bytes) + bytes(chain) + b"\x00" * (image_size - total)


def parse_image(data: bytes) -> ParsedImage:
    """Parse a complete EEPROM image back into header and files.

    Applies the reader rules of filesystem-v1.md: the fs_version byte
    gates the file area (0 = no filesystem, files ignored; unknown =
    error), every visited file header must pass its headerCrc32 before
    any field is trusted, links must be exactly contiguous, and every
    file's data CRC is verified.

    Raises ValueError when no valid board header is present, on an
    unknown fs_version, or on a corrupted chain.
    """
    version = detect_version(data)
    if version is None:
        raise ValueError("no valid board header (magic/version mismatch)")
    header_size = _HEADER_SIZES[version]
    if len(data) < header_size:
        raise ValueError(f"image shorter than the v{version} header")

    header_bytes = bytes(data[:header_size])
    header: EEPROMHeaderV3 | None = None
    if version == 3:
        header = EEPROMHeaderV3.from_bytes(header_bytes)
    elif version == 4:
        header = EEPROMHeaderV4.from_bytes(header_bytes)

    fs_version = data[EEPROM_FS_VERSION_OFFSET]
    result = ParsedImage(version, header_bytes, header, fs_version)
    if fs_version == 0:
        return result  # no filesystem: the file area is empty by contract
    if fs_version != EEPROM_FS_VERSION:
        raise ValueError(f"unsupported fs_version {fs_version}")

    offset = header_size
    while offset != 0 and offset + _FHDR <= len(data):
        raw = data[offset : offset + _FHDR]
        if raw[0] in (0x00, 0xFF):
            break  # unwritten slot: end of chain
        stored_hcrc = struct.unpack("<I", raw[24:28])[0]
        if binascii.crc32(raw[:24]) & 0xFFFFFFFF != stored_hcrc:
            raise ValueError(f"file header CRC mismatch at offset {offset}")
        if raw[EEPROM_FILE_NAME_LENGTH] != 0:
            raise ValueError(f"unterminated file name at offset {offset}")
        name = raw[:16].split(b"\x00")[0].decode("ascii")
        data_size, stored_crc, next_addr = struct.unpack("<HIH", raw[16:24])
        if data_size == 0 or data_size == 0xFFFF:
            raise ValueError(f"corrupted dataSize at offset {offset}")
        end = offset + _FHDR + data_size
        if end > len(data):
            raise ValueError(f"file {name!r} runs past the image")
        if next_addr == 0xFFFF:
            next_addr = 0  # erased link terminates like 0
        if next_addr != 0 and (next_addr != end or end + _FHDR > len(data)):
            raise ValueError(f"broken chain link at offset {offset}")
        payload = bytes(data[offset + _FHDR : end])
        if binascii.crc32(payload) & 0xFFFFFFFF != stored_crc:
            raise ValueError(f"file {name!r} data CRC mismatch")
        result.files.append(ImageFile(name, payload))
        offset = next_addr

    return result
