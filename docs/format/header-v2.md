# JEEPROMHeaderv2 (256 bytes) — Legacy

<!-- STRUCT: JEEPROMHeaderv2 -->
<!-- SIZE: 256 -->
<!-- VERSION: 2 -->
<!-- CRC_FIELD: crc32 -->
<!-- CRC_COVERAGE: 0-251 -->

| Offset  | Size | Field        | Type         | Endianness    | Description                 |
|---------|------|--------------|--------------|--------------|-----------------------------|
| 0-7     | 8    | magic        | char[8]      | -             | "JETHOME\0"                 |
| 8       | 1    | version      | uint8_t      | -             | Header version = 2          |
| 9-11    | 3    | reserved1    | uint8_t[3]   | -             | Reserved (zeros)            |
| 12-43   | 32   | boardname    | char[32]     | -             | Board name, null-terminated |
| 44-75   | 32   | boardversion | char[32]     | -             | Board version, null-term.   |
| 76-107  | 32   | serial       | uint8_t[32]  | -             | Device serial number        |
| 108-139 | 32   | usid         | uint8_t[32]  | -             | CPU eFuse USID              |
| 140-171 | 32   | cpuid        | uint8_t[32]  | -             | CPU ID                      |
| 172-177 | 6    | mac          | uint8_t[6]   | -             | MAC address (6 raw bytes)   |
| 178-179 | 2    | reserved2    | uint8_t[2]   | -             | Reserved for extended MAC   |
| 180-251 | 72   | reserved3    | uint8_t[72]  | -             | Reserved for future use     |
| 252-255 | 4    | crc32        | uint32_t     | little-endian | CRC32 of bytes 0-251        |

## Notes

- Compact version of v1 (256 bytes vs 512 bytes).
- Removed `modules[16]` array (32 bytes of uint16_t module IDs).
- `reserved3` area (72 bytes) later became `signature` + `timestamp` in v3.
