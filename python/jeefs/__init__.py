"""EEPROM header generation, parsing, and validation.

Provides a single source of truth for JEEPROMHeaderv3 binary format.
"""

from .constants import (
    EEPROM_CRC_COVERAGE,
    EEPROM_FIELDS,
    EEPROM_HEADER_SIZE,
    EEPROM_HEADER_VERSION,
    EEPROM_MAGIC,
    EEPROM_PARTITION_SIZE,
    EEPROM_SIGNATURE_FIELD_SIZE,
    SIGNATURE_SIZES,
    SignatureAlgorithm,
)
from .header import EEPROMHeaderV3, parse_mac_string

__all__ = [
    "EEPROMHeaderV3",
    "SignatureAlgorithm",
    "SIGNATURE_SIZES",
    "EEPROM_FIELDS",
    "EEPROM_HEADER_SIZE",
    "EEPROM_HEADER_VERSION",
    "EEPROM_MAGIC",
    "EEPROM_CRC_COVERAGE",
    "EEPROM_SIGNATURE_FIELD_SIZE",
    "EEPROM_PARTITION_SIZE",
    "parse_mac_string",
]
