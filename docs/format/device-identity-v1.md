# DeviceIdentityV1 (256 bytes) — DRAFT

> **Status: DRAFT — under discussion in RFC
> [#26](https://github.com/jethome-iot/jeefs/issues/26). Do not implement.**
> Codegen metadata comments (`STRUCT`/`SIZE`/`CRC_*`) are intentionally absent and
> will be added only after the RFC is agreed, so this draft never reaches
> generated code by accident.

## Motivation

A device may contain several boards with their own EEPROM (SoM/CPU module and a
mainboard/carrier). Each board carries a board-scoped JEEFS header
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

| Offset  | Size | Field             | Type        | Endianness    | Description                                |
|---------|------|-------------------|-------------|---------------|--------------------------------------------|
| 0-7     | 8    | magic             | char[8]     | -             | "JHDEVID\0" (null-terminated string)       |
| 8       | 1    | record_version    | uint8_t     | -             | Record version = 1                         |
| 9       | 1    | signature_version | uint8_t     | -             | Signature algorithm (same enum as header)  |
| 10-11   | 2    | reserved1         | uint8_t[2]  | -             | Reserved (zeros)                           |
| 12-43   | 32   | device_model      | char[32]    | -             | Device model name, null-terminated         |
| 44-75   | 32   | device_serial     | char[32]    | -             | Device serial number, null-terminated      |
| 76-91   | 16   | hw_revision       | char[16]    | -             | Device hardware revision, null-terminated  |
| 92-93   | 2    | flags             | uint16_t    | little-endian | Reserved flags (zeros)                     |
| 94-179  | 86   | reserved2         | uint8_t[86] | -             | Reserved for future use (zeros)            |
| 180-243 | 64   | signature         | uint8_t[64] | -             | ECDSA signature (r‖s, zero-padded)         |
| 244-251 | 8    | timestamp         | int64_t     | little-endian | Unix timestamp (seconds)                   |
| 252-255 | 4    | crc32             | uint32_t    | little-endian | CRC32 of bytes 0-251                       |

- CRC32: IEEE 802.3 polynomial, same convention as headers.
- The record size matches the v2/v3 header size (256 bytes), and the tail
  layout is **byte-identical to header v3**: `signature` at offset 180,
  `timestamp` at 244, `crc32` at 252, CRC coverage 0-251. Header-v3 signature
  and CRC handling code applies to the record without offset changes; the
  86-byte reserve leaves room for future fields (or a longer signature via a
  new `record_version`) without another size change.
- The record deliberately carries **no MAC fields**: board MACs are provisioned
  independently and are not contiguous, so a base+count pool cannot describe
  them; the device MAC is taken from the cpuboard header. A real MAC pool, if
  ever needed, arrives with a new `record_version`.
- Signature: same ECDSA infrastructure (algorithms, keys, signature service) as
  the header. What data is signed, how it is verified and by whom is outside
  the scope of this spec — a firmware/production concern. The spec defines
  only the field layout and the `signature_version` algorithm enum.
- String semantics follow the outcome of the raw-vs-string RFC
  ([#13](https://github.com/jethome-iot/jeefs/issues/13)); the draft assumes
  null-terminated UTF-8, zero-padded.
- Versioning: a parser encountering an unknown `record_version` returns an
  error. No forward-compatibility guessing.

## Reserved file name

`device.id` (fits the 15-character limit). To be added to
[header-common.md](header-common.md) as a named constant after approval.

## Signature algorithm rationale

[DECISION] The record reuses the header's ECDSA scheme (secp192r1 legacy /
secp256r1 current, raw r‖s in a fixed 64-byte field) rather than Ed25519. The
production and runtime stack is ESP32/ESP-IDF: mbedTLS — ESP-IDF's crypto
library — does not implement Ed25519, while the ESP32 hardware ECDSA
peripheral supports exactly P-192 and P-256 with eFuse-resident keys; Ed25519
would be software-only with no eFuse key integration. A future algorithm, if
ever needed, arrives as a new `signature_version` value, not a layout change.

## Open questions (for RFC #26)

- Whether `flags` should encode "identity locked" / provisioning state.
- Terminology: [header-v3.md](header-v3.md) describes the `serial` field as
  "Device serial number", while in multi-board products it is board-scoped —
  the header specs' field descriptions need aligning once this RFC settles.
- Interaction with the `0xFF is empty` rule
  ([#14](https://github.com/jethome-iot/jeefs/issues/14)) for unwritten records.
