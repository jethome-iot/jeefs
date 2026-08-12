# JEEFS Roadmap

Strategic plan across releases. Tactical near-term items live in [TODO.md](TODO.md);
this document defines release themes, ordering and freeze criteria. Progress is
tracked in GitHub milestones and the audit tracking issue
([#30](https://github.com/jethome-iot/jeefs/issues/30)).

## Compatibility policy

- **Header formats v1–v3 are wire-stable forever**: every implementation must keep
  parsing all released header versions. New fields only via a new header version.
- **The FS layer carries no legacy constraints before v1.0**: the on-EEPROM file
  layout follows [format/filesystem-v1.md](format/filesystem-v1.md), but the C API,
  ABI and internal behavior may change freely between 0.x releases.
- Known consumers (jethome-iot/testsuite\*) are non-critical and migrate with the API.

## v0.2.0 — Trustworthy FS core

The FS layer is rewritten rather than patched; header pipeline stays.

1. FS-core rewrite around a single validated chain iterator (#9): fixes chain
   corruption on delete (#5), inverted `EEPROM_SetHeader` (#6), unbounded VLAs (#7),
   non-atomic `WriteFile`, phantom files, cycle hangs; unified `EEPROMError` codes.
2. Real C test suite (#18): per-test fixtures, asserts on every return code,
   negative tests (corrupted chain, cycle, out-of-bounds sizes).
3. Header API dedup and freestanding include chain (#10); little-endian access in
   the C core (#11).
4. C++ FS wrapper leaves the build until the core stabilizes (#8);
   `jeefs_headerpp.hpp` stays.
5. Codegen parser hardening (#12); CI: Python drift-check, sanitizers, format,
   `-Wvla` (#20); committed test vectors wired into CI (#19).
6. Spec decisions that block the v3 freeze: raw-vs-string field semantics (#13),
   `0xFF is empty` rule (#14).
7. Release automation: crates.io + GitHub Releases alongside PyPI (#21);
   Dependabot and supply-chain pinning (#22).
8. Documentation sync: CLAUDE.md / EEPROM_FORMAT.md cleanup (#27), Outline wiki
   mirror (#28).

## v0.3.0 — Embeddability (MCU / U-Boot / Linux kernel)

Ordering rule: the FS rewrite lands **before** the storage-contract change, so the
ABI break happens under a green regression net.

1. Port layer (#24): `jeefs_crc32` (built-in table / zlib / U-Boot / kernel
   mapping), LE accessors, overridable logging, no POSIX in public headers.
2. `eepromops` v2 (#25): ops table + `void *ctx` instead of a POSIX fd; optional
   lock/unlock hooks; documented "no heap, no VLA, bounded stack" contract.
3. Rust big-endian correctness (#15).
4. CMake install/export/pkg-config, `jeefs::header` (freestanding) and `jeefs::fs`
   targets (#23).
5. CI: arm-none-eabi smoke build, big-endian (qemu) job, fuzz targets for the
   chain iterator and `detect_version`.
6. Reference backends: MCU I2C (HAL callbacks), U-Boot i2c_eeprom (DM), kernel
   nvmem/regmap, POSIX file; `examples/uboot/` skeleton.

## v0.4.0 — Ecosystem and language parity

1. Go and TypeScript header implementations; cross-language matrix grows to 6×6.
2. Implementation conformance contract + shared conformance tests (#17); close
   current asymmetries (Python v1/v2 + `detect_version`, #16).
3. Device identity: finalize and implement `device.id`
   ([format/device-identity-v1.md](format/device-identity-v1.md), RFC #26) once
   agreed — parsers in C/Python/Rust, codegen metadata, test vectors.
4. Header v4 draft only if concrete field requirements have accumulated
   (board role field, etc.) — RFC process through docs/format.

## v1.0.0 — Freeze

1. Format v3 (and v4 if introduced) and the public C API are frozen; SemVer,
   stable SOVERSION / crate / package versions.
2. Full spec audit: EEPROM_FORMAT.md free of legacy external references, Outline
   mirror regenerated, README reflects the multi-language scope.
3. Entry criteria: every public function and error code covered by tests, N hours
   of fuzzing without findings, cross-language matrix green on LE and BE.

## Non-goals

- Async/callback storage API — all target environments access EEPROM synchronously.
- FFI-based unification of header parsing — native implementations per language
  is a settled decision.
- Wear-leveling, journaling, directories, fragmentation support — the format
  remains a singly-linked list for ~8 KiB parts.
- Migrating the spec source from markdown tables to YAML/JSON.
- Internal locking — synchronization is the caller's responsibility (plus
  optional port-layer hooks).
- Speculative format v4 without concrete field owners.
