# JEEPROMHeaderv3 (256 bytes) — Current Production Format

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
| 10-11   | 2    | header_reserved   | uint8_t[2]   | -             | Reserved (zeros)                     |
| 12-43   | 32   | boardname         | char[32]     | -             | Board name, null-terminated          |
| 44-75   | 32   | boardversion      | char[32]     | -             | Board version, null-terminated       |
| 76-107  | 32   | serial            | uint8_t[32]  | -             | Device serial number                 |
| 108-139 | 32   | usid              | uint8_t[32]  | -             | CPU eFuse USID                       |
| 140-171 | 32   | cpuid             | uint8_t[32]  | -             | CPU ID / factory MAC                 |
| 172-177 | 6    | mac               | uint8_t[6]   | -             | MAC address (6 raw bytes)            |
| 178-179 | 2    | reserved2         | uint8_t[2]   | -             | Reserved for extended MAC            |
| 180-243 | 64   | signature         | uint8_t[64]  | -             | ECDSA signature (r‖s, zero-padded)  |
| 244-251 | 8    | timestamp         | int64_t      | little-endian | Unix timestamp (seconds)             |
| 252-255 | 4    | crc32             | uint32_t     | little-endian | CRC32 of bytes 0-251                 |

## Notes

- Based on v2 layout with `reserved3` replaced by `signature` (64B) + `timestamp` (8B).
- `signature_version` at offset 9 replaces `reserved1[0]` from v2.
- `header_reserved[2]` at offset 10-11 replaces remaining `reserved1` bytes.
- CRC32 covers bytes 0-251 (includes signature and timestamp).
- String fields (`boardname`, `boardversion`) are UTF-8, null-terminated, zero-padded to field size.
- `serial`, `usid`, `cpuid` are raw byte arrays (not null-terminated strings).
