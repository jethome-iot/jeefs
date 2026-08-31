# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""Tests for EEPROM header library (EEPROMHeaderV3).

Tests cover:
1. Serialization (to_bytes) for signature v0/v1/v2
2. Deserialization (from_bytes) roundtrip
3. CRC32 correctness
4. Field offsets match specification
5. Signature algorithm validation
6. MAC address parsing
7. Partition image creation
8. Error handling for invalid inputs
"""

import binascii
import struct

import pytest
from jeefs import (
    EEPROMHeaderV4,
    detect_version,
    EEPROM_CRC_COVERAGE,
    EEPROM_FIELDS,
    EEPROM_FIELDS_V3,
    EEPROM_HEADER_SIZE,
    EEPROM_MAGIC,
    EEPROM_PARTITION_SIZE,
    EEPROM_SIGNATURE_FIELD_SIZE,
    SIGNATURE_SIZES,
    EEPROMHeaderV3,
    SignatureAlgorithm,
    parse_mac_string,
)

# --- Fixtures ---


@pytest.fixture
def sample_data() -> dict:
    """Sample device data for header creation."""
    return {
        "boardname": "JXD-CPU-E1ETH",
        "boardversion": "1.3",
        "serial": "SN-2024-001",
        "usid": "1234567890ABCDEF",
        "cpuid": "AA:BB:CC:DD:EE:FF",
        "mac": "F0:57:8D:01:00:00",
    }


@pytest.fixture
def sig_48() -> bytes:
    """48-byte signature (secp192r1)."""
    return bytes(range(0xA0, 0xD0))  # 48 bytes


@pytest.fixture
def sig_64() -> bytes:
    """64-byte signature (secp256r1)."""
    return bytes(range(0x80, 0xC0))  # 64 bytes


# --- SignatureAlgorithm enum ---


class TestSignatureAlgorithm:
    """Tests for SignatureAlgorithm enum."""

    def test_values(self):
        assert SignatureAlgorithm.NONE == 0
        assert SignatureAlgorithm.SECP192R1 == 1
        assert SignatureAlgorithm.SECP256R1 == 2

    def test_from_int_valid(self):
        assert SignatureAlgorithm.from_int(0) == SignatureAlgorithm.NONE
        assert SignatureAlgorithm.from_int(1) == SignatureAlgorithm.SECP192R1
        assert SignatureAlgorithm.from_int(2) == SignatureAlgorithm.SECP256R1

    def test_from_int_invalid_raises(self):
        with pytest.raises(ValueError, match="Unknown value=3"):
            SignatureAlgorithm.from_int(3)

        with pytest.raises(ValueError, match="Unknown value=255"):
            SignatureAlgorithm.from_int(255)

        with pytest.raises(ValueError, match="Unknown value=-1"):
            SignatureAlgorithm.from_int(-1)

    def test_signature_sizes_complete(self):
        """Every enum member has a defined size."""
        for member in SignatureAlgorithm:
            assert member in SIGNATURE_SIZES

    def test_signature_sizes_values(self):
        assert SIGNATURE_SIZES[SignatureAlgorithm.NONE] == 0
        assert SIGNATURE_SIZES[SignatureAlgorithm.SECP192R1] == 48
        assert SIGNATURE_SIZES[SignatureAlgorithm.SECP256R1] == 64


# --- MAC parsing ---


class TestParseMacString:
    """Tests for parse_mac_string utility."""

    def test_colon_format(self):
        assert parse_mac_string("AA:BB:CC:DD:EE:FF") == bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])

    def test_dash_format(self):
        assert parse_mac_string("AA-BB-CC-DD-EE-FF") == bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])

    def test_no_separator(self):
        assert parse_mac_string("AABBCCDDEEFF") == bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])

    def test_lowercase(self):
        assert parse_mac_string("aa:bb:cc:dd:ee:ff") == bytes([0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF])

    def test_invalid_length_raises(self):
        with pytest.raises(ValueError, match="Invalid MAC address length"):
            parse_mac_string("AA:BB:CC")

    def test_invalid_hex_raises(self):
        with pytest.raises(ValueError, match="Invalid MAC address hex"):
            parse_mac_string("GG:HH:II:JJ:KK:LL")


# --- Header field offsets ---


class TestFieldOffsets:
    """Verify field offsets cover exactly 256 bytes with no gaps or overlaps."""

    def test_all_fields_defined(self):
        expected_fields = {
            "magic",
            "version",
            "signature_version",
            "fs_version",
            "header_reserved",
            "boardname",
            "boardversion",
            "serial",
            "usid",
            "cpuid",
            "mac",
            "reserved2",
            "signature",
            "timestamp",
            "crc32",
        }
        assert set(EEPROM_FIELDS_V3.keys()) == expected_fields

    def test_fs_version_round_trips(self):
        """Read-modify-write of a header must not zero the filesystem gate."""
        raw = bytearray(EEPROMHeaderV3(boardname="board").to_bytes())
        assert raw[10] == 0  # python-built headers carry no filesystem
        raw[10] = 1  # image whose file area holds a filesystem
        hdr = EEPROMHeaderV3.from_bytes(bytes(raw))
        assert hdr.fs_version == 1
        assert EEPROMHeaderV3.from_bytes(hdr.to_bytes()).fs_version == 1

    def test_offsets_cover_256_bytes(self):
        coverage = [False] * EEPROM_HEADER_SIZE
        for name, (offset, size) in EEPROM_FIELDS_V3.items():
            for i in range(offset, offset + size):
                assert not coverage[i], f"Overlap at byte {i} ({name})"
                coverage[i] = True
        assert all(coverage), "Not all 256 bytes covered"

    def test_specific_offsets(self):
        """Verify critical offsets match C struct layout."""
        assert EEPROM_FIELDS_V3["magic"] == (0, 8)
        assert EEPROM_FIELDS_V3["version"] == (8, 1)
        assert EEPROM_FIELDS_V3["signature_version"] == (9, 1)
        assert EEPROM_FIELDS_V3["boardname"] == (12, 32)
        assert EEPROM_FIELDS_V3["boardversion"] == (44, 32)
        assert EEPROM_FIELDS_V3["serial"] == (76, 32)
        assert EEPROM_FIELDS_V3["usid"] == (108, 32)
        assert EEPROM_FIELDS_V3["cpuid"] == (140, 32)
        assert EEPROM_FIELDS_V3["mac"] == (172, 6)
        assert EEPROM_FIELDS_V3["signature"] == (180, 64)
        assert EEPROM_FIELDS_V3["timestamp"] == (244, 8)
        assert EEPROM_FIELDS_V3["crc32"] == (252, 4)


# --- Header generation (to_bytes) ---


class TestHeaderToBytes:
    """Tests for EEPROMHeaderV3.to_bytes()."""

    def test_size_is_256(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        assert len(result) == EEPROM_HEADER_SIZE

    def test_magic(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        assert result[0:8] == EEPROM_MAGIC

    def test_version_is_3(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        assert result[8] == 3

    def test_boardname(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        off, sz = EEPROM_FIELDS_V3["boardname"]
        name = result[off : off + sz].split(b"\x00")[0].decode()
        assert name == "JXD-CPU-E1ETH"

    def test_mac_raw_bytes(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        assert result[172:178] == bytes.fromhex("F0578D010000")

    def test_crc32_correct(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        stored_crc = struct.unpack("<I", result[252:256])[0]
        expected_crc = binascii.crc32(result[:EEPROM_CRC_COVERAGE]) & 0xFFFFFFFF
        assert stored_crc == expected_crc

    def test_timestamp_unset_writes_zero(self, sample_data):
        # ts is written verbatim (#16): no silent "now" substitution
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        ts = struct.unpack("<q", result[244:252])[0]
        assert ts == 0

    def test_timestamp_zero_is_verbatim(self, sample_data):
        # ts=0 stays 0 on the wire: it is the placeholder state
        # (header_is_empty), never replaced with the current time
        header = EEPROMHeaderV3(**sample_data, timestamp=0)
        result = header.to_bytes()
        ts = struct.unpack("<q", result[244:252])[0]
        assert ts == 0

    def test_timestamp_explicit(self, sample_data):
        header = EEPROMHeaderV3(**sample_data, timestamp=1734264000)
        result = header.to_bytes()
        ts = struct.unpack("<q", result[244:252])[0]
        assert ts == 1734264000

    def test_reserved_areas_zeros(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        # header_reserved (10-11)
        assert result[10:12] == b"\x00\x00"
        # reserved2 (178-179)
        assert result[178:180] == b"\x00\x00"


# --- Signature version 0 (NONE) ---


class TestSignatureNone:
    """Tests for signature_algorithm=NONE (no signature)."""

    def test_signature_version_byte_is_0(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        assert result[9] == 0

    def test_signature_field_all_zeros(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        assert all(b == 0 for b in result[180:244])


# --- Signature version 1 (secp192r1, 48 bytes) ---


class TestSignatureSecp192r1:
    """Tests for signature_algorithm=SECP192R1."""

    def test_signature_version_byte_is_1(self, sample_data, sig_48):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
        )
        result = header.to_bytes()
        assert result[9] == 1

    def test_48_bytes_at_offset_180(self, sample_data, sig_48):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
        )
        result = header.to_bytes()
        assert result[180:228] == sig_48

    def test_remaining_16_bytes_zeros(self, sample_data, sig_48):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
        )
        result = header.to_bytes()
        assert all(b == 0 for b in result[228:244])

    def test_wrong_size_raises(self, sample_data, sig_64):
        """64-byte signature with SECP192R1 should fail validation."""
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_64,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
        )
        with pytest.raises(ValueError, match="Signature size mismatch"):
            header.to_bytes()

    def test_signature_in_crc(self, sample_data, sig_48):
        """Signature is included in CRC32 calculation."""
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
        )
        result = header.to_bytes()
        stored_crc = struct.unpack("<I", result[252:256])[0]
        expected_crc = binascii.crc32(result[:252]) & 0xFFFFFFFF
        assert stored_crc == expected_crc


# --- Signature version 2 (secp256r1, 64 bytes) ---


class TestSignatureSecp256r1:
    """Tests for signature_algorithm=SECP256R1."""

    def test_signature_version_byte_is_2(self, sample_data, sig_64):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_64,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
        )
        result = header.to_bytes()
        assert result[9] == 2

    def test_64_bytes_at_offset_180(self, sample_data, sig_64):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_64,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
        )
        result = header.to_bytes()
        assert result[180:244] == sig_64

    def test_full_signature_field_used(self, sample_data, sig_64):
        """With secp256r1, all 64 bytes of signature field are used."""
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_64,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
        )
        result = header.to_bytes()
        assert result[180:244] == sig_64
        assert len(sig_64) == EEPROM_SIGNATURE_FIELD_SIZE

    def test_wrong_size_48_raises(self, sample_data, sig_48):
        """48-byte signature with SECP256R1 should fail validation."""
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
        )
        with pytest.raises(ValueError, match="Signature size mismatch"):
            header.to_bytes()

    def test_signature_in_crc(self, sample_data, sig_64):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_64,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
        )
        result = header.to_bytes()
        stored_crc = struct.unpack("<I", result[252:256])[0]
        expected_crc = binascii.crc32(result[:252]) & 0xFFFFFFFF
        assert stored_crc == expected_crc


# --- Roundtrip (to_bytes -> from_bytes) ---


class TestRoundtrip:
    """Tests for to_bytes() -> from_bytes() roundtrip."""

    def test_roundtrip_no_signature(self, sample_data):
        original = EEPROMHeaderV3(**sample_data, timestamp=1734264000)
        binary = original.to_bytes()
        parsed = EEPROMHeaderV3.from_bytes(binary)

        assert parsed.boardname == original.boardname
        assert parsed.boardversion == original.boardversion
        assert parsed.serial == original.serial
        assert parsed.usid == original.usid
        assert parsed.cpuid == original.cpuid
        assert parsed.mac == original.mac
        assert parsed.signature_algorithm == SignatureAlgorithm.NONE
        assert parsed.signature == b""
        assert parsed.timestamp == 1734264000

    def test_roundtrip_secp192r1(self, sample_data, sig_48):
        original = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
            timestamp=1734264000,
        )
        binary = original.to_bytes()
        parsed = EEPROMHeaderV3.from_bytes(binary)

        assert parsed.signature == sig_48
        assert parsed.signature_algorithm == SignatureAlgorithm.SECP192R1
        assert parsed.boardname == original.boardname
        assert parsed.serial == original.serial

    def test_roundtrip_secp256r1(self, sample_data, sig_64):
        original = EEPROMHeaderV3(
            **sample_data,
            signature=sig_64,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
            timestamp=1734264000,
        )
        binary = original.to_bytes()
        parsed = EEPROMHeaderV3.from_bytes(binary)

        assert parsed.signature == sig_64
        assert parsed.signature_algorithm == SignatureAlgorithm.SECP256R1
        assert parsed.boardname == original.boardname

    def test_crc_valid_after_roundtrip(self, sample_data, sig_64):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_64,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
            timestamp=1734264000,
        )
        binary = header.to_bytes()
        assert EEPROMHeaderV3.verify_crc_static(binary)


# --- from_bytes error handling ---


class TestFromBytesErrors:
    """Tests for from_bytes() error handling."""

    def test_too_short_raises(self):
        with pytest.raises(ValueError, match="Data too short"):
            EEPROMHeaderV3.from_bytes(b"\x00" * 100)

    def test_invalid_magic_raises(self):
        data = bytearray(256)
        data[0:8] = b"INVALID\x00"
        with pytest.raises(ValueError, match="Invalid EEPROM magic"):
            EEPROMHeaderV3.from_bytes(bytes(data))

    def test_unsupported_version_raises(self):
        data = bytearray(256)
        data[0:8] = EEPROM_MAGIC
        data[8] = 99  # unsupported version
        with pytest.raises(ValueError, match="Unsupported EEPROM header version"):
            EEPROMHeaderV3.from_bytes(bytes(data))

    def test_unknown_signature_version_raises(self):
        data = bytearray(256)
        data[0:8] = EEPROM_MAGIC
        data[8] = 3
        data[9] = 99  # unknown signature version
        with pytest.raises(ValueError, match="Unknown value=99"):
            EEPROMHeaderV3.from_bytes(bytes(data))


# --- Validation ---


class TestValidation:
    """Tests for header validation."""

    def test_valid_header_no_errors(self, sample_data, sig_48):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
        )
        assert header.validate() == []

    def test_signature_size_mismatch(self, sample_data, sig_48):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,  # 48 bytes
            signature_algorithm=SignatureAlgorithm.SECP256R1,  # expects 64
        )
        errors = header.validate()
        assert any("Signature size mismatch" in e for e in errors)

    def test_signature_with_none_algorithm(self, sample_data, sig_48):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.NONE,
        )
        errors = header.validate()
        assert any("signature_algorithm is NONE" in e for e in errors)

    def test_invalid_mac_format(self, sample_data):
        header = EEPROMHeaderV3(**{**sample_data, "mac": "INVALID"})
        errors = header.validate()
        assert any("Invalid MAC" in e for e in errors)

    def test_unknown_signature_version_int(self, sample_data):
        """Integer not in enum raises during __post_init__."""
        with pytest.raises(ValueError, match="Unknown value=99"):
            EEPROMHeaderV3(**sample_data, signature_algorithm=99)

    def test_boardname_too_long(self, sample_data):
        header = EEPROMHeaderV3(**{**sample_data, "boardname": "A" * 40})
        errors = header.validate()
        assert any("boardname too long" in e for e in errors)


# --- Partition image ---


class TestPartitionImage:
    """Tests for partition image generation."""

    def test_partition_is_4kb(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        image = header.to_partition_image()
        assert len(image) == EEPROM_PARTITION_SIZE

    def test_header_at_offset_0(self, sample_data):
        header = EEPROMHeaderV3(**sample_data, timestamp=1734264000)
        image = header.to_partition_image()
        assert image[0:8] == EEPROM_MAGIC

    def test_padding_is_zeros(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        image = header.to_partition_image()
        assert all(b == 0 for b in image[EEPROM_HEADER_SIZE:])


# --- CRC verification ---


class TestCRCVerification:
    """Tests for CRC32 verification."""

    def test_verify_crc_valid(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        binary = header.to_bytes()
        assert EEPROMHeaderV3.verify_crc_static(binary) is True

    def test_verify_crc_corrupted(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        binary = bytearray(header.to_bytes())
        binary[100] ^= 0xFF  # corrupt a byte
        assert EEPROMHeaderV3.verify_crc_static(bytes(binary)) is False

    def test_verify_crc_too_short(self):
        assert EEPROMHeaderV3.verify_crc_static(b"\x00" * 100) is False

    def test_known_crc32_vector(self):
        """CRC32 of '123456789' = 0xCBF43926 (standard test vector)."""
        assert binascii.crc32(b"123456789") & 0xFFFFFFFF == 0xCBF43926


# --- to_dict / repr ---


class TestSerialization:
    """Tests for to_dict and __repr__."""

    def test_to_dict_fields(self, sample_data, sig_48):
        header = EEPROMHeaderV3(
            **sample_data,
            signature=sig_48,
            signature_algorithm=SignatureAlgorithm.SECP192R1,
        )
        d = header.to_dict()
        assert d["version"] == 3
        assert d["signature_version"] == 1
        assert d["signature_algorithm"] == "SECP192R1"
        assert d["boardname"] == "JXD-CPU-E1ETH"
        assert d["signature_present"] is True

    def test_repr(self, sample_data):
        header = EEPROMHeaderV3(**sample_data)
        r = repr(header)
        assert "EEPROMHeaderV3" in r
        assert "JXD-CPU-E1ETH" in r


class TestBoundedStrings:
    """RFC #13: serial/usid/cpuid are bounded strings — printable ASCII,
    all 32 bytes usable, NUL optional when the value fills the field."""

    def _mk(self, **kw):
        defaults = dict(boardname="JetHub-D1p", boardversion="1.0",
                        serial="SN-1", usid="U-1", cpuid="C-1")
        defaults.update(kw)
        return EEPROMHeaderV3(**defaults)

    def test_full_32_byte_serial_round_trips(self):
        full = "S" * 32
        hdr = self._mk(serial=full, usid="u" * 32, cpuid="c" * 32)
        raw = hdr.to_bytes()
        off, sz = EEPROM_FIELDS_V3["serial"]
        assert raw[off : off + sz] == b"S" * 32  # no NUL stolen
        back = EEPROMHeaderV3.from_bytes(raw)
        assert back.serial == full
        assert back.usid == "u" * 32
        assert back.cpuid == "c" * 32

    def test_short_serial_is_nul_terminated_and_padded(self):
        hdr = self._mk(serial="SN-42")
        raw = hdr.to_bytes()
        off, sz = EEPROM_FIELDS_V3["serial"]
        field = raw[off : off + sz]
        assert field[:5] == b"SN-42"
        assert field[5:] == b"\x00" * (sz - 5)

    def test_validate_allows_32_rejects_33(self):
        assert self._mk(serial="S" * 32).validate() == []
        errs = self._mk(serial="S" * 33).validate()
        assert any("serial" in e for e in errs)

    def test_boardname_stays_nul_mandatory(self):
        errs = self._mk(boardname="B" * 32).validate()
        assert any("boardname" in e for e in errs)
        assert self._mk(boardname="B" * 31).validate() == []


class TestHeaderV4:
    """Header v4: layout identical to v3, board-scoped semantics,
    the serial slot is named board_serial (RFC #56)."""

    def _mk(self, **kw):
        defaults = dict(boardname="JetHub-D2", boardversion="2.1",
                        board_serial="BS-0001", usid="U", cpuid="C")
        defaults.update(kw)
        return EEPROMHeaderV4(**defaults)

    def test_version_byte_and_layout(self):
        raw = self._mk().to_bytes()
        assert raw[8] == 4
        off, sz = EEPROM_FIELDS["board_serial"]
        assert raw[off : off + sz].split(b"\x00")[0] == b"BS-0001"
        assert (off, sz) == EEPROM_FIELDS_V3["serial"]  # same slot as v3

    def test_round_trip(self):
        hdr = self._mk(board_serial="S" * 32)
        back = EEPROMHeaderV4.from_bytes(hdr.to_bytes())
        assert back.board_serial == "S" * 32
        assert back.VERSION == 4
        assert back.verify_crc(hdr.to_bytes())

    def test_rejects_v3_bytes(self):
        v3 = EEPROMHeaderV3(boardname="b", boardversion="1",
                            serial="s", usid="u", cpuid="c")
        with pytest.raises(ValueError, match="version"):
            EEPROMHeaderV4.from_bytes(v3.to_bytes())

    def test_to_dict_uses_board_serial(self):
        d = self._mk().to_dict()
        assert d["version"] == 4
        assert d["board_serial"] == "BS-0001"
        assert "serial" not in d

    def test_repr_names_the_class_and_slot(self):
        r = repr(self._mk())
        assert r.startswith("EEPROMHeaderV4(")
        assert "board_serial=BS-0001" in r


class TestDetectVersion:
    """Module-level detect_version — the cross-version entry point."""

    def test_detects_all_versions(self):
        v3 = EEPROMHeaderV3(boardname="b", boardversion="1",
                            serial="s", usid="u", cpuid="c")
        assert detect_version(v3.to_bytes()) == 3
        v4 = EEPROMHeaderV4(boardname="b", boardversion="1",
                            board_serial="s", usid="u", cpuid="c")
        assert detect_version(v4.to_bytes()) == 4

    def test_erased_and_garbage_are_none(self):
        assert detect_version(b"\x00" * 512) is None
        assert detect_version(b"\xff" * 512) is None
        assert detect_version(b"JETHOME\x00\x07" + b"\x00" * 500) is None  # unknown version
        assert detect_version(b"JETHOME") is None  # too short

    def test_committed_vectors(self):
        from pathlib import Path

        vectors = Path(__file__).resolve().parents[2] / "test-vectors" / "vectors"
        for stem, expected in (("v1_header_minimal", 1), ("v2_header_minimal", 2),
                               ("v3_header_nosig", 3), ("v4_header_minimal", 4)):
            raw = (vectors / f"{stem}.bin").read_bytes()
            assert detect_version(raw) == expected, stem


class TestTimestampRoundTrip:
    def test_zero_timestamp_stays_zero(self):
        # Regression: to_bytes silently replaced ts==0 with current time —
        # round-trip unstable and ts=0 unwritable (#16).
        hdr = EEPROMHeaderV3(boardname="b", boardversion="1",
                             serial="s", usid="u", cpuid="c", timestamp=0)
        raw = hdr.to_bytes()
        assert raw[244:252] == b"\x00" * 8
        assert hdr.to_bytes() == raw  # stable across calls

    def test_twelve_byte_probe_suffices(self):
        # C/Rust detect only need the 12-byte version-detect struct
        assert detect_version(b"JETHOME\x00\x03\x00\x00\x00") == 3
        assert detect_version(b"JETHOME\x00\x04\x00\x00\x00") == 4


class TestHeaderIsEmpty:
    """A placeholder (claimed) header is distinguishable by content."""

    def test_no_header(self):
        from jeefs import header_is_empty

        assert header_is_empty(b"\x00" * 256) is None
        assert header_is_empty(b"\xff" * 256) is None
        assert header_is_empty(b"junk") is None

    def test_empty_and_populated(self):
        from jeefs import header_is_empty
        from jeefs.header import EEPROMHeaderV4

        empty = bytearray(256)
        empty[0:8] = b"JETHOME\x00"
        empty[8] = 4
        assert header_is_empty(bytes(empty)) is True
        empty[10] = 1  # fs_version is prologue, not identity
        assert header_is_empty(bytes(empty)) is True

        hdr = EEPROMHeaderV4(boardname="board", boardversion="1.0")
        assert header_is_empty(hdr.to_bytes()) is False
