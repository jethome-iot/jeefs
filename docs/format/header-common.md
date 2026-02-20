# Common Header Properties

This file defines constants, enumerations, and shared properties used across all EEPROM header versions.

## Common Properties

- **Byte order:** All multi-byte fields are **little-endian**.
- **Magic:** `"JETHOME\0"` = bytes `4A 45 54 48 4F 4D 45 00` (8 bytes, null-terminated).
- **CRC32:** IEEE 802.3 polynomial `0xEDB88320` (same as zlib `crc32()`). Covers bytes 0 through `header_size - 4 - 1`.
- **Packing:** No padding — all structs use `#pragma pack(push, 1)`.
- **Empty bytes:** Both `0x00` and `0xFF` are treated as "empty/unwritten".
- **String fields:** Null-terminated UTF-8, zero-padded to field size.

## Version Detection Struct

<!-- STRUCT: JEEPROMHeaderversion -->
<!-- SIZE: 12 -->

| Offset | Size | Field     | Type        | Endianness | Description              |
|--------|------|-----------|-------------|------------|--------------------------|
| 0-7    | 8    | magic     | char[8]     | -          | Magic string "JETHOME\0" |
| 8      | 1    | version   | uint8_t     | -          | Header version number    |
| 9-11   | 3    | reserved1 | uint8_t[3]  | -          | Alignment / reserved     |

Used to detect the header version by reading only the first 12 bytes. The `version` field determines which full struct to use for parsing.

## Signature Algorithms

<!-- ENUM: JEEFSSignatureAlgorithm -->
<!-- C_PREFIX: JEEFS_SIG -->
<!-- PY_CLASS: SignatureAlgorithm -->

| Value | Name      | Signature Size | Description                        |
|-------|-----------|----------------|------------------------------------|
| 0     | NONE      | 0              | No signature                       |
| 1     | SECP192R1 | 48             | ECDSA secp192r1/NIST P-192, r‖s  |
| 2     | SECP256R1 | 64             | ECDSA secp256r1/NIST P-256, r‖s  |

- Stored in `signature_version` field at offset 9 (v3 only).
- Signature is raw `r || s` concatenation (not DER-encoded).
- `secp192r1`: bytes 180-227 used (48B), bytes 228-243 are zeros.
- `secp256r1`: bytes 180-243 fully used (64B).

## Named Constants

<!-- CONSTANTS -->

| Name                | Value     | Type   | Description                             |
|---------------------|-----------|--------|-----------------------------------------|
| MAGIC               | "JETHOME" | string | Magic string (7 chars, without null)    |
| MAGIC_LENGTH        | 8         | int    | Magic field size (including null byte)  |
| HEADER_VERSION      | 3         | int    | Current (latest) header version         |
| SIGNATURE_FIELD_SIZE| 64        | int    | Total signature field size in bytes     |
| FILE_NAME_LENGTH    | 15        | int    | Max filename length (excluding null)    |
| MAC_LENGTH          | 6         | int    | MAC address size in bytes               |
| SERIAL_LENGTH       | 32        | int    | Serial field size in bytes              |
| USID_LENGTH         | 32        | int    | USID field size in bytes                |
| CPUID_LENGTH        | 32        | int    | CPUID field size in bytes               |
| BOARDNAME_LENGTH    | 31        | int    | Max boardname chars (excluding null)    |
| BOARDVERSION_LENGTH | 31        | int    | Max boardversion chars (excluding null) |
| EEPROM_EMPTYBYTE    | 0x00      | byte   | Default empty byte marker               |
| EEPROM_PARTITION_SIZE | 4096    | int    | Flash partition image size (4KB)        |

## Union Type

The C implementation provides a union for version-agnostic header handling:

<!-- UNION: JEEPROMHeader -->

| Member  | Type                 | Description              |
|---------|----------------------|--------------------------|
| version | JEEPROMHeaderversion | Version detection (12B)  |
| v1      | JEEPROMHeaderv1      | Full v1 header (512B)    |
| v2      | JEEPROMHeaderv2      | Full v2 header (256B)    |
| v3      | JEEPROMHeaderv3      | Full v3 header (256B)    |
