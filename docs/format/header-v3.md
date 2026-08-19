# JEEPROMHeaderv3 (256 bytes) — Fielded Production Format (superseded by v4)

<!-- STRUCT: JEEPROMHeaderv3 -->
<!-- SIZE: 256 -->
<!-- VERSION: 3 -->
<!-- CRC_FIELD: crc32 -->
<!-- CRC_COVERAGE: 0-251 -->

| Offset  | Size | Field             | Type         | Endianness    | Description                          |
|---------|------|-------------------|--------------|---------------|--------------------------------------|
| 0-7     | 8    | magic             | char[8]      | -             | "JETHOME\0" (null-terminated string) |
| 8       | 1    | version           | uint8_t      | -             | Header version = 3                   |
| 9       | 1    | signature_version | uint8_t      | -             | Signature algorithm (see enums)      |
| 10      | 1    | fs_version        | uint8_t      | -             | Filesystem version: 0 = none, 1 = current |
| 11      | 1    | header_reserved   | uint8_t      | -             | Reserved (zeros)                     |
| 12-43   | 32   | boardname         | char[32]     | -             | Board name, null-terminated          |
| 44-75   | 32   | boardversion      | char[32]     | -             | Board version, null-terminated       |
| 76-107  | 32   | serial            | uint8_t[32]  | -             | Board serial number (bounded string) |
| 108-139 | 32   | usid              | uint8_t[32]  | -             | CPU eFuse USID (bounded string)      |
| 140-171 | 32   | cpuid             | uint8_t[32]  | -             | CPU ID / factory MAC (bounded string)|
| 172-177 | 6    | mac               | uint8_t[6]   | -             | MAC address (6 raw bytes)            |
| 178-179 | 2    | reserved2         | uint8_t[2]   | -             | Reserved for extended MAC            |
| 180-243 | 64   | signature         | uint8_t[64]  | -             | ECDSA signature (r‖s, zero-padded)  |
| 244-251 | 8    | timestamp         | int64_t      | little-endian | Unix timestamp (seconds)             |
| 252-255 | 4    | crc32             | uint32_t     | little-endian | CRC32 of bytes 0-251                 |

## Notes

- Based on v2 layout with `reserved3` replaced by `signature` (64B) + `timestamp` (8B).
- `signature_version` at offset 9 replaces `reserved1[0]` from v2.
- `fs_version` (offset 10) and `header_reserved` (offset 11) replace the
  remaining `reserved1` bytes. `fs_version` was reserved-zero until the
  filesystem gained its own version: `0` = no filesystem, `1` = the layout
  in [filesystem-v1.md](filesystem-v1.md) — every image written before the
  field existed carries `0`, which is accurate (fielded v3 devices have
  empty file areas).
- CRC32 covers bytes 0-251 (includes signature and timestamp).
- String fields (`boardname`, `boardversion`) are UTF-8, null-terminated, zero-padded to field size.
- `serial`, `usid`, `cpuid` are **bounded strings** (RFC #13): printable
  ASCII (0x20-0x7E, punctuation included), zero-padded; the NUL terminator
  is optional and appears only when the value is shorter than the field, so
  all 32 bytes are usable. Content validation is the producer's concern,
  not the library's. `serial` is board-scoped: it identifies this board,
  not the device (device identity is RFC #26); header v4
  ([header-v4.md](header-v4.md)) renames the field to `board_serial`.
