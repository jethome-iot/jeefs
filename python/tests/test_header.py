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
import time

import pytest
from jeefs import (
    EEPROM_CRC_COVERAGE,
    EEPROM_FIELDS,
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
        assert set(EEPROM_FIELDS.keys()) == expected_fields

    def test_offsets_cover_256_bytes(self):
        coverage = [False] * EEPROM_HEADER_SIZE
        for name, (offset, size) in EEPROM_FIELDS.items():
            for i in range(offset, offset + size):
                assert not coverage[i], f"Overlap at byte {i} ({name})"
                coverage[i] = True
        assert all(coverage), "Not all 256 bytes covered"

    def test_specific_offsets(self):
        """Verify critical offsets match C struct layout."""
        assert EEPROM_FIELDS["magic"] == (0, 8)
        assert EEPROM_FIELDS["version"] == (8, 1)
        assert EEPROM_FIELDS["signature_version"] == (9, 1)
        assert EEPROM_FIELDS["boardname"] == (12, 32)
        assert EEPROM_FIELDS["boardversion"] == (44, 32)
        assert EEPROM_FIELDS["serial"] == (76, 32)
        assert EEPROM_FIELDS["usid"] == (108, 32)
        assert EEPROM_FIELDS["cpuid"] == (140, 32)
        assert EEPROM_FIELDS["mac"] == (172, 6)
        assert EEPROM_FIELDS["signature"] == (180, 64)
        assert EEPROM_FIELDS["timestamp"] == (244, 8)
        assert EEPROM_FIELDS["crc32"] == (252, 4)


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
        off, sz = EEPROM_FIELDS["boardname"]
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

    def test_timestamp_default_current_time(self, sample_data):
        before = int(time.time())
        header = EEPROMHeaderV3(**sample_data)
        result = header.to_bytes()
        after = int(time.time())

        ts = struct.unpack("<q", result[244:252])[0]
        assert before <= ts <= after

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
