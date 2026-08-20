# SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
"""EEPROM Header V3 class -- generation, parsing, and validation.

Implements JEEPROMHeaderv3 (256-byte, v2-based layout with signature).
Single source of truth for EEPROM binary format.
"""

from __future__ import annotations

import binascii
import logging
import struct
from dataclasses import dataclass, field

from .constants import (
    EEPROM_CRC_COVERAGE,
    EEPROM_FIELDS_V3,
    EEPROM_HEADER_SIZE,
    EEPROM_HEADER_VERSION,
    EEPROM_MAGIC,
    EEPROM_PARTITION_SIZE,
    SIGNATURE_SIZES,
    SignatureAlgorithm,
)

logger = logging.getLogger(__name__)


def parse_mac_string(mac_str: str) -> bytes:
    """Parse MAC address string to 6 raw bytes.

    Supports formats: "AA:BB:CC:DD:EE:FF", "AA-BB-CC-DD-EE-FF", "AABBCCDDEEFF".

    Args:
        mac_str: MAC address string.

    Returns:
        6-byte MAC address.

    Raises:
        ValueError: If MAC string is invalid.
    """
    mac_hex = mac_str.replace(":", "").replace("-", "")
    if len(mac_hex) != 12:
        raise ValueError(
            f"Invalid MAC address length: {mac_str!r} "
            f"(expected 12 hex chars, got {len(mac_hex)})"
        )
    try:
        return bytes.fromhex(mac_hex)
    except ValueError:
        raise ValueError(f"Invalid MAC address hex: {mac_str!r}") from None


def _pack_string(value: str, size: int) -> bytes:
    """Encode string to fixed-size null-terminated bytes.

    Args:
        value: String to encode.
        size: Field size in bytes (including null terminator space).

    Returns:
        Bytes of exactly `size` length, null-padded.
    """
    encoded = value.encode("utf-8")[: size - 1]
    return encoded + b"\x00" * (size - len(encoded))


def _pack_bounded(value: str, size: int) -> bytes:
    """Encode a bounded string (RFC #13): all `size` bytes are usable.

    The NUL terminator is optional — present (followed by zero padding)
    only when the value is shorter than the field. Content validation
    (printable ASCII) is the producer's concern, not the library's.

    Args:
        value: String to encode.
        size: Field size in bytes.

    Returns:
        Bytes of exactly `size` length.
    """
    encoded = value.encode("utf-8")[:size]
    return encoded + b"\x00" * (size - len(encoded))


def _unpack_string(data: bytes) -> str:
    """Decode null-terminated string from bytes.

    Args:
        data: Raw bytes (may contain trailing nulls).

    Returns:
        Decoded string (up to first null byte).
    """
    return data.split(b"\x00")[0].decode("utf-8", errors="replace")


_HEADER_SIZES = {1: 512, 2: 256, 3: 256, 4: 256}


def detect_version(data: bytes) -> int | None:
    """Detect the header version of a raw buffer.

    Mirrors the C/Rust detect functions: a 12-byte probe (magic plus a
    known version byte) is enough — full-header validation is the
    parser's job. Returns the version (1-4) or None: an erased or
    foreign buffer is "no header", not an error.
    """
    if len(data) < 12 or data[0:8] != EEPROM_MAGIC:
        return None
    version = data[8]
    return version if version in _HEADER_SIZES else None



def header_is_empty(data: bytes) -> bool | None:
    """Distinguish a placeholder header from a provisioned one.

    True when a header is detected and every identity byte (offset 12 up
    to the CRC field) is zero — the empty header a first-write claim
    leaves behind; False when detected and populated; None when no valid
    header is detected. Emptiness is independent of CRC validity.
    """
    version = detect_version(data)
    if version is None:
        return None
    size = _HEADER_SIZES.get(version)
    if size is None or len(data) < size:
        return None
    return not any(data[12 : size - 4])


