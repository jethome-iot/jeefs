# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
# Copyright (c) 2026 JetHome. All rights reserved.
"""DeviceIdentityV1 record — device-scoped identity (RFC #26).

A self-contained 256-byte signed blob stored as the JEEFS file
``device.id``. The tail layout (signature/timestamp/crc32) is
byte-identical to header v3; the record carries its own magic
("JHDEVID\\0") and record_version.
"""

from __future__ import annotations

import binascii
import struct
from dataclasses import dataclass, field

from .constants_generated import (
    EEPROM_DEVID_MAGIC,
    EEPROM_FIELDS_DEVICEIDENTITYV1,
    SIGNATURE_SIZES,
    SignatureAlgorithm,
)
from .header import _pack_bounded, _unpack_string

RECORD_SIZE = 256
RECORD_VERSION = 1
_CRC_COVERAGE = 252
_FIELDS = EEPROM_FIELDS_DEVICEIDENTITYV1


@dataclass
class DeviceIdentityV1:
    """Device identity record: model, serial and hardware revision.

    All three string fields are bounded (printable ASCII, every byte of
    the field usable, NUL optional at full length). Fields derivable
    from the factory database by device_serial are deliberately absent.
    """

    device_model: str = ""
    device_serial: str = ""
    hw_revision: str = ""
    signature: bytes = b""
    signature_algorithm: SignatureAlgorithm = SignatureAlgorithm.NONE
    timestamp: int = 0

    RECORD_VERSION = RECORD_VERSION

    def __post_init__(self) -> None:
        if not isinstance(self.signature_algorithm, SignatureAlgorithm):
            self.signature_algorithm = SignatureAlgorithm.from_int(self.signature_algorithm)

    @staticmethod
    def crc32_of(data: bytes) -> int:
        """CRC32 over the covered bytes (0-251), IEEE 802.3."""
        return binascii.crc32(data[:_CRC_COVERAGE]) & 0xFFFFFFFF

    def validate(self) -> list[str]:
        """Return a list of error messages; empty means valid."""
        errors: list[str] = []
        for name in ("device_model", "device_serial", "hw_revision"):
            value = getattr(self, name)
            limit = _FIELDS[name][1]
            if value and len(value.encode("utf-8")) > limit:
                errors.append(f"{name} too long: {len(value.encode('utf-8'))} bytes (max {limit})")
        expected = SIGNATURE_SIZES.get(self.signature_algorithm, 0)
        if self.signature_algorithm != SignatureAlgorithm.NONE:
            if len(self.signature) != expected:
                errors.append(
                    f"signature size mismatch: {self.signature_algorithm.name} "
                    f"expects {expected} bytes, got {len(self.signature)}"
                )
        elif self.signature:
            errors.append("signature provided but signature_algorithm is NONE")
        return errors

    def to_bytes(self) -> bytes:
        """Serialize into the 256-byte record (reserved space zeroed)."""
        rec = bytearray(RECORD_SIZE)
        rec[0:8] = EEPROM_DEVID_MAGIC
        rec[8] = self.RECORD_VERSION
        rec[9] = int(self.signature_algorithm)

        for name in ("device_model", "device_serial", "hw_revision"):
            off, sz = _FIELDS[name]
            rec[off : off + sz] = _pack_bounded(getattr(self, name), sz)

        if self.signature:
            off = _FIELDS["signature"][0]
            rec[off : off + len(self.signature)] = self.signature

        struct.pack_into("<q", rec, _FIELDS["timestamp"][0], self.timestamp)

        crc = self.crc32_of(bytes(rec))
        struct.pack_into("<I", rec, _FIELDS["crc32"][0], crc)
        return bytes(rec)

    @classmethod
    def from_bytes(cls, data: bytes) -> DeviceIdentityV1:
        """Parse a 256-byte record; strict version gates per the spec."""
        if len(data) < RECORD_SIZE:
            raise ValueError(f"Data too short: {len(data)} bytes (need {RECORD_SIZE})")
        if data[0:8] != EEPROM_DEVID_MAGIC:
            raise ValueError(f"Invalid record magic: {data[0:8]!r}")
        if data[8] != cls.RECORD_VERSION:
            raise ValueError(f"Unknown record_version: {data[8]}")
        try:
            sig_algo = SignatureAlgorithm.from_int(data[9])
        except ValueError as e:
            raise ValueError(f"Unknown signature_version: {data[9]}") from e

        def get_str(name: str) -> str:
            off, sz = _FIELDS[name]
            return _unpack_string(data[off : off + sz])

        sig_size = SIGNATURE_SIZES.get(sig_algo, 0)
        sig_off = _FIELDS["signature"][0]
        signature = bytes(data[sig_off : sig_off + sig_size]) if sig_size else b""
        timestamp = struct.unpack_from("<q", data, _FIELDS["timestamp"][0])[0]

        return cls(
            device_model=get_str("device_model"),
            device_serial=get_str("device_serial"),
            hw_revision=get_str("hw_revision"),
            signature=signature,
            signature_algorithm=sig_algo,
            timestamp=timestamp,
        )

    def verify_crc(self, data: bytes) -> bool:
        """Check the stored CRC32 of a raw record buffer."""
        if len(data) < RECORD_SIZE:
            return False
        stored = struct.unpack_from("<I", data, _FIELDS["crc32"][0])[0]
        return stored == self.crc32_of(data)
