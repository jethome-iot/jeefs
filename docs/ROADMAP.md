# JEEFS Roadmap

Strategic direction across releases. Tactical items live in [TODO.md](TODO.md);
work is tracked in GitHub issues and milestones. The 2026-08 audit backlog
(#5–#30) is fully resolved — v0.2.0 through v0.5.0 shipped it.

## Compatibility policy

- **Header formats v1–v4 are wire-stable forever**: every implementation must
  keep parsing all released header versions. New fields only via a new header
  version through the RFC process. v1/v2 are **obsolete** — never written
  anew, kept parseable as a basis for future formats; v3 is the fielded
  production format; **v4 (board-scoped) is current**.
- **The DeviceIdentityV1 record** (`device.id`) is wire-stable the same way;
  changes only via a new `record_version`.
- **The FS layer carries no legacy constraints before v1.0**: the C API, ABI,
  behavior — and the on-EEPROM file layout itself — may change between 0.x
  releases per the current [filesystem spec](format/filesystem-v1.md); no
  migration support for images written by older 0.x releases.
- Known consumers (jethome-iot/testsuite\*) are non-critical and migrate with
  the API.

## Shipped

| Release | Theme | Highlights |
|---------|-------|------------|
| v0.2.0 | Trustworthy FS core | FS rewritten around a validated chain iterator (#5–#9, #18); header dedup + freestanding include chain (#10); C++ wrappers; release automation to PyPI/crates.io |
| v0.3.0 | Correctness & CI hardening | LE wire access (#11); `assert` re-armed in Release CI; strict codegen parser (#12); sanitizers / strict-warnings / clang-format gates (#20); supply chain: Dependabot + SHA-pinned actions (#22); `JEEFS_`-prefixed public macros; GitHub Release automation (#21) |
| v0.4.0 | Format completion | **Header v4** — board-scoped identity (#56, #58); **DeviceIdentityV1 accepted** with parsers in all four ports (#26, #60); bounded strings (#13); two-domain emptiness rule (#14); Rust BE-correct accessors (#15); implementation contract + conformance suite (#17); CMake install/export/pkg-config (#23) |
| v0.5.0 | Embeddability | **Buffer-centric FS API** — the library performs no I/O (#25 variant A); **port layer**: CRC32 providers with a freestanding built-in default, libraries free of zlib, `JEEFS_LOG`, freestanding smoke CI for aarch64/riscv64 (#24); audit epic closed (#30) |
| v0.6.0 | Filesystem integrity | **Dual versioning** — the `fs_version` byte gates the file area (#80); **28-byte file header** with its own CRC32; **`device.id` first** is normative, enforced by AddFile (#82); **pull-model walker** for bounded-RAM targets + running-CRC port form (#81, #84) |
| v0.7.0 | Provisioning flow | **First-write claim** — AddFile formats a headerless image with an empty current-version header, atomically with the write (#86); **`is_empty`** contract entry point in all four ports distinguishes a placeholder header from a provisioned one |
| v0.7.1 | Contract clarity | Verbatim **timestamp semantics** stated everywhere (docstring, spec, wire-locking test): producers set the signing moment, 0 = not provisioned — part of the placeholder-header state |
| v0.8.0 | Image tooling | **Python whole-image API** — `build_image`/`parse_image` over plain bytes (#92): device.id-first, double CRC, hard board-header CRC gate on parse; goldens reproduced byte-for-byte and locked by ctest; `generate_reference.py` is a thin driver over the API |
| v0.9.0 | Rust image tooling | **Rust whole-image API** — `jeefs_header::image` under the `std` feature (#97): the same build/parse semantics as the Python surface, byte-transparent names, `unreadable` reporting; locked to the goldens by `image_build_rs_matches_golden[_v4]`; the `no_std` core is untouched |
| v0.10.0 | Firmware-side images | **Rust image module re-gated on `alloc`** (#100): a `no_std` firmware with an allocator can parse and rebuild whole EEPROM images; MSRV declared (1.87); CI runs the image tests without std; in-place FS CRUD from Rust tracked in #99 |

The consumption model settled in #25: *the environment reads the EEPROM
itself and hands the library bytes* — every operation is a pure function
over a caller-owned image buffer. No hardware backends, ever; the
deployment targets (x86_64 / aarch64 / riscv64) are all little-endian, so
CI proves LE-correctness structurally rather than on BE hosts.

## Next (unscheduled, owner-prioritized)

- **Go port** — the first external validation of the
  [implementation contract](IMPLEMENTATION_CONTRACT.md); follows its 7-step
  checklist, grows the matrix to 5×5. TypeScript follows the same path when
  a consumer materializes.
- **`examples/uboot/` skeleton** — a compilable integration example on top of
  [PORTING.md](PORTING.md), when production integration starts.
- **Consumer migration** — jethome-iot/testsuite\* onto the `jeefs` PyPI
  package; production tooling writing v4 headers and `device.id` records
  (conventions in RFC #26).
- Continuous fuzzing beyond the CI smoke (longer campaigns, corpus growth).

## v1.0.0 — Freeze criteria

1. The FS API has survived a stabilization window with real consumers on
   0.5.x without another break; the buffer-centric surface is then frozen
   under SemVer with stable SOVERSION / crate / package versions.
2. At least one port implemented from the contract alone (Go) with no
   contract corrections required — the contract is proven descriptive,
   not aspirational.
3. Accumulated fuzzing time without findings across all three harnesses;
   the cross-language matrix and both goldens green throughout.
4. Docs audit: README reflects the multi-language scope, the Outline mirror
   matches the release, PORTING.md verified against a real embedding.

## Non-goals

- Async/callback storage API — all target environments access EEPROM
  synchronously; I/O belongs to the environment entirely (#25).
- FFI-based unification of header parsing — native implementations per
  language is a settled decision.
- Wear-leveling, journaling, directories, fragmentation support — the format
  remains a singly-linked list for ~8 KiB parts.
- Migrating the spec source from markdown tables to YAML/JSON.
- Internal locking — synchronization is the caller's responsibility.
- Big-endian CI targets — no BE host exists among the deployment targets.
