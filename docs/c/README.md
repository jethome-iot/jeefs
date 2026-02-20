# JEEFS Header — C Library

Pure C11 library for parsing JEEFS EEPROM headers. Operates on byte buffers, no I/O dependency.

## Integration

### As a CMake subdirectory

```cmake
add_subdirectory(path/to/jeefs)
target_link_libraries(your_target jeefs_header)
```

The `jeefs_header` static library is built from `src/jeefs_header.c` and depends only on zlib (for CRC32).

### Manual compilation

```bash
cc -c src/jeefs_header.c -Iinclude -o jeefs_header.o
cc your_app.c jeefs_header.o -lz -o your_app
```

### Headers

```c
#include "jeefs_generated.h"  // Structs, constants, enums
#include "jeefs_header.h"     // API functions
```

## Dependencies

- **zlib** — CRC32 calculation (`crc32()`)
- **C11** compiler

## Building

```bash
mkdir -p build && cd build
cmake ..
cmake --build . --target jeefs_header          # library only
cmake --build . --target example_c_read_header # example
```

## API

### Version detection

```c
int jeefs_header_detect_version(const uint8_t *data, size_t len);
```

Returns header version (1, 2, or 3) or -1 on error. Needs at least 12 bytes.

### Header size

```c
int jeefs_header_size(int version);
```

Returns header size in bytes: 512 (v1), 256 (v2/v3), or -1 for unknown version.

### CRC verification

```c
int jeefs_header_verify_crc(const uint8_t *data, size_t len);
```

Returns 0 if CRC32 is valid, -1 on error.

### CRC update

```c
int jeefs_header_update_crc(uint8_t *data, size_t len);
```

Recalculates and writes CRC32 in place. Returns 0 on success.

### Header initialization

```c
int jeefs_header_init(uint8_t *data, size_t len, int version);
```

Zeros the buffer, sets magic + version, computes CRC. Returns 0 on success.

### Struct access

Cast the byte buffer to the appropriate struct after checking the version **and verifying the buffer is large enough**:

```c
int version = jeefs_header_detect_version(buf, buf_len);
int hdr_size = jeefs_header_size(version);
if (hdr_size < 0 || (int)buf_len < hdr_size) {
    /* buffer too short for this version */
    return -1;
}
const JEEPROMHeaderv3 *hdr = (const JEEPROMHeaderv3 *)buf;
printf("Board: %s\n", hdr->boardname);
printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
       hdr->mac[0], hdr->mac[1], hdr->mac[2],
       hdr->mac[3], hdr->mac[4], hdr->mac[5]);
```

**Important:** Always verify `buf_len >= jeefs_header_size(version)` before casting to a struct pointer. Version detection only needs 12 bytes, but struct access reads up to 512 bytes (v1) or 256 bytes (v2/v3).

## Usage example

See [examples/c/read_header.c](../../examples/c/read_header.c) — reads an EEPROM binary, prints version, CRC status, board name, and MAC address.

## Structs and constants

All structs are packed (`#pragma pack(push, 1)`) and defined in `include/jeefs_generated.h`:

| Struct | Size | Description |
|--------|------|-------------|
| `JEEPROMHeaderversion` | 12B | Version detection (magic + version byte) |
| `JEEPROMHeaderv1` | 512B | Full v1 header |
| `JEEPROMHeaderv2` | 256B | Full v2 header |
| `JEEPROMHeaderv3` | 256B | Full v3 header (with signature + timestamp) |
| `JEEPROMHeader` | union | Version-agnostic union of all headers |

## Format specification

- [Header common properties](../format/header-common.md)
- [Header v1](../format/header-v1.md)
- [Header v2](../format/header-v2.md)
- [Header v3](../format/header-v3.md)
- [Filesystem v1](../format/filesystem-v1.md)
- [Full format spec](../../EEPROM_FORMAT.md)
