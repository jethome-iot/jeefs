# EEPROM Format Specification

> **NOTE:** The canonical machine-parseable specifications are in [`docs/format/`](docs/format/).
> These are used by the code generator (`tools/jeefs_codegen/`) to produce C structs and Python constants.
> This file is maintained as a human-readable overview.
>
> - [Header v3 (256B, current)](docs/format/header-v3.md)
> - [Header v2 (256B, legacy)](docs/format/header-v2.md)
> - [Header v1 (512B, legacy)](docs/format/header-v1.md)
> - [Common properties, constants, enums](docs/format/header-common.md)
> - [Filesystem v1 (file storage format)](docs/format/filesystem-v1.md)

## Overview

This is the **canonical specification** for JEEFS EEPROM header formats. All language implementations (C, Python, Rust, Go, TypeScript) must conform to this document.

CPU board EEPROM uses a structured header followed by optional file system data. Three header versions exist; **v3** is the current production format.

### Common Properties (All Versions)

- **Byte order:** All multi-byte fields are **little-endian**
- **Magic:** `"JETHOME\0"` = bytes `4A 45 54 48 4F 4D 45 00` (8 bytes, null-terminated)
- **CRC32:** IEEE 802.3 polynomial `0xEDB88320` (same as zlib `crc32()`)
- **Packing:** No padding — all structs use `#pragma pack(push, 1)` / `__attribute__((packed))`
- **Empty bytes:** Both `0x00` and `0xFF` are treated as "empty"
- **String fields:** Null-terminated UTF-8, zero-padded to field size

---

## JEEPROMHeaderv3 (256 bytes) — Current

| Offset    | Size  | Field             | Description                                    |
|-----------|-------|-------------------|------------------------------------------------|
| 0-7       | 8     | magic             | "JETHOME\0" (null-terminated string)           |
| 8         | 1     | version           | Version = 3                                    |
| 9         | 1     | signature_version | Signature algorithm (see table below)          |
| 10-11     | 2     | header_reserved   | Reserved (zeros)                               |
| 12-43     | 32    | boardname         | Board name (e.g. "JXD-CPU-E1ETH")              |
| 44-75     | 32    | boardversion      | Board version (e.g. "1.3")                     |
| 76-107    | 32    | serial            | Device serial number                           |
| 108-139   | 32    | usid              | CPU eFuse USID                                 |
| 140-171   | 32    | cpuid             | CPU ID / factory MAC                           |
| 172-177   | 6     | mac               | MAC address (6 raw bytes)                      |
| 178-179   | 2     | reserved2         | Reserved for extended MAC                      |
| 180-243   | 64    | signature         | ECDSA signature (size depends on algorithm)    |
| 244-251   | 8     | timestamp         | Unix timestamp (int64, little-endian)          |
| 252-255   | 4     | crc32             | CRC32 of bytes 0-251 (little-endian uint32)    |

## Signature Storage

**Device signature is now stored in EEPROM** (not in eFUSE BLK1+BLK2).

This provides several benefits:
- **Unified programming**: Identity and signature programmed in single step
- **Freed eFUSE blocks**: BLK1+BLK2 now available for future use
- **Simpler workflow**: No separate signature burn step

### Signature Algorithms

| Value | Algorithm          | Curve     | Signature Size | Notes                      |
|-------|--------------------|-----------|----------------|----------------------------|
| 0     | None               | -         | 0 bytes        | No signature               |
| 1     | ECDSA secp192r1    | NIST P-192| 48 bytes (r||s)| Legacy, 96-bit security    |
| 2     | ECDSA secp256r1    | NIST P-256| 64 bytes (r||s)| Current, 128-bit security  |

### Signature Field Layout

- **Offset 9**: Signature algorithm version (see table above)
- **Offset 180-243**: Signature field (64 bytes total)
  - **secp192r1 (v1)**: bytes 180-227 used (48 bytes), bytes 228-243 are zeros
  - **secp256r1 (v2)**: bytes 180-243 fully used (64 bytes)

