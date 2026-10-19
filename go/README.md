# JEEFS/Go

Native identity header and DeviceIdentityV1 support, with no cgo, I/O or
external dependencies. Go 1.26 or newer. Module:
`github.com/jethome-iot/jeefs/go`. The module has not yet been released;
use this checkout locally until a module version is published.

```go
import "github.com/jethome-iot/jeefs/go"

func provision(image []byte, usid string) error {
    if err := jeefs.InitHeader(image, 4); err != nil {
        return err
    }
    header, err := jeefs.NewHeader(image)
    if err != nil {
        return err
    }
    if err := header.SetString("usid", usid); err != nil {
        return err
    }
    return header.UpdateCRC()
}
```

USID is an internal identifier assigned during production. Its physical
storage is platform-dependent, including eFuse when capacity permits.

`DetectVersion` needs a 12-byte probe and returns zero for no recognized
header. `NewHeader`, `VerifyCRC`, `UpdateCRC`, `HeaderIsEmpty` and field
operations need a complete header (512 bytes for v1, 256 for v2–v4).
Buffers may include a trailing image: it is preserved. Versions v1/v2
remain parseable but are obsolete for new production writes.

`HeaderIsEmpty` returns `NoHeader`, `Placeholder` or `Populated`. A known
but incomplete header is an error. CRC validity is a separate concern.
`NewHeader` does not verify CRC or interpret signature algorithms; call
`VerifyCRC` and `SignatureAlgorithm` explicitly. Unknown header signature
algorithms remain detectable and CRC-checkable, but interpretation fails.

`NewRecord` and `DetectRecord` require a complete DeviceIdentityV1 and
reject unknown record/signature versions. Use `InitRecord` to initialize,
`record.VerifyCRC()` to verify. Nonzero reserved content is accepted on
read. `record.Bytes()` zeroes reserved fields/flags in an owned copy and
seals its CRC; it does not alter the original memory.

Views alias caller buffers and revalidate on every operation. Do not mutate
or share the backing memory concurrently. Setters invalidate stored CRC.
`UpdateCRC` reseals original bytes without normalization; `Bytes` returns
an owned, CRC-sealed serialization. Zero-value views return errors.

`String` / `SetString` use canonical field names: v3 `serial`, v4
`board_serial`; `boardname`, `boardversion`, `usid`, `cpuid`; records use
`device_model`, `device_serial`, `hw_revision`. Byte strings preserve
invalid UTF-8. Bounded fields use their whole width; name-like fields
reserve one terminating NUL. Overflow and embedded NUL are rejected
without partial writes. Timestamps are signed LE values stored verbatim.
Signatures are raw r||s bytes; no ECDSA verification is performed.

The Go port implements neither CRUD nor whole-image build/parse. Golden
file-chain inspection exists only in the conformance command.

```sh
go test ./...
go vet ./...
```

Wire metadata in `generated.go` comes from `docs/format/*.md`. Never edit
it by hand; regeneration needs Python and gofmt, see `docs/CODEGEN.md`.
Dual-licensed GPL-2.0-or-later / Apache-2.0, like the repository.
