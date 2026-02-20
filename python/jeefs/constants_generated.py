"""EEPROM format constants — auto-generated from docs/format/*.md.

DO NOT EDIT — regenerate with:
    python -m jeefs_codegen --specs docs/format/*.md --py-output python/jeefs/constants_generated.py
"""

from __future__ import annotations

from enum import IntEnum


class SignatureAlgorithm(IntEnum):
    """JEEFSSignatureAlgorithm values.

    Values:
        NONE: No signature (0)
        SECP192R1: ECDSA secp192r1/NIST P-192, r‖s (1)
        SECP256R1: ECDSA secp256r1/NIST P-256, r‖s (2)
    """

    NONE = 0
    SECP192R1 = 1
    SECP256R1 = 2

    @classmethod
    def from_int(cls, value: int) -> "SignatureAlgorithm":
        """Convert integer to enum, raising ValueError for unknown."""
        try:
            return cls(value)
        except ValueError:
            valid = ", ".join(f"{m.value}={m.name}" for m in cls)
            raise ValueError(
                f"Unknown value={value}. Valid values: {valid}"
            ) from None

SIGNATURE_SIZES: dict[SignatureAlgorithm, int] = {
    SignatureAlgorithm.NONE: 0,
    SignatureAlgorithm.SECP192R1: 48,
    SignatureAlgorithm.SECP256R1: 64,
}
"""Signature byte size per algorithm (raw r||s format)."""

# --- Named constants ---

EEPROM_MAGIC = b"JETHOME\x00"
EEPROM_MAGIC_LENGTH = 8
EEPROM_HEADER_VERSION = 3
EEPROM_SIGNATURE_FIELD_SIZE = 64
EEPROM_FILE_NAME_LENGTH = 15
EEPROM_MAC_LENGTH = 6
EEPROM_SERIAL_LENGTH = 32
EEPROM_USID_LENGTH = 32
EEPROM_CPUID_LENGTH = 32
EEPROM_BOARDNAME_LENGTH = 31
EEPROM_BOARDVERSION_LENGTH = 31
EEPROM_EMPTYBYTE = 0x00
EEPROM_PARTITION_SIZE = 4096

# Field offsets and sizes for JEEPROMHeaderv3
EEPROM_FIELDS: dict[str, tuple[int, int]] = {
    "magic": (0, 8),
    "version": (8, 1),
    "signature_version": (9, 1),
    "header_reserved": (10, 2),
    "boardname": (12, 32),
    "boardversion": (44, 32),
    "serial": (76, 32),
    "usid": (108, 32),
    "cpuid": (140, 32),
    "mac": (172, 6),
    "reserved2": (178, 2),
    "signature": (180, 64),
    "timestamp": (244, 8),
    "crc32": (252, 4),
}
EEPROM_HEADER_SIZE = 256
EEPROM_CRC_COVERAGE = 252

# Field offsets and sizes for JEEPROMHeaderv2
EEPROM_FIELDS_V2: dict[str, tuple[int, int]] = {
    "magic": (0, 8),
    "version": (8, 1),
    "reserved1": (9, 3),
    "boardname": (12, 32),
    "boardversion": (44, 32),
    "serial": (76, 32),
    "usid": (108, 32),
    "cpuid": (140, 32),
    "mac": (172, 6),
    "reserved2": (178, 2),
    "reserved3": (180, 72),
    "crc32": (252, 4),
}
EEPROM_V2_HEADER_SIZE = 256
EEPROM_V2_CRC_COVERAGE = 252

# Field offsets and sizes for JEEPROMHeaderv1
EEPROM_FIELDS_V1: dict[str, tuple[int, int]] = {
    "magic": (0, 8),
    "version": (8, 1),
    "reserved1": (9, 3),
    "boardname": (12, 32),
    "boardversion": (44, 32),
    "serial": (76, 32),
    "usid": (108, 32),
    "cpuid": (140, 32),
    "mac": (172, 6),
    "reserved2": (178, 2),
    "modules": (180, 32),
    "reserved3": (212, 296),
    "crc32": (508, 4),
}
EEPROM_V1_HEADER_SIZE = 512
EEPROM_V1_CRC_COVERAGE = 508

# Field offsets and sizes for JEEPROMHeaderversion
EEPROM_FIELDS_JEEPROMHEADERVERSION: dict[str, tuple[int, int]] = {
    "magic": (0, 8),
    "version": (8, 1),
    "reserved1": (9, 3),
}

# Field offsets and sizes for JEEFSFileHeaderv1
EEPROM_FIELDS_JEEFSFILEHEADERV1: dict[str, tuple[int, int]] = {
    "name": (0, 16),
    "dataSize": (16, 2),
    "crc32": (18, 4),
    "nextFileAddress": (22, 2),
}
