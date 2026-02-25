# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

JEEFS (JetHome EEPROM File System) — a library for working with a simple linked-list filesystem on small EEPROMs (target: 64Kbit / 8KB). Dual-licensed GPL-2.0+ / Apache-2.0.

The long-term goal is a **universal multi-language library** (C/C++/Python/Rust/Go/TypeScript) with two areas:

1. **Header parsing/manipulation** (priority) — native implementations per language, `EEPROM_FORMAT.md` as source of truth, shared binary test vectors
2. **File system operations** — C/C++ only, CRUD for files stored as a linked list after the header

### Multi-language strategy

Header parsing uses **native implementations per language** (not FFI). Rationale: the header is 256 bytes / ~13 fields — the parsing logic (~80 lines) is comparable in size to an FFI wrapper, and native packages are trivial to deploy (`pip install`, `cargo add`, `go get`). `EEPROM_FORMAT.md` is the canonical spec; shared binary test vectors in `test-vectors/` ensure cross-language consistency.

FS operations stay in C/C++ — more complex, I/O-dependent, only needed on embedded targets.

## Build & Test

### C

```bash
# Full build
mkdir -p build && cd build && cmake .. && cmake --build .

# Run all tests (requires eeprom.bin)
mkdir -p build/tests/common
dd if=/dev/zero of=build/tests/common/eeprom.bin bs=1 count=8192
cd build && ctest

# Run a single test
cd build && ctest -R test_00    # format test (v1, v2, v3)
cd build && ctest -R test_01    # add files test
cd build && ctest -R test_02    # read file test

# Verbose test output
cd build && ctest --verbose
```

**Dependencies:** zlib (for CRC32), CMake 3.19+, C11, C++17.

### Python

```bash
cd python
uv venv .venv && uv pip install -e ".[test]"
.venv/bin/python -m pytest tests/ -v
```

**Formatting:** `clang-format` with LLVM base style, 120-column limit, 4-space indent. Config in `.clang-format`.

## Architecture

### Layered Design

```
┌───────────────────────────────────────────────┐
│  Python package (python/jeefs/)               │  Native header parsing
├───────────────────────────────────────────────┤
│  C++ wrapper (srcpp/jeefspp)                  │  std::vector-based API
├───────────────────────────────────────────────┤
│  Pure header API (jeefs_header.h/c)           │  Byte-buffer ops, no I/O
├───────────────────────────────────────────────┤
│  JEEFS core C API (src/jeefs.c)               │  File ops + header mgmt
├───────────────────────────────────────────────┤
│  eepromops interface (include/eepromops.h)     │  Abstract read/write
├───────────────────────────────────────────────┤
│  eepromops-memory backend (eepromops-memory/)  │  In-memory EEPROM sim
└───────────────────────────────────────────────┘
```

Two distinct APIs:

- **`jeefs_header.h`** — pure functions on byte buffers (detect version, verify/update CRC, init header). No eepromops dependency. Suitable for standalone use or integration.
- **`jeefs.h`** — full FS API (open/close EEPROM, format, list/read/write/add/delete files, header get/set). Depends on eepromops backend.

### EEPROM Binary Layout

```
Offset 0:
┌──────────────────────────────────────────────────┐
│ Header v1 (512B), v2 (256B), or v3 (256B)       │
│  magic "JETHOME\0" | version | boardname         │
│  boardversion | serial | usid | cpuid            │
│  mac[6] | [signature + timestamp (v3)] | crc32   │
├──────────────────────────────────────────────────┤
│ File1: FileHeader (24B) + data (N bytes)         │
│  name[16] | dataSize | crc32 |                   │
│  nextFileAddress → points to File2               │
├──────────────────────────────────────────────────┤
│ ...                                               │
│ FileN: nextFileAddress = 0 (end marker)          │
├──────────────────────────────────────────────────┤
│ Free space (0x00)                                 │
└──────────────────────────────────────────────────┘
```

