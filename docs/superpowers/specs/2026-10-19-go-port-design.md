# Go header and device identity port

## Intent and approved scope

Implement issue #110: validate that IMPLEMENTATION_CONTRACT.md and the
canonical format specs suffice to build another native port. Implement
Core and Record operations, a code generator and the conformance harness.
FS CRUD, walker and whole-image build/parse are outside this change.
Do not use existing C, Python or Rust implementation logic as the port's
source. Shared harness conventions and vectors are integration inputs.
Do not silently repair the contract: record any missing or contradictory
requirement as a finding before resolving it.

## Package and API choices

Builder proposal: use a dependency-free module at `go/`, module path
`github.com/jethome-iot/jeefs/go`, package `jeefs`. Use only the standard
library (`encoding/binary`, `hash/crc32`, etc.). Select the minimum Go
version after checking the maintained toolchains available in CI; pin the
CI toolchain. No cgo, unsafe casts or native struct memory serialization.

Use a byte-view API over caller-owned slices, with documented aliasing.
DetectVersion accepts a 12-byte probe; CRC, initialization, emptiness and
field operations require a complete structure. Header sizes and field
metadata come from generated code. Return errors for unsupported versions,
short buffers and invalid field operations, with no panic on malformed input.
Detection returns a known version or no header and does not interpret the
signature algorithm. Header signature interpretation is a separate accessor.
Header emptiness distinguishes no header, placeholder and populated,
independently of CRC. Full header views validate bounds and version, while
CRC verification remains explicit.

Expose current v3/v4 string fields, MAC, signature, signature algorithm,
fs_version and signed timestamp through checked accessors/setters. Bounded
strings accept the entire field; name-like fields reserve a terminating NUL.
Reject overflow instead of truncating. Byte strings preserve their bytes;
printable ASCII is a producer convention for identity fields. Timestamp 0
is preserved. Do not verify ECDSA cryptographically: expose stored data.

Record detection rejects unknown record and signature versions. Provide
record initialize/verify/update operations and checked field access for
DeviceIdentityV1. Ignore reserved content on read; initialize and serialize
reserved bytes and flags as zero. Document when field mutation invalidates
CRC and require explicit update before persistence. Initialization creates
zero-filled placeholders and seals their CRC, without touching any trailing
bytes in a larger caller buffer. Header init supports known versions for
conformance tooling; v1/v2 remain obsolete for production writers.

## Generated artifacts

Add `tools/jeefs_codegen/generators/go_generator.py` and `--go-output`.
Generate committed wire metadata, constants, enum values and byte-backed
structure types from the same parsed FormatSpec. All multibyte operations
use explicit little-endian encoding. Do not expose a Go struct whose native
padding could be mistaken for the wire layout. Integrate the artifact with
`check_codegen_sync.sh`, CI and CODEGEN.md. Code generation remains manual;
the normal build must never regenerate committed code. Add generator tests
for deterministic output, offsets, sizes and little-endian field handling.

## Conformance integration

Add Go generator/verifier commands under tests/cross-language, using the
same JSON-to-binary and binary-plus-JSON interface as existing runners.
Validate all expected fields and all signature bytes including padding;
reject malformed metadata and wrong sizes without a panic. Add the Go
verifier to the rejection suite. CMake locates Go and builds harnesses into
the build directory, depending on every relevant source and go.mod. When
Go is installed, register all 25 generator/verifier pairs for every header
vector. CI must install Go so the fifth language cannot silently disappear.

Verify both golden images, including header, record, file-chain bounds and
the two CRCs on file entries; parse the committed record vector against its
JSON expectations. Golden-chain inspection may live in test tooling and
must not turn into an unrequested public filesystem API.

## Validation and completion

Native tests cover both erased fills, every header version, short buffers,
unknown header/record/signature versions, CRC corruption, placeholder state,
full-width bounded strings, name terminators, signed timestamps, explicit LE
wire bytes and reserved-record semantics. Include malformed-input coverage
that checks public functions do not panic. Run go test and go vet, codegen
checks, the full 5x5 matrix, golden/record checks and verifier rejections.
Save command outputs as durable validation artifacts. Run the existing
build and relevant suites to catch integration regressions; inspect the diff.

Document the package, install/import path and conformance status. Keep USID
wording and JEEFS/Rust naming changes separate from the Go implementation.
Go release/tag automation is deferred: this change does not publish a module
or change existing package versions. Any need for a separate Go module tag
must be decided before announcing an installable release.
