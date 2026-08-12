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
as a separate signed fixed-format record: the file `device.id` in the JEEFS
filesystem of the **anchor board**.

Prior art: IPMI FRU (Chassis Area "present in exactly one FRU device of a
system"), NVIDIA Jetson (system part/serial fields populated only in the carrier
EEPROM), ONIE TlvInfo (MAC pool as base + count), Raspberry Pi HAT (bootloader
publishes EEPROM identity into the device tree).

## Record layout

| Offset  | Size | Field             | Type        | Endianness    | Description                              |
|---------|------|-------------------|-------------|---------------|------------------------------------------|
| 0-7     | 8    | magic             | char[8]     | -             | "JHDEVID\0" (null-terminated string)     |
| 8       | 1    | record_version    | uint8_t     | -             | Record version = 1                       |
| 9       | 1    | signature_version | uint8_t     | -             | Signature algorithm (same enum as header) |
| 10-11   | 2    | reserved1         | uint8_t[2]  | -             | Reserved (zeros)                         |
| 12-43   | 32   | device_model      | char[32]    | -             | Device model name, null-terminated       |
| 44-75   | 32   | device_serial     | char[32]    | -             | Device serial number, null-terminated    |
| 76-91   | 16   | hw_revision       | char[16]    | -             | Device hardware revision, null-terminated |
| 92-97   | 6    | mac_pool_base     | uint8_t[6]  | -             | First MAC address of the device pool     |
| 98-99   | 2    | mac_pool_count    | uint16_t    | little-endian | Number of MACs in the pool (0 = none)    |
| 100-101 | 2    | flags             | uint16_t    | little-endian | Reserved flags (zeros)                   |
| 102-115 | 14   | reserved2         | uint8_t[14] | -             | Reserved for future use (zeros)          |
| 116-179 | 64   | signature         | uint8_t[64] | -             | ECDSA signature (r‖s, zero-padded)      |
| 180-187 | 8    | timestamp         | int64_t     | little-endian | Unix timestamp (seconds)                 |
| 188-191 | 4    | crc32             | uint32_t    | little-endian | CRC32 of bytes 0-187                     |

- CRC32: IEEE 802.3 polynomial, same convention as headers.
- Signature: same ECDSA infrastructure and coverage convention as header v3
  (signature field zeroed during signing; exact coverage to be fixed in the RFC).
- String semantics follow the outcome of the raw-vs-string RFC
  ([#13](https://github.com/jethome-iot/jeefs/issues/13)); the draft assumes
  null-terminated UTF-8, zero-padded.

## Placement rules

1. **Reserved file name**: `device.id` (fits the 15-character limit). To be added
   to [header-common.md](header-common.md) as a named constant after approval.
2. **Anchor board**: the board that defines the product — mainboard/carrier. If
   the device has a single board with EEPROM, that board is the anchor. The SoM
   is never the anchor (it is interchangeable and migrates between products).
3. **Uniqueness**: exactly one `device.id` per device. Conflict resolution:
   mainboard wins, then the record with the newer timestamp; conflicts are logged.
4. **First-file invariant**: `device.id` is written as the **first** file of the
   filesystem, so the record body sits at offset `header_size + 24`
   ([filesystem-v1.md](filesystem-v1.md): the first file header starts at
   `header_size`). That is **280** for the 256-byte v2/v3 headers and **536**
   for the 512-byte v1 header. Anchor boards are provisioned with v3 headers,
   so bootloaders probe offset 280 first (verify the record magic), then 536,
   then fall back to a full chain walk. FS operations (delete, rewrite,
   compaction) must not move the first file.
5. **Board role** (SOM = 1, MAINBOARD = 2, PERIPHERAL = 3): documented now,
   materialized as a header field only in a future header v4. For new v3 batches
   the role MAY be written into `header_reserved[0]` before signing
   (0 = legacy, semantics of existing devices unchanged). Anchor detection
   remains based on the presence of `device.id`; the role byte is a hint.

## Boot and OS access

- The EEPROM is the only source of truth. U-Boot reads both board headers and
  `device.id`, selects the device tree / overlays by the boardname pair, and
  publishes device identity into the DT (e.g. a `/firmware/jethome` node, the
  way RPi HAT publishes `/hat`) and into environment variables as a cache.
- The U-Boot environment is never a storage of identity.
- No kernel parser is required: userspace reads `/proc/device-tree`.

## Production and repair

- Board test stage writes and signs the board header (unchanged workflow).
- Final assembly writes `device.id` (a normal `EEPROM_AddFile` + signature from
  the signature service).
- Repair: replacing the SoM does not touch device identity; replacing the
  mainboard re-provisions a single 192-byte record (signature service needs an
  API to re-sign a record for an existing device serial).
- **Legacy fallback** (documented, not stored): for devices without `device.id`,
  device serial = mainboard serial until the record is provisioned. The fleet
  migrates without header reflashes.

## Open questions (for RFC #26)

- Exact signature coverage bytes and signing procedure for the record.
- Whether `mac_pool_base`/`mac_pool_count` should also cover per-board MACs or
  strictly device-level pools.
- Whether `flags` should encode "identity locked" / provisioning state.
- Interaction with the `0xFF is empty` rule
  ([#14](https://github.com/jethome-iot/jeefs/issues/14)) for unwritten records.
