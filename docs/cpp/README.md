# JEEFS — C++ Libraries

Two header-only C++17 wrappers over the compiled C libraries:

- **`jeefs_headerpp.hpp`** — header parsing: `HeaderView` (non-owning, read-only) and `HeaderBuffer` (owning, mutable) over `jeefs_header`. No I/O.
- **`jeefspp.hpp`** — file system operations: `jeefs::FileSystem` (RAII) over the `jeefs` FS library. See the section at the end.

## Integration

### As a CMake subdirectory

```cmake
add_subdirectory(path/to/jeefs)
target_link_libraries(your_target jeefs_headerpp)
target_compile_features(your_target PRIVATE cxx_std_17)
```

`jeefs_headerpp` is an INTERFACE library that bundles `jeefs_header` and `ZLIB::ZLIB` — no need to link them manually.

### Header

```cpp
#include "jeefs_headerpp.hpp"
```

All classes are in the `jeefs` namespace.

## Dependencies

- **zlib** — CRC32 calculation
- **C++17** compiler
- C `jeefs_header` library (linked automatically via CMake)

## Building

```bash
mkdir -p build && cd build
cmake ..
cmake --build . --target example_cpp_read_header
```

## API

### `jeefs::HeaderView` — read-only, non-owning

```cpp
// From raw pointer
jeefs::HeaderView view(data_ptr, size);

// From std::vector
std::vector<uint8_t> buf = ...;
jeefs::HeaderView view(buf);
```

Methods:

| Method | Return type | Description |
|--------|-------------|-------------|
| `detect_version()` | `std::optional<int>` | Header version (1/2/3) or `nullopt` |
| `header_size()` | `int` | Expected size for detected version |
| `verify_crc()` | `bool` | CRC32 validity |
| `boardname()` | `std::string_view` | Board name |
| `boardversion()` | `std::string_view` | Board version |
| `serial()` | `std::string_view` | Serial number |
| `usid()` | `std::string_view` | USID |
| `cpuid()` | `std::string_view` | CPU ID |
| `mac()` | `const uint8_t*` | 6-byte MAC pointer (or `nullptr`) |
| `as_v1()` / `as_v2()` / `as_v3()` | `const JEEPROMHeadervN&` | Direct struct access (see safety note) |

**Safety note:** Before calling `as_v1()` / `as_v2()` / `as_v3()`, ensure the buffer is at least `header_size()` bytes for the detected version. These accessors perform `reinterpret_cast` on the internal pointer — calling them on a truncated buffer causes out-of-bounds reads.

### `jeefs::HeaderBuffer` — owning, mutable

```cpp
// Create new header for version 3
jeefs::HeaderBuffer buf(3);

// From existing data (copies)
jeefs::HeaderBuffer buf(data_ptr, size);
jeefs::HeaderBuffer buf(vec);
```

Methods:

| Method | Return type | Description |
|--------|-------------|-------------|
| `update_crc()` | `bool` | Recalculate and write CRC32 |
| `view()` | `HeaderView` | Get read-only view |
| `as_v1()` / `as_v2()` / `as_v3()` | `JEEPROMHeadervN&` | Mutable struct access |
| `data()` | `uint8_t*` | Raw mutable data pointer |
| `size()` | `size_t` | Buffer size |
| `valid()` | `bool` | Non-empty buffer check |

### Example: modify a header

```cpp
jeefs::HeaderBuffer buf(3);
auto& hdr = buf.as_v3();
strncpy(hdr.boardname, "MyBoard", sizeof(hdr.boardname));
hdr.mac[0] = 0xAA; hdr.mac[1] = 0xBB; hdr.mac[2] = 0xCC;
hdr.mac[3] = 0xDD; hdr.mac[4] = 0xEE; hdr.mac[5] = 0xFF;
buf.update_crc();
// buf.data() is now a valid 256-byte header
```

## Usage example

See [examples/cpp/read_header.cpp](../../examples/cpp/read_header.cpp) — reads an EEPROM binary, prints version, CRC status, board name, and MAC address.

## Format specification

- [Header common properties](../format/header-common.md)
- [Header v1](../format/header-v1.md) / [v2](../format/header-v2.md) / [v3](../format/header-v3.md)
- [Full format spec](../../EEPROM_FORMAT.md)

## `jeefs::FileSystem` — file system operations (jeefspp.hpp)

Header-only RAII wrapper over the C FS API (`jeefs.h`). Non-copyable,
movable; the EEPROM is closed by the destructor.

### Integration

```cmake
target_link_libraries(your_target jeefspp)
target_compile_features(your_target PRIVATE cxx_std_17)
```

`jeefspp` is an INTERFACE library that bundles the compiled `jeefs` FS
library (and its eepromops backend). Combine with `jeefs_headerpp` to
parse the header bytes returned by `readHeader()`.

### API

| Method | Return type | Description |
|--------|-------------|-------------|
| `FileSystem(path, size)` | — | Open the EEPROM (RAII) |
| `valid()` | `bool` | Open succeeded |
| `format(version)` | `int` | 0 on success |
| `listFiles(max = 64)` | `std::optional<std::vector<std::string>>` | File names |
| `readFile(name)` | `std::optional<std::vector<uint8_t>>` | File contents |
| `addFile(name, data)` | `int16_t` | Written bytes, `<=0` on error |
| `writeFile(name, data)` | `int16_t` | Written bytes, `FILENOTFOUND` if missing |
| `deleteFile(name)` | `int16_t` | 1 deleted, `FILENOTFOUND` if missing |
| `checkConsistency()` | `int16_t` | 1 consistent, 0 inconsistent |
| `readHeader()` | `std::optional<std::vector<uint8_t>>` | Header bytes, exactly header-sized |
| `setHeader(ptr)` | `int` | 0 on success |
| `lastError()` | `int` | Last non-positive C return code |
| `descriptor()` | `EEPROMDescriptor` | Escape hatch to the C API |

Error codes are `EEPROMError` values from `eepromerr.h`.

### Notes

- The wrapper passes C return values through unchanged; contracts match
  jeefs.h since the FS-core rewrite
  ([#9](https://github.com/jethome-iot/jeefs/issues/9)).
- Header parsing is not duplicated here: feed `readHeader()` bytes to
  `jeefs::HeaderView`.

```cpp
#include "jeefspp.hpp"
#include "jeefs_headerpp.hpp"

jeefs::FileSystem fs("/dev/eeprom.bin", 8192);
if (!fs.valid())
    return 1;
if (auto hdr = fs.readHeader()) {
    jeefs::HeaderView view(*hdr);
    printf("board: %.*s\n", (int)view.boardname().size(), view.boardname().data());
}
if (auto data = fs.readFile("config")) {
    // use *data
}
```
