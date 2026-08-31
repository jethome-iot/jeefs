# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""EEPROM header generation, parsing, and validation.

Provides a single source of truth for JEEPROMHeaderv3 binary format.
"""

from .constants import (
    EEPROM_CRC_COVERAGE,
    EEPROM_FIELDS,
    EEPROM_FIELDS_V3,
    EEPROM_HEADER_SIZE,
    EEPROM_HEADER_VERSION,
    EEPROM_MAGIC,
    EEPROM_PARTITION_SIZE,
    EEPROM_SIGNATURE_FIELD_SIZE,
    SIGNATURE_SIZES,
    SignatureAlgorithm,
)
from .devid import DeviceIdentityV1
from .header import EEPROMHeaderV3, EEPROMHeaderV4, detect_version, header_is_empty, parse_mac_string
from .image import DEVICE_ID_FILENAME, ImageFile, ParsedImage, build_image, parse_image

__all__ = [
    "DEVICE_ID_FILENAME",
    "ImageFile",
    "ParsedImage",
    "build_image",
    "parse_image",
    "DeviceIdentityV1",
    "EEPROMHeaderV3",
    "detect_version",
    "header_is_empty",
    "EEPROMHeaderV4",
    "SignatureAlgorithm",
    "SIGNATURE_SIZES",
    "EEPROM_FIELDS",
    "EEPROM_FIELDS_V3",
    "EEPROM_HEADER_SIZE",
    "EEPROM_HEADER_VERSION",
    "EEPROM_MAGIC",
    "EEPROM_CRC_COVERAGE",
    "EEPROM_SIGNATURE_FIELD_SIZE",
    "EEPROM_PARTITION_SIZE",
    "parse_mac_string",
]