### CRC32 Coverage

CRC32 covers bytes **0-251**, which includes the signature and timestamp.
This ensures any corruption in the signature area is detected.

## Version History

### Version 3 (Current) - v2-based layout with signature

- Based on v2 layout for field sizes
- **signature**: 64 bytes at offset 180 (replaces reserved3)
- **signature_version**: 1 byte at offset 9 (replaces reserved1[0])
- **timestamp**: int64 LE at offset 244
- **CRC32**: covers bytes 0-251

---

## JEEPROMHeaderv2 (256 bytes)

| Offset    | Size  | Field             | Endianness    | Description                          |
|-----------|-------|-------------------|---------------|--------------------------------------|
| 0-7       | 8     | magic             | -             | "JETHOME\0"                          |
| 8         | 1     | version           | -             | Version = 2                          |
| 9-11      | 3     | reserved1         | -             | Reserved (zeros)                     |
| 12-43     | 32    | boardname         | -             | Board name, null-terminated          |
| 44-75     | 32    | boardversion      | -             | Board version, null-terminated       |
| 76-107    | 32    | serial            | -             | Device serial number                 |
| 108-139   | 32    | usid              | -             | CPU eFuse USID                       |
| 140-171   | 32    | cpuid             | -             | CPU ID                               |
| 172-177   | 6     | mac               | -             | MAC address (6 raw bytes)            |
| 178-179   | 2     | reserved2         | -             | Reserved for extended MAC             |
| 180-251   | 72    | reserved3         | -             | Reserved for future use              |
| 252-255   | 4     | crc32             | little-endian | CRC32 of bytes 0-251                 |

## JEEPROMHeaderv1 (512 bytes) — Legacy

| Offset    | Size  | Field             | Endianness    | Description                          |
|-----------|-------|-------------------|---------------|--------------------------------------|
| 0-7       | 8     | magic             | -             | "JETHOME\0"                          |
| 8         | 1     | version           | -             | Version = 1                          |
| 9-11      | 3     | reserved1         | -             | Reserved (zeros)                     |
| 12-43     | 32    | boardname         | -             | Board name, null-terminated          |
| 44-75     | 32    | boardversion      | -             | Board version, null-terminated       |
| 76-107    | 32    | serial            | -             | Device serial number                 |
| 108-139   | 32    | usid              | -             | CPU eFuse USID                       |
| 140-171   | 32    | cpuid             | -             | CPU ID                               |
| 172-177   | 6     | mac               | -             | MAC address (6 raw bytes)            |
| 178-179   | 2     | reserved2         | -             | Reserved for extended MAC             |
| 180-211   | 32    | modules           | little-endian | 16 x uint16_t module IDs             |
| 212-507   | 296   | reserved3         | -             | Reserved for future use              |
| 508-511   | 4     | crc32             | little-endian | CRC32 of bytes 0-507                 |

## Data Sources

### Board Information

**boardname** and **boardversion** are extracted from device module composition:

- Look for module with type `"cpu"` or `"mainboard"` in device modules
- Format: `"{series_name}-{model_name}"` (e.g. "JXD-CPU-E1ETH")
- Version from module `revision` field (e.g. "1.3")

### Device Identifiers

All device identifiers come from test context:

- **serial**: Assigned during testing (from batch allocation)
- **usid**: Unique System ID (from signature service)
- **cpuid**: CPU ID / factory MAC (read from device)
- **mac**: Primary MAC address (programmed in eFuse)
- **signature**: ECDSA signature (from signature service CSV/API)
- **timestamp**: Current Unix time when EEPROM is programmed

## Testing Workflow

EEPROM programming includes signature and happens **AFTER** identifiers are available:

```
1. ReadCPUIDTest
   ↓ Reads CPUID from device

2. AllocateIdentifiers
   ↓ Gets serial, USID, MACs, signature from service

3. ProgramEFuseTest
   ↓ Programs custom MAC to eFuse (signature NOT in eFuse!)

4. ESP32ProgramEEPROM
   ↓ Programs EEPROM with complete identity + signature
```