All structures use `#pragma pack(push, 1)` — no padding. Addresses are `uint16_t` (max 64KB). Empty bytes are `0x00` or `0xFF` (both treated as empty). All multi-byte fields are **little-endian**. Magic is `4A 45 54 48 4F 4D 45 00` ("JETHOME\0") for all versions. CRC32 uses IEEE 802.3 polynomial (zlib `crc32()`).

See `EEPROM_FORMAT.md` for full field-by-field layout tables for all versions.

### Key Data Structures

**C (include/jeefs.h):**

- `JEEPROMHeaderv1` (512B) / `JEEPROMHeaderv2` (256B) / `JEEPROMHeaderv3` (256B) — device identity headers
- `JEEPROMHeader` — union of all header versions + `JEEPROMHeaderversion` (12-byte version-detect struct)
- `JEEFSSignatureAlgorithm` — enum: `JEEFS_SIG_NONE` (0), `JEEFS_SIG_SECP192R1` (1), `JEEFS_SIG_SECP256R1` (2)
- `JEEFSFileHeaderv1` (24B) — file entry: name (15 chars max), dataSize, CRC32, nextFileAddress
- `EEPROMDescriptor` — opaque handle (file descriptor + EEPROM size)

**Python (python/jeefs/):**

- `EEPROMHeaderV3` — dataclass with `to_bytes()`, `from_bytes()`, `verify_crc()`, `validate()`, `to_partition_image()`
- `SignatureAlgorithm` — IntEnum: `NONE` (0), `SECP192R1` (1), `SECP256R1` (2)
- Constants: `EEPROM_FIELDS` (offset/size dict), `EEPROM_MAGIC`, `SIGNATURE_SIZES`

### Error Codes (include/eepromerr.h)

Negative return values are errors defined in `EEPROMError` enum: `FILEEXISTS`, `FILENAMETOOLONG`, `FILENOTFOUND`, `NOTENOUGHSPACE`, `EEPROMCORRUPTED`, `EEPROMREADERROR`, etc.

### Known Issues / TODOs in Code

- `defragEEPROM()` is declared in the header but not implemented in jeefs.c
- Several `// TODO` markers for CRC validation of file data on read and write error checking
- Binary test vectors (`test-vectors/`) not yet generated — planned for cross-language CI
- Additional language implementations (Rust, Go, TypeScript) planned but not yet started

## CMake Options

| Option | Default | Description |
|---|---|---|
| `JEEFS_BUILD_TESTS` | ON | Build test executables |
| `JEEFS_USEDYNAMIC_FILES` | ON | Build shared library |
| `JEEFS_USE_EEPROMOPS_MEMORY` | ON | Use in-memory EEPROM backend |

## Repository Structure

```text
include/
  jeefs.h            # Core types, structs (v1/v2/v3), FS API declarations
  jeefs_header.h     # Pure header API (no I/O dependency)
  eepromops.h        # Hardware abstraction interface
  eepromerr.h        # Error codes
  debug.h            # Debug macros
src/
  jeefs.c            # FS implementation + header management (uses eepromops)
  jeefs_header.c     # Pure header functions (uses zlib only)
eepromops-memory/    # In-memory EEPROM backend
srcpp/               # C++ wrapper
python/
  jeefs/             # Python package: constants.py, header.py
  tests/             # pytest suite (58 tests)
  pyproject.toml     # Package config (jeefs)
tests/               # C test suite (ctest)
EEPROM_FORMAT.md     # Canonical binary format specification
```

## CI/CD

- GitHub Actions workflows MUST use `runs-on: self-hosted` — never `ubuntu-latest` or other GitHub-hosted runners.

## Language/Style Notes

- C11 for core library, C++17 for wrapper
- All public headers have `extern "C"` guards for C++ compatibility
- Debug output controlled by `-DDEBUG` compile flag (uses `debug()` macro from `include/debug.h`)
- SPDX license identifiers on all source files
- Python: PEP 8, type hints, dataclasses
