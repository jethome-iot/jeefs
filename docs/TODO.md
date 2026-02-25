# JEEFS — Roadmap / TODO

## v0.1.3 — Code quality

### Proper error codes — low complexity, ~30 LOC

Add `EEPROMWRITEERROR = -12` to `eepromerr.h`. Replace ~10 bare `-1`
returns in `jeefs.c` with specific `EEPROMError` values (there are 6
occurrences marked `@TODO: errno`). Do this first — other tasks depend
on having proper error codes.

Files: `include/eepromerr.h`, `src/jeefs.c`

### Write error propagation — low complexity, ~5 LOC

In `eepromops-memory.c`, check `eeprom_save()` return value for `-1`
and propagate the error instead of silently continuing. Currently a
failed save-on-write leaves the caller unaware of data loss.

Files: `eepromops-memory/eepromops-memory.c`

### CRC32 validation on file read — low complexity, ~10 LOC

After reading file data in `EEPROM_ReadFile()`, compute CRC32 and
compare against the value stored in the file header. Return
`EEPROMCORRUPTED` on mismatch. Depends on the error codes task.

Files: `src/jeefs.c`

### Extract defragEEPROM() — medium complexity, ~80-100 LOC

`EEPROM_DeleteFile()` currently performs inline defragmentation
(shifts all subsequent files after unlinking). Extract the compaction
logic into a standalone `defragEEPROM()` function declared in
`jeefs.h`. `EEPROM_DeleteFile()` calls `defragEEPROM()` at the end —
existing behavior is preserved, but the function becomes available
for standalone use.

Files: `src/jeefs.c`, `include/jeefs.h`

### Expanded test vectors — low complexity

Add ~6 new vectors to `test-vectors/vectors/`. The CMake cross-language
matrix auto-discovers new vectors — no code changes required.

Candidates:

- Maximum-length field values (31-char boardname, 32-char serial, etc.)
- All-zero and all-FF MAC addresses
- v3 with SECP192R1 signature
- v1/v2 with non-trivial USID/CPUID content

## v0.2.0 — Package publishing

### PyPI (`jeefs-header`) — low complexity

Add metadata to `python/pyproject.toml`: authors, repository, homepage,
keywords, classifiers. Build with `python -m build`, upload with
`twine upload`.

### crates.io (`jeefs-header`) — low complexity

Add metadata to `rust/jeefs-header/Cargo.toml`: repository,
documentation, homepage, keywords, categories. Publish with
`cargo publish`.

## Future — New language implementations

Not prioritized yet. Planned when C quality and publishing are done.

### Go

Native header parsing library (~600-800 LOC). Uses `crypto/crc32`
from stdlib and `encoding/binary` for little-endian parsing. Add Go
generator to `tools/jeefs_codegen/`. Cross-language matrix grows from
4x4 to 5x5.

### TypeScript

Native header parsing library (~700-900 LOC). Uses DataView API for
binary parsing. Requires pure-JS IEEE CRC32 implementation or npm
dependency (no built-in CRC32 in JavaScript). Add TypeScript generator
to `tools/jeefs_codegen/`. Cross-language matrix grows to 6x6.
