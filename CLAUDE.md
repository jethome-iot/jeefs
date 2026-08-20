# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

JEEFS (JetHome EEPROM File System) — a library for working with a simple linked-list filesystem on small EEPROMs (target: 64Kbit / 8KB). Dual-licensed GPL-2.0+ / Apache-2.0.

The long-term goal is a **universal multi-language library** (C/C++/Python/Rust/Go/TypeScript) with two areas:

1. **Header parsing/manipulation** (priority) — native implementations per language, `docs/format/*.md` as source of truth, shared binary test vectors
2. **File system operations** — C/C++ only, CRUD for files stored as a linked list after the header

### Code generation policy

The build never runs the generator: `include/jeefs_generated.h`,
`python/jeefs/constants_generated.py` and `rust/jeefs-header/src/generated.rs`
are committed, reviewed files. `docs/format/*.md` is the canonical format
description; CI (`codegen-check`) and the prek hook reject any drift between
the two. **Never edit generated files by hand; never change the format outside
`docs/format/*.md`.** Local hooks: `brew install prek && prek install`
(one time per clone). Full policy: `docs/CODEGEN.md`.

### Multi-language strategy

Header parsing uses **native implementations per language** (not FFI). Rationale: the header is 256 bytes / ~13 fields — the parsing logic (~80 lines) is comparable in size to an FFI wrapper, and native packages are trivial to deploy (`pip install`, `cargo add`, `go get`). `docs/format/*.md` is the canonical spec (`EEPROM_FORMAT.md` is a human-readable overview); shared binary test vectors in `test-vectors/` ensure cross-language consistency.

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
│  C++ wrappers (jeefspp, jeefs_headerpp)       │  header-only; FileSystem owns the image
├───────────────────────────────────────────────┤
│  Pure header/record API (jeefs_header, devid) │  Byte-buffer ops, no I/O
├───────────────────────────────────────────────┤
│  JEEFS FS core (src/jeefs.c)                  │  File ops over a caller image buffer
└───────────────────────────────────────────────┘
The library performs no I/O (#25 variant A): the environment reads the
EEPROM itself and hands every API a `uint8_t *image, uint16_t size`.
```

Two distinct APIs:

- **`jeefs_header.h`** — pure functions on byte buffers (detect version, verify/update CRC, init header). No I/O dependency. Suitable for standalone use or integration.
- **`jeefs.h`** — FS API over a caller-owned image buffer (format, list/read/write/add/delete files, header get/set). No I/O, no descriptors.
- **`jeefs_walk.h`** — pull-model file locator for bounded-RAM targets: the environment feeds 28-byte header windows, the walker owns the state machine and validation; data streams verify via `jeefs_crc32_update` (port layer).

### EEPROM Binary Layout

```
Offset 0:
┌──────────────────────────────────────────────────┐
│ Header v1 (512B) or v2/v3/v4 (256B)             │
│  magic "JETHOME\0" | version | fs_version        │
│  boardname | boardversion | serial | usid        │
│  cpuid | mac[6] | [signature + timestamp] | crc32│
├──────────────────────────────────────────────────┤
│ File1: FileHeader (28B) + data (N bytes)         │
│  name[16] | dataSize | crc32 |                   │
│  nextFileAddress → File2 | headerCrc32           │
├──────────────────────────────────────────────────┤
│ ...                                               │
│ FileN: nextFileAddress = 0 (end marker)          │
├──────────────────────────────────────────────────┤
│ Free space (0x00)                                 │
└──────────────────────────────────────────────────┘
```

All structures use `#pragma pack(push, 1)` — no padding. Addresses are `uint16_t` (max 64KB). Empty bytes are `0x00` or `0xFF` (both treated as empty). All multi-byte fields are **little-endian**. Magic is `4A 45 54 48 4F 4D 45 00` ("JETHOME\0") for all versions. CRC32 uses IEEE 802.3 polynomial (zlib `crc32()`).

See `docs/format/*.md` (canonical) and `EEPROM_FORMAT.md` (overview) for field-by-field layout tables.

### Key Data Structures

**C (include/jeefs.h):**

- `JEEPROMHeaderv1` (512B) / `JEEPROMHeaderv2` / `JEEPROMHeaderv3` / `JEEPROMHeaderv4` (256B) — board identity headers (device identity = `DeviceIdentityV1`, file `device.id`)
- `JEEPROMHeader` — union of all header versions + `JEEPROMHeaderversion` (12-byte version-detect struct)
- `JEEFSSignatureAlgorithm` — enum: `JEEFS_SIG_NONE` (0), `JEEFS_SIG_SECP192R1` (1), `JEEFS_SIG_SECP256R1` (2)
- `JEEFSFileHeaderv1` (28B) — file entry: name (15 chars max), dataSize, data CRC32, nextFileAddress, headerCrc32; gated by the `fs_version` byte at header offset 10 (0 = no filesystem, 1 = current)

**Python (python/jeefs/):**

- `EEPROMHeaderV3` — dataclass with `to_bytes()`, `from_bytes()`, `verify_crc()`, `validate()`, `to_partition_image()`
- `SignatureAlgorithm` — IntEnum: `NONE` (0), `SECP192R1` (1), `SECP256R1` (2)
- Constants: `EEPROM_FIELDS` (v4, current) / `EEPROM_FIELDS_V3` etc. (offset/size dicts), `EEPROM_MAGIC`, `SIGNATURE_SIZES`

### Error Codes (include/eepromerr.h)

Negative return values are errors defined in `EEPROMError` enum: `FILEEXISTS`, `FILENAMETOOLONG`, `FILENOTFOUND`, `NOTENOUGHSPACE`, `EEPROMCORRUPTED`, `EEPROMREADERROR`, etc.

### Known Issues / TODOs in Code

- Go and TypeScript implementations planned, not yet started (C, C++, Python and Rust are live with an NxN cross-language test matrix)
- Header v4 (board-scoped) and the DeviceIdentityV1 record have generated structs; full parser/tooling coverage is tracked in issues #58 and #60

## CMake Options

| Option | Default | Description |
|---|---|---|
| `JEEFS_BUILD_TESTS` | ON | Build test executables |
| `JEEFS_USEDYNAMIC_FILES` | ON | Build shared library |

## Repository Structure

```text
include/
  jeefs.h            # Core types, structs (v1/v2/v3), FS API declarations
  jeefs_header.h     # Pure header API (no I/O dependency)
  eepromerr.h        # Error codes
  debug.h            # Debug macros
src/
  jeefs.c            # FS implementation over a caller image buffer
  jeefs_header.c     # Pure header functions (uses zlib only)
python/
  jeefs/             # Python package: constants.py, header.py
  tests/             # pytest suite (62 tests)
  pyproject.toml     # Package config (jeefs)
tests/               # C test suite (ctest)
docs/format/         # Canonical machine-readable format specs (codegen source):
                     #   header v1-v4, device-identity-v1, filesystem-v1, common
docs/CODEGEN.md      # Generated-files policy
docs/IMPLEMENTATION_CONTRACT.md  # Per-language behavioral contract + conformance suite
tools/jeefs_codegen/ # Spec parser + C/Python/Rust generators
EEPROM_FORMAT.md     # Human-readable format overview
```

## CI/CD

- GitHub Actions workflows MUST use `runs-on: self-hosted` — never `ubuntu-latest` or other GitHub-hosted runners.

## Language/Style Notes

- C11 for core library, C++17 for wrapper
- All public headers have `extern "C"` guards for C++ compatibility
- Debug output controlled by `-DDEBUG` compile flag (uses `debug()` macro from `include/debug.h`)
- SPDX license identifiers on all source files
- Python: PEP 8, type hints, dataclasses
