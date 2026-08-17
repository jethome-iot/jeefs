# DeviceIdentityV1 (256 bytes)

## Motivation

A device may contain several boards with their own EEPROM (the CPU module —
"cpuboard" below — and a mainboard/carrier). Each board carries a board-scoped
JEEFS header
([header-v3.md](header-v3.md)): boardname, boardversion, serial, MAC.
The **device** as a product has its own identity — device serial number, model
name, hardware revision — which must survive board replacement during repair.

Header v3 stays unchanged and strictly board-scoped. Device identity is stored
as a separate signed fixed-size record: a **self-contained 256-byte blob**
(own magic, version, CRC32, signature) that is parsed by a pure function over
a byte buffer, exactly like the headers. The record lives as the file
`device.id` in a JEEFS filesystem.

This spec defines the record format and the reserved file name — nothing else.
Which board's EEPROM carries the file, in what order files are written, who
reads or verifies the record and when — production and firmware concerns,
outside the scope of this document and of the library (which sees 8 KiB of
data and read/write requests).

Prior art: IPMI FRU (Chassis Area "present in exactly one FRU device of a
system"), NVIDIA Jetson (system part/serial fields populated only in the carrier
EEPROM), ONIE TlvInfo, Raspberry Pi HAT, Toradex config block.

## Record layout

<!-- STRUCT: DeviceIdentityV1 -->
<!-- SIZE: 256 -->
<!-- CRC_FIELD: crc32 -->
<!-- CRC_COVERAGE: 0-251 -->

| Offset  | Size | Field             | Type        | Endianness    | Description                                |
|---------|------|-------------------|-------------|---------------|--------------------------------------------|
| 0-7     | 8    | magic             | char[8]     | -             | "JHDEVID\0" (null-terminated string)       |
| 8       | 1    | record_version    | uint8_t     | -             | Record version = 1                         |
| 9       | 1    | signature_version | uint8_t     | -             | Signature algorithm (same enum as header)  |
| 10-11   | 2    | reserved1         | uint8_t[2]  | -             | Reserved (zeros)                           |
| 12-43   | 32   | device_model      | char[32]    | -             | Device model name (bounded string)         |
| 44-75   | 32   | device_serial     | char[32]    | -             | Device serial number (bounded string)      |
| 76-91   | 16   | hw_revision       | char[16]    | -             | Device hardware revision (bounded string)  |
| 92-93   | 2    | flags             | uint16_t    | little-endian | Flags: all bits reserved (see below)       |
| 94-179  | 86   | reserved2         | uint8_t[86] | -             | Reserved for future use (zeros)            |
| 180-243 | 64   | signature         | uint8_t[64] | -             | ECDSA signature (r‖s, zero-padded)         |
| 244-251 | 8    | timestamp         | int64_t     | little-endian | Record creation/signing time (Unix s)      |
| 252-255 | 4    | crc32             | uint32_t    | little-endian | CRC32 of bytes 0-251                       |

- CRC32: IEEE 802.3 polynomial, same convention as headers.
- The record size matches the v2/v3 header size (256 bytes), and the tail
  layout is **byte-identical to header v3**: `signature` at offset 180,
  `timestamp` at 244, `crc32` at 252, CRC coverage 0-251. The offset
  arithmetic and CRC/signature conventions are shared with header v3. Note
  for implementers: only the CRC32 helper and the little-endian accessors are
  reusable as-is — all public `jeefs_header_*` entry points gate on the
  "JETHOME\0" magic and the version 1-3 size table, so record support means
  parameterizing them or adding a parallel record module, not calling the
  existing functions. The 86-byte
  reserve leaves room for future fields (or a longer signature via a new
  `record_version`) without another size change.
- The record deliberately carries **no MAC fields**: board MACs are provisioned
  independently and are not contiguous, so a base+count pool cannot describe
  them; the device MAC is taken from the cpuboard header. A real MAC pool, if
  ever needed, arrives with a new `record_version`.
- Signature: same ECDSA infrastructure (algorithms, keys, signature service) as
  the header. What data is signed, how it is verified and by whom is outside
  the scope of this spec — a firmware/production concern. The spec defines
  only the field layout and the `signature_version` algorithm enum.
- `hw_revision` format: dot-separated numbers with an optional trailing
  letter suffix encoding the device variant (e.g. `1.2`, `1.2a`). Firmware
  identifies the hardware by `device_model` and `hw_revision`; no numeric
  product/SKU registry is introduced.
- Fields derivable from the database by `device_serial`/board `usid` (batch,
  part number, manufacture date, vendor, per-unit UUID) are deliberately NOT
  stored: the record carries only what the device itself needs without
  database access.
- `device_model`, `device_serial` and `hw_revision` are **bounded strings**
  per the settled RFC [#13](https://github.com/jethome-iot/jeefs/issues/13)
  semantics: printable ASCII (0x20-0x7E), zero-padded, the NUL terminator is
  optional and appears only when the value is shorter than the field — every
  byte of the field is usable. Content validation is the producer's concern,
  not the library's.
- Versioning: a parser encountering an unknown `record_version` returns an
  error, and an unknown `signature_version` value (outside the enum) with a
  known `record_version` is equally a parse error. No forward-compatibility
  guessing.
- Reserved space (`reserved1`, `reserved2`, all `flags` bits): writers MUST
  write zeros; readers MUST ignore the content — never reject a record with
  non-zero reserved bytes as corrupt. Future `record_version` values are the
  only way reserved space gains meaning; `flags` bit semantics likewise
  arrive only with a new `record_version`.
- Erased storage needs no special case at the record layer: an all-`0x00` or
  all-`0xFF` buffer (both are "empty" per
  [header-common.md](header-common.md)) fails the magic check — "no record",
  not "corrupt record". When the record is read as the `device.id` file, the
  JEEFS file CRC32 gates the payload first; the record's own magic/CRC are a
  second, standalone check for tooling that only ever sees the extracted
  256 bytes — the double CRC is deliberate.

## Reserved file name

`device.id` (fits the 15-character limit).

<!-- CONSTANTS -->

| Name               | Value       | Type   | Description                              |
|--------------------|-------------|--------|------------------------------------------|
| DEVID_MAGIC        | "JHDEVID"   | string | Record magic (7 chars, NUL-terminated)   |
| DEVICE_ID_FILENAME | "device.id" | string | Reserved JEEFS file name for the record  |

## Signature algorithm rationale

The record reuses the header's ECDSA scheme (secp192r1 legacy /
secp256r1 current, raw r‖s in a fixed 64-byte field) rather than Ed25519. The
production and runtime stack is ESP32/ESP-IDF: mbedTLS — ESP-IDF's crypto
library — does not implement Ed25519, while the ESP32 hardware ECDSA
peripheral supports exactly P-192 and P-256 with eFuse-resident keys; Ed25519
would be software-only with no eFuse key integration. A future algorithm, if
ever needed, arrives as a new `signature_version` value, not a layout change.