@dataclass
class EEPROMHeaderV3:
    """JEEPROMHeaderv3 -- 256-byte EEPROM header (v2-based layout with signature).

    Supports EEPROM header version 3 only. Future versions (1, 2) can be
    added as separate classes with a common protocol/ABC if needed.

    Signature handling:
        - signature_algorithm=NONE (0): no signature, signature field is zeros
        - signature_algorithm=SECP192R1 (1): 48-byte ECDSA secp192r1 r||s
        - signature_algorithm=SECP256R1 (2): 64-byte ECDSA secp256r1 r||s

    Usage:
        # Generate EEPROM binary
        header = EEPROMHeaderV3(
            boardname="JXD-CPU-E1ETH",
            boardversion="1.3",
            serial="SN-2024-001",
            usid="1234567890ABCDEF",
            cpuid="AA:BB:CC:DD:EE:FF",
            mac="F0:57:8D:01:00:00",
            signature=sig_bytes,
            signature_algorithm=SignatureAlgorithm.SECP256R1,
        )
        binary = header.to_bytes()

        # Parse EEPROM binary
        header = EEPROMHeaderV3.from_bytes(raw_data)
    """

    # Pinned literal: this class parses/writes header v3 regardless of the
    # repo-wide current version (EEPROM_HEADER_VERSION is 4 since RFC #56)
    VERSION = 3

    # Device identity fields
    boardname: str = ""
    boardversion: str = ""
    serial: str = ""
    usid: str = ""
    cpuid: str = ""
    mac: str = ""  # String format "AA:BB:CC:DD:EE:FF" or "AABBCCDDEEFF"

    # Signature
    signature: bytes = field(default_factory=bytes)
    signature_algorithm: SignatureAlgorithm = SignatureAlgorithm.NONE

    # Timestamp (Unix epoch, written verbatim; 0/None serialize as 0)
    timestamp: int | None = None

    # Filesystem version byte (offset 10): 0 = no filesystem, 1 = the
    # current layout. The Python port does not touch the file area, so the
    # value round-trips verbatim — a read-modify-write of a header must not
    # make an existing filesystem invisible by zeroing the gate.
    fs_version: int = 0

    def __post_init__(self) -> None:
        """Validate signature_algorithm type after init."""
        if not isinstance(self.signature_algorithm, SignatureAlgorithm):
            self.signature_algorithm = SignatureAlgorithm.from_int(self.signature_algorithm)

    def validate(self) -> list[str]:
        """Validate header fields, return list of error messages.

        Returns:
            List of error strings. Empty list means valid.
        """
        errors: list[str] = []

        # Validate signature algorithm
        try:
            SignatureAlgorithm.from_int(int(self.signature_algorithm))
        except ValueError as e:
            errors.append(str(e))

        # Validate signature size matches algorithm
        expected_size = SIGNATURE_SIZES.get(self.signature_algorithm, 0)
        if self.signature_algorithm != SignatureAlgorithm.NONE:
            if len(self.signature) != expected_size:
                errors.append(
                    f"Signature size mismatch: "
                    f"algorithm {self.signature_algorithm.name} expects "
                    f"{expected_size} bytes, got {len(self.signature)}"
                )
        elif self.signature and len(self.signature) > 0:
            errors.append("Signature provided but signature_algorithm is NONE")

        # String field lengths: boardname/boardversion are NUL-terminated
        # (31 usable bytes); serial/usid/cpuid are bounded strings (RFC #13,
        # all 32 bytes usable, NUL optional at full length)
        for field_name, max_bytes in [
            ("boardname", 31),
            ("boardversion", 31),
            ("serial", 32),
            ("usid", 32),
            ("cpuid", 32),
        ]:
            value = getattr(self, field_name)
            if value and len(value.encode("utf-8")) > max_bytes:
                errors.append(
                    f"{field_name} too long: "
                    f"{len(value.encode('utf-8'))} bytes "
                    f"(max {max_bytes})"
                )

        # Validate MAC format
        if self.mac:
            try:
                parse_mac_string(self.mac)
            except ValueError as e:
                errors.append(str(e))

        return errors

    def to_bytes(self) -> bytes:
        """Serialize header to 256-byte binary (JEEPROMHeaderv3 format).

        Calculates CRC32 over bytes 0-251 and appends at offset 252.
        If timestamp is None or 0, uses current Unix time.

        Returns:
            256-byte EEPROM header binary.

        Raises:
            ValueError: If signature size doesn't match algorithm,
                or signature_algorithm is unknown.
        """
        errors = self.validate()
        if errors:
            raise ValueError(f"EEPROM header validation failed: {'; '.join(errors)}")

        eeprom = bytearray(EEPROM_HEADER_SIZE)

        # Magic (offset 0, 8 bytes)
        eeprom[0:8] = EEPROM_MAGIC

        # Version (offset 8, 1 byte)
        eeprom[8] = self.VERSION

        # Signature algorithm version (offset 9, 1 byte)
        eeprom[9] = int(self.signature_algorithm)

        # Filesystem version (offset 10): written verbatim, see the field
        # docstring. header_reserved (offset 11) stays zero from init.
        eeprom[EEPROM_FIELDS_V3["fs_version"][0]] = self.fs_version

        # String fields
        off, sz = EEPROM_FIELDS_V3["boardname"]
        eeprom[off : off + sz] = _pack_string(self.boardname, sz)

        off, sz = EEPROM_FIELDS_V3["boardversion"]
        eeprom[off : off + sz] = _pack_string(self.boardversion, sz)

        off, sz = EEPROM_FIELDS_V3["serial"]
        eeprom[off : off + sz] = _pack_bounded(self.serial, sz)

        off, sz = EEPROM_FIELDS_V3["usid"]
        eeprom[off : off + sz] = _pack_bounded(self.usid, sz)

        off, sz = EEPROM_FIELDS_V3["cpuid"]
        eeprom[off : off + sz] = _pack_bounded(self.cpuid, sz)

        # MAC: 6 raw bytes (offset 172)
        if self.mac:
            mac_bytes = parse_mac_string(self.mac)
            off, sz = EEPROM_FIELDS_V3["mac"]
            eeprom[off : off + sz] = mac_bytes

        # reserved2 (offset 178-179): zeros from init

        # Signature (offset 180, 64 bytes total field)
        if self.signature and self.signature_algorithm != SignatureAlgorithm.NONE:
            expected_size = SIGNATURE_SIZES[self.signature_algorithm]
            sig_data = self.signature[:expected_size]
            off = EEPROM_FIELDS_V3["signature"][0]
            eeprom[off : off + len(sig_data)] = sig_data

        # Timestamp (offset 244, int64 little-endian). Written verbatim:
        # replacing 0 with "now" made round-trips unstable and ts=0
        # unwritable (#16); producers set the signing time explicitly.
        struct.pack_into("<q", eeprom, EEPROM_FIELDS_V3["timestamp"][0], self.timestamp or 0)

        # CRC32 (offset 252, uint32 little-endian, covers bytes 0-251)
        crc32_value = binascii.crc32(bytes(eeprom[:EEPROM_CRC_COVERAGE])) & 0xFFFFFFFF
        struct.pack_into("<I", eeprom, EEPROM_FIELDS_V3["crc32"][0], crc32_value)

        return bytes(eeprom)

    @classmethod
    def from_bytes(cls, data: bytes) -> EEPROMHeaderV3:
        """Parse 256-byte binary into EEPROMHeaderV3 instance.

        Args:
            data: Raw binary data (at least 256 bytes).

        Returns:
            Parsed EEPROMHeaderV3 instance.

        Raises:
            ValueError: If data is too short, magic is invalid, or version != 3.
        """
        if len(data) < EEPROM_HEADER_SIZE:
            raise ValueError(f"Data too short: {len(data)} bytes " f"(need {EEPROM_HEADER_SIZE})")

        # Verify magic
        off, sz = EEPROM_FIELDS_V3["magic"]
        magic = data[off : off + sz]
        if magic != EEPROM_MAGIC:
            raise ValueError(f"Invalid EEPROM magic: {magic!r} (expected {EEPROM_MAGIC!r})")

        # Verify version
        version = data[EEPROM_FIELDS_V3["version"][0]]
        if version != cls.VERSION:
            raise ValueError(
                f"Unsupported EEPROM header version: {version} "
                f"(only version {cls.VERSION} is supported)"
            )

        # Parse signature algorithm
        sig_ver_byte = data[EEPROM_FIELDS_V3["signature_version"][0]]
        sig_algo = SignatureAlgorithm.from_int(sig_ver_byte)

        # Parse string fields
        def get_str(field_name: str) -> str:
            off, sz = EEPROM_FIELDS_V3[field_name]
            return _unpack_string(data[off : off + sz])

        # Parse MAC (6 raw bytes)
        off, sz = EEPROM_FIELDS_V3["mac"]
        mac_bytes = data[off : off + sz]
        mac_str = ":".join(f"{b:02X}" for b in mac_bytes)

        # Parse signature based on algorithm
        sig_off = EEPROM_FIELDS_V3["signature"][0]
        sig_size = SIGNATURE_SIZES.get(sig_algo, 0)
        signature = bytes(data[sig_off : sig_off + sig_size]) if sig_size > 0 else b""

        # Parse timestamp (int64 little-endian)
        ts_off = EEPROM_FIELDS_V3["timestamp"][0]
        timestamp = struct.unpack("<q", data[ts_off : ts_off + 8])[0]

        return cls(
            boardname=get_str("boardname"),
            boardversion=get_str("boardversion"),
            serial=get_str("serial"),
            usid=get_str("usid"),
            cpuid=get_str("cpuid"),
            mac=mac_str,
            signature=signature,
            signature_algorithm=sig_algo,
            timestamp=timestamp,
            fs_version=data[EEPROM_FIELDS_V3["fs_version"][0]],
        )

    def verify_crc(self, data: bytes) -> bool:
        """Verify CRC32 of raw binary data.

        Args:
            data: Raw 256-byte EEPROM header binary.

        Returns:
            True if stored CRC32 matches calculated CRC32.
        """
        if len(data) < EEPROM_HEADER_SIZE:
            return False

        stored_crc = struct.unpack(
            "<I", data[EEPROM_FIELDS_V3["crc32"][0] : EEPROM_FIELDS_V3["crc32"][0] + 4]
        )[0]
        expected_crc = binascii.crc32(data[:EEPROM_CRC_COVERAGE]) & 0xFFFFFFFF
        return stored_crc == expected_crc

    @staticmethod
    def verify_crc_static(data: bytes) -> bool:
        """Verify CRC32 of raw binary data (static method, no instance needed).

        Args:
            data: Raw 256-byte EEPROM header binary.

        Returns:
            True if stored CRC32 matches calculated CRC32.
        """
        if len(data) < EEPROM_HEADER_SIZE:
            return False

        crc_off = EEPROM_FIELDS_V3["crc32"][0]
        stored_crc = struct.unpack("<I", data[crc_off : crc_off + 4])[0]
        expected_crc = binascii.crc32(data[:EEPROM_CRC_COVERAGE]) & 0xFFFFFFFF
        return stored_crc == expected_crc

    def to_partition_image(self) -> bytes:
        """Create 4KB partition image for eeprom_data flash partition.

        Returns:
            4096-byte binary: 256-byte header at offset 0, rest zeros.
        """
        header_bytes = self.to_bytes()
        image = bytearray(EEPROM_PARTITION_SIZE)
        image[:EEPROM_HEADER_SIZE] = header_bytes
        return bytes(image)

    def to_dict(self) -> dict:
        """Convert to dictionary for logging/serialization.

        Returns:
            Dictionary with all header fields.
        """
        sig_hex = self.signature.hex() if self.signature else ""
        return {
            "version": self.VERSION,
            "signature_version": int(self.signature_algorithm),
            "signature_algorithm": self.signature_algorithm.name,
            "boardname": self.boardname,
            "boardversion": self.boardversion,
            "serial": self.serial,
            "usid": self.usid,
            "cpuid": self.cpuid,
            "mac": self.mac,
            "signature": sig_hex[:32] + "..." if sig_hex else "",
            "signature_present": bool(self.signature),
            "timestamp": self.timestamp,
            "crc32": None,  # Calculated on to_bytes()
        }

    # The v4 subclass renames the serial slot; repr and to_dict use this label
    SERIAL_LABEL = "serial"

    def __repr__(self) -> str:
        sig_info = (
            f"{self.signature_algorithm.name}({len(self.signature)}B)" if self.signature else "none"
        )
        return (
            f"{type(self).__name__}("
            f"board={self.boardname}/{self.boardversion}, "
            f"{self.SERIAL_LABEL}={self.serial}, "
            f"mac={self.mac}, "
            f"sig={sig_info})"
        )


