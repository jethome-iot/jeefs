# Common Header Properties

This file defines constants, enumerations, and shared properties used across all EEPROM header versions.

## Common Properties

- **Byte order:** All multi-byte fields are **little-endian**.
- **Magic:** `"JETHOME\0"` = bytes `4A 45 54 48 4F 4D 45 00` (8 bytes, null-terminated).
- **CRC32:** IEEE 802.3 polynomial `0xEDB88320` (same as zlib `crc32()`). Covers bytes 0 through `header_size - 4 - 1`.
- **Packing:** No padding — all structs use `#pragma pack(push, 1)`.
- **Emptiness (two domains, RFC #14):** *inside structures* (headers, file
  headers, the device-identity record) empty/reserved bytes are always
  `0x00`; *unmanaged medium space* past the last written structure reads
  `0xFF` when erased, `0x00` where the library has zero-filled spans it
  freed (delete, format) — both are legal empty space, and virgin erased
  space is never proactively rewritten. Validity of written data is decided by
  magic, CRC and bounds checks — never by content heuristics; the one
  emptiness heuristic (an unwritten file slot) MUST accept both `0x00` and
  `0xFF`, and an erased 16-bit link (`0xFFFF`) terminates the file chain
  like `0`. An all-`0x00`/all-`0xFF` buffer where a header or record is
  expected means "nothing written", not "corrupt".
- **String fields:** two kinds (RFC #13):
  - *name-like* (`boardname`, `boardversion`): null-terminated, zero-padded
    — at most `size - 1` content bytes;
  - *bounded* (`serial`, `usid`, `cpuid`): printable ASCII, zero-padded,
    NUL optional when the value fills the field — all bytes usable.
    Content validation is outside the library.

## Empty (placeholder) header

A header whose every byte from offset 12 up to (not including) its CRC
field is `0x00` is **empty**: it reserves the slot — the first file
write claims a headerless image with exactly such a header — and carries
no identity yet. Readers distinguish the two states by content
(`jeefs_header_is_empty` and the per-port equivalents): a provisioned
board has at least one nonzero identity byte. Emptiness is independent
of CRC validity — consistency stays a separate check.

## Version Detection Struct

<!-- STRUCT: JEEPROMHeaderversion -->
<!-- SIZE: 12 -->

| Offset | Size | Field     | Type        | Endianness | Description              |
|--------|------|-----------|-------------|------------|--------------------------|
| 0-7    | 8    | magic     | char[8]     | -          | Magic string "JETHOME\0" |
| 8      | 1    | version   | uint8_t     | -          | Header version number    |
| 9-11   | 3    | reserved1 | uint8_t[3]  | -          | Not used by detection    |

Used to detect the header version by reading only the first 12 bytes. The
`version` field determines which full struct to use for parsing. Detection
never inspects bytes 9-11; note that byte 10 is the `fs_version` field in
headers v3/v4 ([filesystem-v1.md](filesystem-v1.md)), not free space.

## Signature Algorithms

<!-- ENUM: JEEFSSignatureAlgorithm -->
<!-- C_PREFIX: JEEFS_SIG -->
<!-- PY_CLASS: SignatureAlgorithm -->

| Value | Name      | Signature Size | Description                        |
|-------|-----------|----------------|------------------------------------|
| 0     | NONE      | 0              | No signature                       |
| 1     | SECP192R1 | 48             | ECDSA secp192r1/NIST P-192, r‖s  |
| 2     | SECP256R1 | 64             | ECDSA secp256r1/NIST P-256, r‖s  |

- Stored in the `signature_version` byte at offset 9 of headers v3/v4 and of
  the DeviceIdentityV1 record ([device-identity-v1.md](device-identity-v1.md)).
- Signature is raw `r || s` concatenation (not DER-encoded).
- `secp192r1`: bytes 180-227 used (48B), bytes 228-243 are zeros.
- `secp256r1`: bytes 180-243 fully used (64B).

## Named Constants

<!-- CONSTANTS -->

| Name                | Value     | Type   | Description                             |
|---------------------|-----------|--------|-----------------------------------------|
| MAGIC               | "JETHOME" | string | Magic string (7 chars, without null)    |
| MAGIC_LENGTH        | 8         | int    | Magic field size (including null byte)  |
| HEADER_VERSION      | 4         | int    | Current (latest) header version         |
| SIGNATURE_FIELD_SIZE| 64        | int    | Total signature field size in bytes     |
| FILE_NAME_LENGTH    | 15        | int    | Max filename length (excluding null)    |
| FS_VERSION          | 1         | int    | Current filesystem version              |
| FS_VERSION_OFFSET   | 10        | int    | fs_version byte offset in the header    |
| MAC_LENGTH          | 6         | int    | MAC address size in bytes               |
| SERIAL_LENGTH       | 32        | int    | Serial field size in bytes              |
| USID_LENGTH         | 32        | int    | USID field size in bytes                |
| CPUID_LENGTH        | 32        | int    | CPUID field size in bytes               |
| BOARDNAME_LENGTH    | 31        | int    | Max boardname chars (excluding null)    |
| BOARDVERSION_LENGTH | 31        | int    | Max boardversion chars (excluding null) |
| EMPTYBYTE           | 0x00      | byte   | Empty byte inside structures            |
| ERASEDBYTE          | 0xFF      | byte   | Erased-medium byte (unmanaged space)    |
| PARTITION_SIZE      | 4096      | int    | Flash partition image size (4KB)        |

## Union Type

The C implementation provides a union for version-agnostic header handling:

<!-- UNION: JEEPROMHeader -->

| Member  | Type                 | Description              |
|---------|----------------------|--------------------------|
| version | JEEPROMHeaderversion | Version detection (12B)  |
| v1      | JEEPROMHeaderv1      | Full v1 header (512B)    |
| v2      | JEEPROMHeaderv2      | Full v2 header (256B)    |
| v3      | JEEPROMHeaderv3      | Full v3 header (256B)    |
| v4      | JEEPROMHeaderv4      | Full v4 header (256B)    |
