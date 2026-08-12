# DeviceIdentityV1 (192 bytes) — DRAFT

> **Status: DRAFT — under discussion in RFC
> [#26](https://github.com/jethome-iot/jeefs/issues/26). Do not implement.**
> Codegen metadata comments (`STRUCT`/`SIZE`/`CRC_*`) are intentionally absent and
> will be added only after the RFC is agreed, so this draft never reaches
> generated code by accident.

## Motivation

A device may contain several boards with their own EEPROM (SoM/CPU module and a
mainboard/carrier). Each board carries a board-scoped JEEFS header
([header-v3.md](header-v3.md)): board name, board version, board serial, MAC.
The **device** as a product has its own identity — device serial number, model
name, hardware revision, a MAC pool — which must survive board replacement
during repair and be available to the bootloader.

Header v3 stays unchanged and strictly board-scoped. Device identity is stored
as a separate signed fixed-size record: a **self-contained 192-byte blob**
(own magic, version, CRC32, signature) that is parsed by a pure function over
a byte buffer, exactly like the headers. The container is a per-product
detail: the file `device.id` in the JEEFS filesystem of a board EEPROM, or a
raw record in a dedicated region of alternative storage (eMMC RPMB, SPI-flash
partition) on products that have no EEPROM at all.

Prior art: IPMI FRU (Chassis Area "present in exactly one FRU device of a
system"), NVIDIA Jetson (system part/serial fields populated only in the carrier
EEPROM), ONIE TlvInfo (MAC pool as base + count), Raspberry Pi HAT (bootloader
publishes EEPROM identity into the device tree), Toradex config block (anchor
block on the SoM, extra blocks on the carrier).

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
| 94-115  | 22   | reserved2         | uint8_t[22] | -             | Reserved for future use (zeros)            |
| 116-179 | 64   | signature         | uint8_t[64] | -             | ECDSA signature (r‖s, zero-padded)         |
| 180-187 | 8    | timestamp         | int64_t     | little-endian | Unix timestamp (seconds)                   |
| 188-191 | 4    | crc32             | uint32_t    | little-endian | CRC32 of bytes 0-187                       |

- CRC32: IEEE 802.3 polynomial, same convention as headers.
- The record deliberately carries **no MAC fields**: board MACs are provisioned
  independently and are not contiguous, so a base+count pool cannot describe
  them. The device MAC is defined by a lookup rule instead: the `mac` field of
  the cpuboard header. No duplication — nothing to fall out of sync. A real
  MAC pool, if ever needed, arrives with a new `record_version`.
- Signature: same ECDSA infrastructure (algorithms, keys, signature service) as
  the header. The signing coverage and the signature-field zeroing procedure
  are not defined by the header specs either — fixing them for this record is
  an open question for RFC #26.
- String semantics follow the outcome of the raw-vs-string RFC
  ([#13](https://github.com/jethome-iot/jeefs/issues/13)); the draft assumes
  null-terminated UTF-8, zero-padded.

## Placement and storage

1. **Reserved file name** (EEPROM placement): `device.id` (fits the
   15-character limit). To be added to [header-common.md](header-common.md) as
   a named constant after approval.
2. **Storage priority**: the identity storage is declared by the product
   configuration (board config / device tree). Default probe order for generic
   tools: **cpuboard EEPROM → motherboard EEPROM → the product's alternative
   storage** (rule 6). Rationale for cpuboard-first: the device MAC already
   comes from the cpuboard header, so replacing the cpuboard re-provisions
   identity-bearing data anyway, and the bootloader runs on the CPU board —
   its storage is available earliest at boot. Not every product has an EEPROM
   on the cpuboard, and some have no EEPROM at all — hence the fallback chain.
3. **Uniqueness**: exactly one identity record per device **across all
   storages**. Conflict resolution follows the probe order (cpuboard storage
   wins), then the newer timestamp; conflicts are logged.
4. **First-file invariant**: `device.id` is written as the **first** file of the
   filesystem, so the record body sits at offset `header_size + 24`
   ([filesystem-v1.md](filesystem-v1.md): the first file header starts at
   `header_size`). That is **280** for the 256-byte v2/v3 headers and **536**
   for the 512-byte v1 header. Anchor boards are provisioned with v3 headers,
   so bootloaders probe offset 280 first (verify the record magic), then 536,
   then fall back to a full chain walk. FS operations (delete, rewrite,
   compaction) must not move the first file.
5. **Board role** (SOM = 1, MAINBOARD = 2, PERIPHERAL = 3): documented now,
   materialized as a header field only in a future header v4 —
   [header-v3.md](header-v3.md) keeps bytes 10-11 "Reserved (zeros)", and the
   compatibility policy allows new fields only via a new header version.
   Anchor detection is based on the presence of the identity record.
6. **Alternative storages** (products without a usable EEPROM):
   - **eMMC RPMB**: the record at offset 0 of a dedicated RPMB region. RPMB
     authenticated writes and replay protection complement the record
     signature (anti-rollback for identity).
   - **SPI-flash**: the record at offset 0 of a dedicated partition
     (canonical partition name — open question, e.g. `devid`).
   - **eFuse**: 192 bytes generally do not fit (e.g. 32-byte user blocks on
     ESP32). A compact subset profile is an open question; until it is
     defined, eFuse-only products keep their existing scheme and are out of
     scope for record v1.

## Boot and OS access

- The identity storage is the only source of truth. U-Boot reads the board
  headers and the identity record (per the probe order of the product
  configuration), selects the device tree / overlays by the boardname pair, and
  publishes device identity into the DT (e.g. a `/firmware/jethome` node, the
  way RPi HAT publishes `/hat`) and into environment variables as a cache.
- The U-Boot environment is never a storage of identity.
- No kernel parser is required: userspace reads `/proc/device-tree`.

## Production and repair

- Board test stage writes and signs the board header (unchanged workflow).
- Final assembly writes the identity record: for EEPROM placement — as the
  **first file created** on the anchor board's filesystem (`EEPROM_AddFile` on
  an empty FS + signature from the signature service); provisioning any other
  file before `device.id` violates placement rule 4, since `EEPROM_AddFile`
  appends to the end of the chain and a late `device.id` would not land at the
  fixed offset. For raw placements — the record is written at offset 0 of the
  dedicated RPMB region / flash partition.
- Repair: replacing a board that does not carry the identity storage does not
  touch device identity. Replacing the identity-bearing board (typically the
  cpuboard) re-provisions the single 192-byte record together with the
  board's own provisioning — MAC and header signing already require a factory
  step there (signature service needs an API to re-sign a record for an
  existing device serial).
- **Legacy fallback** (documented, not stored): for devices without an identity
  record, device serial = cpuboard serial (from its header) until the record
  is provisioned. The fleet migrates without header reflashes.

## Signature algorithm rationale

[DECISION] The record reuses the header's ECDSA scheme (secp192r1 legacy /
secp256r1 current, raw r‖s in a fixed 64-byte field) rather than Ed25519. The
production and runtime stack is ESP32/ESP-IDF: mbedTLS — ESP-IDF's crypto
library — does not implement Ed25519, while the ESP32 hardware ECDSA
peripheral supports exactly P-192 and P-256 with eFuse-resident keys; Ed25519
would be software-only with no eFuse key integration. A future algorithm, if
ever needed, arrives as a new `signature_version` value, not a layout change.

## Open questions (for RFC #26)

- Exact signature coverage bytes and signing procedure for the record.
- Whether `flags` should encode "identity locked" / provisioning state.
- Canonical identifiers for the alternative storages: RPMB region selector and
  SPI-flash partition name (e.g. `devid`).
- A compact eFuse profile (subset of fields) for products where 192 bytes do
  not fit — or declaring eFuse permanently out of scope.
- Whether a board-role hint may be carried in a v3 reserved byte before v4
  exists — deferred: it would relax header-v3.md's "reserved = zeros" rule and
  contradict the "new fields only via a new header version" policy.
- Interaction with the `0xFF is empty` rule
  ([#14](https://github.com/jethome-iot/jeefs/issues/14)) for unwritten records.
- How the FS layer enforces the first-file invariant: reject a non-first
  `device.id`, reserve the first slot on anchor boards, or rely on production
  ordering discipline alone.
- Whether `signature_version = 0` (NONE) is permitted for device identity —
  i.e. whether unsigned records are acceptable outside development. The layout
  reuses the header enum, which allows NONE unless this spec forbids it.