class EEPROMHeaderV4(EEPROMHeaderV3):
    """JEEPROMHeaderv4 -- board-scoped identity header (RFC #56).

    The byte layout is identical to v3; the version byte carries the
    semantic break. The v3 ``serial`` slot is named ``board_serial``:
    it identifies this board, never the device (device identity lives
    in the ``device.id`` record). The inherited ``serial`` attribute
    remains as storage; ``board_serial`` is the canonical v4 name. Note:
    ``dataclasses.asdict``/``astuple`` reflect the storage field ``serial``
    — use ``to_dict()`` for the canonical v4 naming.
    """

    VERSION = 4
    SERIAL_LABEL = "board_serial"

    def __init__(self, *args, board_serial: str | None = None, **kwargs):
        if board_serial is not None:
            if "serial" in kwargs:
                raise TypeError("pass either board_serial or serial, not both")
            kwargs["serial"] = board_serial
        super().__init__(*args, **kwargs)

    @property
    def board_serial(self) -> str:
        """This board's serial number (the v4 name of the serial slot)."""
        return self.serial

    @board_serial.setter
    def board_serial(self, value: str) -> None:
        self.serial = value

    def to_dict(self) -> dict:
        d = super().to_dict()
        d["board_serial"] = d.pop("serial")
        return d
