# JEEFS — Tactical TODO

Strategic release themes and freeze criteria live in [ROADMAP.md](ROADMAP.md);
work is tracked in GitHub issues and milestones (audit tracking:
[#30](https://github.com/jethome-iot/jeefs/issues/30)). This file keeps only
small tactical items and points to their tracking issues.

## Carried over (planned for v0.1.3, not shipped with it)

The v0.1.3 release ended up being license migration + PyPI publishing; the
code-quality items below did not land and are now tracked as part of the
v0.2.0 FS-core work:

- **Proper error codes** (`EEPROMWRITEERROR`, replace bare `-1` returns) —
  folded into the FS-core rewrite,
  [#9](https://github.com/jethome-iot/jeefs/issues/9).
- **Write error propagation** in `eepromops-memory.c` —
  [#29](https://github.com/jethome-iot/jeefs/issues/29) (backlog).
- **CRC32 validation on file read** — checklist item of
  [#9](https://github.com/jethome-iot/jeefs/issues/9).
- **Extract `defragEEPROM()`** — superseded by the rewrite: compaction is
  redesigned inside [#9](https://github.com/jethome-iot/jeefs/issues/9)
  ("implement or drop the public `defragEEPROM`").
- **Expanded test vectors** (max-length fields, all-zero/all-FF MAC,
  v3+SECP192R1, v1/v2 with non-trivial USID/CPUID) — blocked on the
  raw-vs-string decision [#13](https://github.com/jethome-iot/jeefs/issues/13);
  wiring committed vectors into CI is
  [#19](https://github.com/jethome-iot/jeefs/issues/19).

## Package publishing

- **PyPI** — shipped in v0.1.3 (`jeefs` on PyPI, tag-gated CI job).
- **crates.io** — tracked in
  [#21](https://github.com/jethome-iot/jeefs/issues/21) (milestone v0.2.0),
  together with GitHub Releases.

## Future — new language implementations

Go and TypeScript header implementations are scheduled for v0.4.0 (see
[ROADMAP.md](ROADMAP.md)); gated on the language conformance contract
[#17](https://github.com/jethome-iot/jeefs/issues/17). Sizing estimates:
Go ~600-800 LOC (`crypto/crc32` + `encoding/binary`), TypeScript ~700-900 LOC
(DataView + pure-JS CRC32); both need generators added to
`tools/jeefs_codegen/` and grow the cross-language matrix to 5x5 / 6x6.