**Note**: Signature is now in EEPROM, not eFUSE. BLK1+BLK2 are free.

## Programming Method: Partition Flash

### Concept

Specialized **eeprom-programmer** firmware reads EEPROM data from a flash partition:

```
┌─────────────────────────────────────────┐
│  ESP32 Flash Memory                      │
├─────────────────────────────────────────┤
│  0x00000  Bootloader                     │
│  0x10000  eeprom-programmer firmware     │
│  0x3F0000 eeprom_data partition (4KB)    │  ← EEPROM header + signature
└─────────────────────────────────────────┘
           ↓
      I2C EEPROM (256 bytes)
```

### Performance

**First device in session:** ~7-8 minutes (builds firmware)
**Subsequent devices:** ~3 seconds (only flash data partition)

## Implementation

### C Structure (eeprom_gen.c)

```c
typedef struct __attribute__((packed)) {
    char     magic[8];               // 8 bytes  (offset 0-7)
    uint8_t  version;                // 1 byte   (offset 8)       = 3
    uint8_t  signature_version;      // 1 byte   (offset 9)
    uint8_t  header_reserved[2];     // 2 bytes  (offset 10-11)
    char     boardname[32];          // 32 bytes (offset 12-43)
    char     boardversion[32];       // 32 bytes (offset 44-75)
    uint8_t  serial[32];             // 32 bytes (offset 76-107)
    uint8_t  usid[32];               // 32 bytes (offset 108-139)
    uint8_t  cpuid[32];              // 32 bytes (offset 140-171)
    uint8_t  mac[6];                 // 6 bytes  (offset 172-177)
    uint8_t  reserved2[2];           // 2 bytes  (offset 178-179)
    uint8_t  signature[64];          // 64 bytes (offset 180-243)
    int64_t  timestamp;              // 8 bytes  (offset 244-251)
    uint32_t crc32;                  // 4 bytes  (offset 252-255)
} JEEPROMHeaderv3; // sizeof = 256 bytes
```

### Python Library

File: `shared/common-lib/testsystem_common/eeprom/header.py`

```python
from testsystem_common.eeprom import EEPROMHeaderV3, SignatureAlgorithm

header = EEPROMHeaderV3(
    boardname="JXD-CPU-E1ETH",
    boardversion="1.3",
    serial="SN-2024-001",
    usid="1234567890ABCDEF",
    cpuid="AA:BB:CC:DD:EE:FF",
    mac="F0:57:8D:01:00:00",
    signature=sig_bytes,
    signature_algorithm=SignatureAlgorithm.SECP256R1,
)
binary = header.to_bytes()       # 256-byte header
image = header.to_partition_image()  # 4KB partition image
parsed = EEPROMHeaderV3.from_bytes(binary)  # Parse back
```

### CRC32 Calculation

CRC32 is calculated over bytes 0-251 (includes signature and timestamp):

```python
import binascii
import struct

# Calculate CRC32 of bytes 0-251
crc32_value = binascii.crc32(bytes(eeprom[0:252])) & 0xFFFFFFFF

# Pack as little-endian uint32 at offset 252
struct.pack_into("<I", eeprom, 252, crc32_value)
```

## Verification

After EEPROM programming, device firmware should:

1. Read EEPROM header
2. Verify magic = "JETHOME\0"
3. Verify version == 3
4. Calculate CRC32 of bytes 0-251
5. Compare with stored CRC32 at offset 252
6. Read signature_version at offset 9
7. Read signature at offset 180 (48 bytes for v1, 64 bytes for v2)

If CRC32 mismatch → EEPROM corrupted or not programmed

## Cross-Verification

C and Python implementations are verified to produce identical output:

```bash
# Build C shared library
gcc -shared -fPIC -o tests/libeeprom_gen.so tests/eeprom_gen.c

# Run Python tests (includes cross-verification with C)
pytest tests/test_eeprom_header.py -v
```
