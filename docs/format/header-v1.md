# JEEPROMHeaderv1 (512 bytes) — Legacy

<!-- STRUCT: JEEPROMHeaderv1 -->
<!-- SIZE: 512 -->
<!-- VERSION: 1 -->
<!-- CRC_FIELD: crc32 -->
<!-- CRC_COVERAGE: 0-507 -->

| Offset  | Size | Field        | Type          | Endianness    | Description                 |
|---------|------|--------------|---------------|--------------|-----------------------------|
| 0-7     | 8    | magic        | char[8]       | -             | "JETHOME\0"                 |
| 8       | 1    | version      | uint8_t       | -             | Header version = 1          |
| 9-11    | 3    | reserved1    | uint8_t[3]    | -             | Reserved (zeros)            |
| 12-43   | 32   | boardname    | char[32]      | -             | Board name, null-terminated |
| 44-75   | 32   | boardversion | char[32]      | -             | Board version, null-term.   |
| 76-107  | 32   | serial       | uint8_t[32]   | -             | Device serial number        |
| 108-139 | 32   | usid         | uint8_t[32]   | -             | CPU eFuse USID              |
| 140-171 | 32   | cpuid        | uint8_t[32]   | -             | CPU ID                      |
| 172-177 | 6    | mac          | uint8_t[6]    | -             | MAC address (6 raw bytes)   |
| 178-179 | 2    | reserved2    | uint8_t[2]    | -             | Reserved for extended MAC   |
| 180-211 | 32   | modules      | uint16_t[16]  | little-endian | 16 module IDs               |
| 212-507 | 296  | reserved3    | uint8_t[296]  | -             | Reserved for future use     |
| 508-511 | 4    | crc32        | uint32_t      | little-endian | CRC32 of bytes 0-507        |

## Notes

- Original 512-byte header format.
- `modules` is an array of 16 little-endian uint16_t hardware module IDs.
- Superseded by v2 (compact, no modules) and v3 (compact + signature).
