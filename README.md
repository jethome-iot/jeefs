# JEEFS — JetHome EEPROM filesystem library

Board identity headers and a tiny linked-list file system for ~8 KiB
onboard EEPROMs, as used across JetHome devices. Dual-licensed
GPL-2.0-or-later / Apache-2.0.

## The model

- **Board identity** lives in a 256-byte header at offset 0 —
  [header v4](docs/format/header-v4.md) is current (board-scoped:
  boardname, board_serial, usid/cpuid, MAC, ECDSA signature);
  v3 is the fielded production format, v1/v2 are obsolete but parseable.
- **Device identity** (model, serial, hardware revision of the *product*)
  is the signed 256-byte [DeviceIdentityV1](docs/format/device-identity-v1.md)
  record, stored as the reserved file `device.id`.
- After the header, files form a singly-linked list —
  [filesystem v1](docs/format/filesystem-v1.md).
- **The library performs no I/O**: the environment (U-Boot, the kernel,
  userspace tooling) reads the EEPROM itself and hands every operation a
  caller-owned image buffer. Bounded-RAM targets can locate and
  stream-verify single files through the pull-model walker
  (`jeefs_walk.h`) instead of buffering the image. See
  [docs/PORTING.md](docs/PORTING.md) for embedding into U-Boot / Linux /
  MCU firmware — the core is freestanding, heap-free and dependency-free
  (built-in CRC32 by default).

## Implementations

| Language | Where | Install |
|----------|-------|---------|
| C (headers + record + FS + walker) | `src/`, `include/` | CMake package `jeefs` (`jeefs::header`, `jeefs::fs`) or pkg-config |
| C++17 (header-only wrappers) | `include/*.hpp` | ships with the C package |
| Python (headers + record + image build/parse) | `python/` | `pip install jeefs` |
| Rust (headers + record + FS, all `no_std`; image build/parse under `alloc`) | `rust/jeefs-header/` | `cargo add jeefs-header` |

All ports conform to the
[implementation contract](docs/IMPLEMENTATION_CONTRACT.md) and are locked
together by a cross-language test matrix, golden images and committed
binary vectors. The machine-readable format specs in
[docs/format/](docs/format/) are the single source of truth — C, Python
and Rust structures are generated from them
([policy](docs/CODEGEN.md)).

## Building

```bash
cmake -B build && cmake --build build
ctest --test-dir build            # full suite incl. the cross-language matrix
```

See [CLAUDE.md](CLAUDE.md) for the full development reference and
[docs/ROADMAP.md](docs/ROADMAP.md) for direction and freeze criteria.
