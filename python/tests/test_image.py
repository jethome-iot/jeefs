# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Tests for the public EEPROM image API (build_image / parse_image).

Semantics follow docs/format/filesystem-v1.md (28-byte file header,
absolute nextFileAddress, double CRC, fs_version gate) and header-v4.md
(fs_version stamped when a filesystem is written, header CRC refreshed).
"""

import binascii
import json
import struct
from pathlib import Path

import pytest
from jeefs import (
    DEVICE_ID_FILENAME,
    EEPROMHeaderV3,
    EEPROMHeaderV4,
    ImageFile,
    build_image,
    parse_image,
)

REPO = Path(__file__).resolve().parent.parent.parent
REFERENCE = REPO / "test-vectors" / "reference"

FHDR = 28  # file header size


def v4_header(**kw) -> EEPROMHeaderV4:
    defaults = {"boardname": "JetHub-D1p", "boardversion": "2.0", "board_serial": "SN-1", "timestamp": 1700000000}
    defaults.update(kw)
    return EEPROMHeaderV4(**defaults)


class TestBuildImage:
    def test_roundtrip_with_device_id_reordered_first(self):
        files = [("config", b"abc"), (DEVICE_ID_FILENAME, b"\x01" * 256), ("wifi", b"12345")]
        img = build_image(v4_header(), files, 2048)
        assert len(img) == 2048

        parsed = parse_image(img)
        assert parsed.version == 4
        assert parsed.fs_version == 1
        assert [f.name for f in parsed.files] == [DEVICE_ID_FILENAME, "config", "wifi"]
        assert parsed.files[0].data == b"\x01" * 256
        assert parsed.files[1].data == b"abc"
        assert parsed.files[2].data == b"12345"
        assert parsed.header.board_serial == "SN-1"

    def test_fs_version_stamped_and_header_crc_valid(self):
        hdr = v4_header()
        assert hdr.fs_version == 0  # builder stamps, not the caller
        img = build_image(hdr, [("a", b"x")], 1024)
        assert img[10] == 1
        stored = struct.unpack("<I", img[252:256])[0]
        assert stored == binascii.crc32(img[:252]) & 0xFFFFFFFF
        assert hdr.fs_version == 0  # caller's object untouched

    def test_empty_file_list_is_formatted_empty_fs(self):
        img = build_image(v4_header(), [], 512)
        assert img[10] == 1  # a formatted image carries the (empty) filesystem
        parsed = parse_image(img)
        assert parsed.files == []
        assert set(img[256:]) == {0}  # wiped file area

    def test_chain_layout_is_contiguous_le(self):
        img = build_image(v4_header(), [("a", b"xy"), ("b", b"z")], 1024)
        # file a at 256: data 2, next = 256+28+2 = 286
        assert struct.unpack("<H", img[256 + 16 : 256 + 18])[0] == 2
        assert struct.unpack("<H", img[256 + 22 : 256 + 24])[0] == 286
        # headerCrc32 over bytes 0-23
        assert struct.unpack("<I", img[256 + 24 : 256 + 28])[0] == binascii.crc32(img[256 : 256 + 24]) & 0xFFFFFFFF
        # file b terminal
        assert struct.unpack("<H", img[286 + 22 : 286 + 24])[0] == 0

    def test_accepts_image_file_objects(self):
        img = build_image(v4_header(), [ImageFile("a", b"x")], 512)
        assert parse_image(img).files[0] == ImageFile("a", b"x")

    @pytest.mark.parametrize(
        "files,err",
        [
            ([("", b"x")], "name"),
            ([("abcdefghijklmnop", b"x")], "name"),
            ([("a", b"")], "data"),
            ([("a", b"x" * 32768)], "data"),
            ([("a", b"x"), ("a", b"y")], "duplicate"),
        ],
    )
    def test_rejects_invalid_files(self, files, err):
        with pytest.raises(ValueError, match=err):
            build_image(v4_header(), files, 2048)

    def test_rejects_when_no_capacity(self):
        with pytest.raises(ValueError, match="capacity"):
            build_image(v4_header(), [("a", b"x" * 300)], 512)
        with pytest.raises(ValueError, match="capacity"):
            build_image(v4_header(), [], 255)  # smaller than the header
        with pytest.raises(ValueError, match="image_size"):
            build_image(v4_header(), [], 65536)

    def test_v3_header_supported(self):
        img = build_image(EEPROMHeaderV3(boardname="b", serial="s"), [("f", b"d")], 512)
        parsed = parse_image(img)
        assert parsed.version == 3
        assert parsed.header.serial == "s"
        assert parsed.files[0].name == "f"


class TestParseImage:
    def test_fs_version_zero_reads_no_files(self):
        hdr_only = v4_header().to_partition_image()
        parsed = parse_image(hdr_only[:4096])
        assert parsed.fs_version == 0
        assert parsed.files == []

    def test_unknown_fs_version_rejected(self):
        img = bytearray(build_image(v4_header(), [("a", b"x")], 512))
        img[10] = 2
        img[252:256] = struct.pack("<I", binascii.crc32(bytes(img[:252])) & 0xFFFFFFFF)
        with pytest.raises(ValueError, match="fs_version"):
            parse_image(bytes(img))

    def test_no_header_rejected(self):
        with pytest.raises(ValueError, match="header"):
            parse_image(b"\x00" * 512)
        with pytest.raises(ValueError, match="header"):
            parse_image(b"\xff" * 512)

    def test_corrupted_file_header_crc_rejected(self):
        img = bytearray(build_image(v4_header(), [("a", b"x")], 512))
        img[256 + 1] ^= 0x40  # name byte, header CRC not resealed
        with pytest.raises(ValueError, match="header CRC"):
            parse_image(bytes(img))

    def test_corrupted_data_crc_reported_not_fatal(self):
        # C conformance: the chain walk never aborts on a payload problem —
        # ListFiles lists both files, ReadFile refuses only the bad one.
        img = bytearray(build_image(v4_header(), [("a", b"xyz"), ("b", b"ok")], 512))
        img[256 + FHDR] ^= 0xFF  # first data byte of "a"
        parsed = parse_image(bytes(img))
        assert [f.name for f in parsed.files] == ["a", "b"]
        assert parsed.unreadable == ["a"]

    def test_oversized_datasize_is_unreadable(self):
        # dataSize above INT16_MAX walks fine (chain rules allow it) but
        # C's EEPROM_ReadFile would refuse it — reported, not returned OK.
        img = bytearray(build_image(v4_header(), [("big", b"x" * 100)], 40000 + 284 + 100))
        img[256 + 16 : 256 + 18] = struct.pack("<H", 40000)
        img[256 + 22 : 256 + 24] = struct.pack("<H", 0)
        img[256 + 24 : 256 + 28] = struct.pack("<I", binascii.crc32(bytes(img[256 : 256 + 24])) & 0xFFFFFFFF)
        parsed = parse_image(bytes(img))
        assert parsed.unreadable == ["big"]

    def test_non_ascii_name_bytes_parse(self):
        # The wire format constrains only name length and the terminator,
        # not byte values (C's filename check is length-only) — a name
        # byte above 0x7F must parse, not crash.
        img = bytearray(build_image(v4_header(), [("a", b"x")], 512))
        img[256] = 0xE9
        img[256 + 24 : 256 + 28] = struct.pack("<I", binascii.crc32(bytes(img[256 : 256 + 24])) & 0xFFFFFFFF)
        parsed = parse_image(bytes(img))
        assert parsed.files[0].name == "\xe9"
        assert parsed.unreadable == []

    def test_broken_header_crc_is_a_broken_header(self):
        # parse_image hands out header field values, so the header must
        # prove itself: a CRC error means no fields are returned at all.
        img = bytearray(build_image(v4_header(), [("a", b"x")], 512))
        img[100] ^= 0x01  # identity byte: header CRC now stale
        with pytest.raises(ValueError, match="header CRC"):
            parse_image(bytes(img))

    def test_wide_char_name_rejected_on_build(self):
        with pytest.raises(ValueError, match="byte-range"):
            build_image(v4_header(), [("имя", b"x")], 512)

    def test_nul_in_name_rejected_on_build(self):
        with pytest.raises(ValueError, match="name"):
            build_image(v4_header(), [("a\x00b", b"x")], 512)

    def test_erased_tail_is_clean_end(self):
        img = bytearray(build_image(v4_header(), [("a", b"x")], 1024))
        img[256 + FHDR + 1 :] = b"\xff" * (1024 - 256 - FHDR - 1)  # erased free space
        parsed = parse_image(bytes(img))
        assert [f.name for f in parsed.files] == ["a"]


class TestGoldenCompatibility:
    """The committed goldens are the cross-language contract: the API must
    reproduce them byte-for-byte from their JSON descriptions."""

    @pytest.mark.parametrize("stem", ["eeprom_full", "eeprom_full_v4"])
    def test_build_matches_committed_golden(self, stem):
        spec = json.loads((REFERENCE / f"{stem}.json").read_text())
        h = spec["header"]
        cls = EEPROMHeaderV4 if h["version"] == 4 else EEPROMHeaderV3
        serial_key = "board_serial" if h["version"] == 4 else "serial"
        kwargs = {
            "boardname": h["boardname"],
            "boardversion": h["boardversion"],
            "usid": h["usid"],
            "cpuid": h["cpuid"],
            "mac": h["mac"],
            "timestamp": h.get("timestamp", 0),
        }
        kwargs[serial_key] = h[serial_key]
        header = cls(**kwargs)
        files = [(f["name"], bytes.fromhex(f["data_hex"])) for f in spec["files"]]
        img = build_image(header, files, spec["eeprom_size"])
        assert img == (REFERENCE / f"{stem}.bin").read_bytes()

    def test_parse_committed_golden(self):
        parsed = parse_image((REFERENCE / "eeprom_full_v4.bin").read_bytes())
        assert parsed.version == 4
        assert parsed.header.boardname == "JetHub-D1p"
        assert [f.name for f in parsed.files] == ["config", "wifi.conf", "serial"]
