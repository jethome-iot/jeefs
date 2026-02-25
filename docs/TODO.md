# JEEFS — Roadmap / TODO

## C filesystem improvements

- **`defragEEPROM()`** — declared in `jeefs.h` but not implemented. After file deletions, freed space is not reclaimed; defragmentation is essential for 8 KB EEPROMs.
- **CRC32 validation on file read** — `EEPROM_ReadFile()` does not verify the stored data CRC. Corrupted file contents pass silently.
- **Proper error codes** — several internal functions return `-1` instead of specific `EEPROMError` enum values (6 occurrences marked `@TODO: errno` in `jeefs.c`).
- **Write error checking** — EEPROM write errors in `eepromops-memory` backend are not propagated to the caller.

## New language implementations

- **Go** — native header parsing library (header-only, no FFI), following the same pattern as Python and Rust.
- **TypeScript** — native header parsing library for Node.js / browser environments.

## Test vectors

Current set: 4 vectors (v1 minimal, v2 minimal, v3 no-sig, v3 secp256r1). Candidates for expansion:

- Maximum-length field values (31-char boardname, 32-char serial, etc.)
- All-zero and all-FF MAC addresses
- v3 with SECP192R1 signature
- v1/v2 with non-trivial field content (USID, CPUID)

## Publishing

- **PyPI** — `jeefs-header` Python package is ready but not published.
- **crates.io** — `jeefs-header` Rust crate is ready but not published.
