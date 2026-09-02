# Implementation contract

What every language implementation of the JEEFS formats must provide, and
how conformance is enforced. New language ports (Go and TypeScript are
planned) gate on this contract from day one. The byte-level ground truth is
[docs/format/](format/); this document defines the *behavioral* surface.

## Scope levels

| Level | Contents | Required in |
|-------|----------|-------------|
| **Core** | header parsing/building — pure functions over byte buffers | every port |
| **Record** | DeviceIdentityV1 (`device.id`) operations | every port |
| **FS** | linked-list file storage (open/format/add/read/write/delete) | C/C++ only, by design |

## Core operations

Every port provides these over raw byte buffers, with no I/O dependency:

- **detect_version(buf)** — magic `"JETHOME\0"` plus a known version byte
  (1-4). A 12-byte probe suffices; full-header validation is the parser's
  job. An erased or foreign buffer is *"no header"* (`None`/`-1`/`nullopt`)
  — never an exception or crash. Detection MUST NOT depend on
  `signature_version` or the CRC.
- **verify_crc(buf)** — CRC32 (IEEE 802.3, zlib polynomial) over bytes
  `[0, header_size - 4)`, compared against the little-endian stored value.
- **update_crc(buf)** — recalculate and store.
- **init(buf, version)** — zero fill, magic, version byte, CRC. Unknown
  version is an error.
- **is_empty(buf)** — a detected header whose every byte from offset 12
  up to the CRC field is zero is a placeholder (claimed slot, no
  identity yet); populated otherwise; no-header is a distinct third
  outcome. Independent of CRC validity.
- **Field access for current versions (v3, v4)** — string semantics per
  RFC #13: `boardname`/`boardversion` NUL-terminated (at most `size - 1`
  content bytes); the serial slot, `usid` and `cpuid` are bounded strings
  (every byte usable, NUL optional at full length, printable ASCII by
  convention — content validation is the producer's concern). All
  multi-byte fields decode little-endian on any host endianness.
- **signature_version interpretation** — an unknown byte is an error **at
  the interpretation point** (the enum constructor or accessor), never in
  `detect_version` or CRC verification. Full-parse APIs that construct
  typed models (Python `from_bytes`) perform interpretation and therefore
  reject unknown values; byte-view APIs (C struct access, Rust zero-copy,
  C++ views) defer the error to the accessor. Both shapes are conformant.

## Record operations

- **detect / verify_crc / update_crc / init** for the 256-byte
  DeviceIdentityV1 record, gated on the `"JHDEVID\0"` magic.
- Strictness is *deliberately harder than headers* (spec-mandated): an
  unknown `record_version` **or** `signature_version` is a detect/parse
  failure — no forward-compatibility guessing.
- Reserved space (`reserved1`, `reserved2`, `flags` bits): zeroed on
  write, ignored on read — non-zero reserved content is never treated as
  corruption.

## Cross-cutting rules

- **Emptiness, two domains** ([header-common.md](format/header-common.md),
  RFC #14): inside structures empty/reserved is `0x00`; erased medium
  reads `0xFF`; validity is decided by magic/CRC/bounds, never content
  heuristics; the unwritten-slot heuristic accepts both values; an erased
  16-bit link (`0xFFFF`) terminates the file chain like `0`; an
  all-`0x00`/`0xFF` buffer where a header or record is expected means
  *nothing written*, not *corrupt*.
- **Wire is little-endian** everywhere; implementations must be correct on
  big-endian hosts (C: `jeefs_endian.h`; Rust: generated `from_le`
  accessors; Python: explicit `<` struct formats).
- **Errors are values.** Library code never panics, aborts or throws
  uncontrolled on any input: C returns negative `EEPROMError` codes; Rust
  returns `Option`/`bool`/`Result`; C++ returns `std::optional`/`bool`;
  Python raises only the documented `ValueError` from constructing-parse
  APIs. Test utilities are held to a softer rule: exit non-zero with a
  message — never crash on malformed input.
- **Wire stability**: header formats v1-v3 are frozen forever; v1/v2 are
  obsolete (parseable, never written anew); v4 is current. New fields
  arrive only via a new version through the RFC process.

## Enforcement — the shared conformance suite

Conformance is not a claim, it is the following tests passing in CI:

| Layer | What it proves |
|-------|----------------|
| NxN cross-language matrix | every generator × every verifier × every committed header vector (v1-v4, minimal + max-length bounded) produce and accept identical bytes |
| Golden images (v3 and v4) | full FS + header images verified by all four languages, byte-stable across commits |
| `generate_vectors.py --check` (CI + prek) | committed vector binaries never drift from their JSON descriptions |
| Committed record vector | `devid_record_v1.bin` parsed by all four languages against the JSON expectations |
| Per-language unit suites | ctest (C/C++), pytest, cargo test — erased buffers, unknown versions, CRC gates, bounded round-trips, byte-level LE locks |

## Conformance status (2026-08)

| Requirement | C | C++ | Python | Rust |
|-------------|---|-----|--------|------|
| detect_version | `jeefs_header_detect_version` | `HeaderView::detect_version` | `detect_version` | `detect_version` |
| verify/update/init | `jeefs_header_*` | `HeaderBuffer` | model classes | `verify_crc`/`update_crc`/`initialize_header` |
| is_empty | `jeefs_header_is_empty` | `HeaderView::is_empty` | `header_is_empty` | `header_is_empty` |
| bounded strings | raw bytes (caller-decoded) | `strnlen` views | `_pack_bounded`/`_unpack_string` | `str_from_bytes` |
| LE on BE hosts | `jeefs_endian.h` | via C core | `<` struct formats | generated `from_le` accessors |
| record ops | `jeefs_devid.h` | `DeviceIdView`/`DeviceIdBuffer` | `DeviceIdentityV1` | `devid` module |
| header build | `init` + struct writes | `HeaderBuffer` + struct access | `EEPROMHeaderV3`/`V4` | `initialize_header` + byte ops |

Optional per-port extras (not required by the contract): the Python and
Rust ports ship a whole-image tooling surface — `build_image`/`parse_image`
over plain bytes per `filesystem-v1.md` (device.id-first, double CRC,
fs_version stamp/gate, board-header CRC verified on the raw bytes before
anything else, unreadable-payload names reported without aborting the
walk). Each is locked to the C core by the byte-identity of the committed
goldens it must reproduce (`image_build_py_matches_golden[_v4]`,
`image_build_rs_matches_golden[_v4]`); the Rust module is gated on the
`alloc` feature (a subset of `std`), so a `no_std` firmware with an
allocator can rebuild whole images too — the allocation-free header core
is untouched.

Known, accepted deviations:

- Python has no v1/v2 model classes — the versions are obsolete (#16);
  `detect_version` recognizes them, raw bytes remain accessible.
- The C/C++ header *write* path is struct-level (no field-setter API);
  the wrappers expose the packed structs directly.
- Cross-language test utilities hardcode field offsets; they are checked
  by the matrix itself and exempt from the library API rules.

## Adding a language port — checklist

1. Implement the Core and Record operations above.
2. Add a generator and a verifier under `tests/cross-language/`
   (JSON in → bin out; bin + JSON in → field-by-field check).
3. Wire both into the CMake matrix; every committed vector must pass
   N×N against all existing languages.
4. Add golden verifiers for both golden images (v3 and v4).
5. Parse the committed record vector.
6. Unit-test: erased buffers (both fills), unknown header/record/signature
   versions, CRC corruption, a full-width bounded-string round-trip, and a
   byte-level little-endian decode lock.
7. Add the language's native test runner to CI.
