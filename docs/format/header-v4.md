# JEEPROMHeaderv4 (256 bytes) — Board-Scoped Identity

<!-- STRUCT: JEEPROMHeaderv4 -->
<!-- SIZE: 256 -->
<!-- VERSION: 4 -->
<!-- CRC_FIELD: crc32 -->
<!-- CRC_COVERAGE: 0-251 -->

| Offset  | Size | Field             | Type         | Endianness    | Description                          |
|---------|------|-------------------|--------------|---------------|--------------------------------------|
| 0-7     | 8    | magic             | char[8]      | -             | "JETHOME\0" (null-terminated string) |
| 8       | 1    | version           | uint8_t      | -             | Header version = 4                   |
| 9       | 1    | signature_version | uint8_t      | -             | Signature algorithm (see enums)      |
| 10-11   | 2    | header_reserved   | uint8_t[2]   | -             | Reserved (zeros)                     |
| 12-43   | 32   | boardname         | char[32]     | -             | Board name, null-terminated          |
| 44-75   | 32   | boardversion      | char[32]     | -             | Board version, null-terminated       |
| 76-107  | 32   | board_serial      | uint8_t[32]  | -             | Board serial number (bounded string) |
| 108-139 | 32   | usid              | uint8_t[32]  | -             | Board CPU/eFuse USID, if available   |
| 140-171 | 32   | cpuid             | uint8_t[32]  | -             | Board CPU ID, if available           |
| 172-177 | 6    | mac               | uint8_t[6]   | -             | MAC address (6 raw bytes)            |
| 178-179 | 2    | reserved2         | uint8_t[2]   | -             | Reserved (alignment)                 |
| 180-243 | 64   | signature         | uint8_t[64]  | -             | ECDSA signature (r‖s, zero-padded)  |
| 244-251 | 8    | timestamp         | int64_t      | little-endian | Header creation/signing time (Unix s)|
| 252-255 | 4    | crc32             | uint32_t     | little-endian | CRC32 of bytes 0-251                 |

## Notes

- **Byte layout is identical to v3** — same offsets, sizes, tail and CRC
  coverage. The version bump carries a semantic break, not a layout change:
  RFC [#26](https://github.com/jethome-iot/jeefs/issues/26) split the
  identity model, and a v4 header is **explicitly board-scoped**. Device
  identity lives in the `device.id` record
  ([device-identity-v1.md](device-identity-v1.md)), never in the header.
- `board_serial` (offset 76, the v3 `serial` field renamed): the serial
  number of **this board**. In v3 the field is historically ambiguous
  (fielded devices carry a device serial there); the version byte tells the
  reader which interpretation applies. The legacy fallback for v3 devices
  without a `device.id` record (device serial = cpuboard header serial) is a
  production convention recorded in RFC #26, not part of this spec.
- `usid`, `cpuid`: optional board identifiers, populated **if available**
  (boards carry different identifier sets); content is producer-defined,
  zero-filled when absent. Not specified further on purpose.
- String semantics as in v3 (RFC
  [#13](https://github.com/jethome-iot/jeefs/issues/13)): `boardname`,
  `boardversion` null-terminated (31 usable bytes); `board_serial`, `usid`,
  `cpuid` bounded strings — printable ASCII, zero-padded, NUL optional when
  the value fills the field, all 32 bytes usable.
- `timestamp` is the header creation/signing moment, mirroring
  DeviceIdentityV1.
- `reserved2` keeps the v1-era alignment split of the reserved space.
- CRC32 covers bytes 0-251 (includes signature and timestamp); signature
  conventions are unchanged from v3.
