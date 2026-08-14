# Code generation policy

## The model: committed code, checked against the spec

The build never runs the generator. The C, Python and Rust artifacts are
ordinary committed files, reviewed like any other code; the kernel, the
firmware and every consumer compile **only what is in git**. The markdown
specs in [docs/format/](format/) are the canonical description of the
binary format — for humans and for the checker alike — and the generator
is the tool that keeps the two in sync.

Think of it as *checker-first*: `codegen-check` (CI) and the prek hook
diff the committed artifacts against the specs on every change, exactly
like `sync_version.py --check` does for version numbers. Regenerating is
simply the way to fix that diff.

## Files

| Role | Files |
|------|-------|
| Source of truth (edit these) | `docs/format/header-common.md`, `header-v1.md`, `header-v2.md`, `header-v3.md`, `header-v4.md`, `device-identity-v1.md`, `filesystem-v1.md` |
| Generated (never edit by hand) | `include/jeefs_generated.h`, `python/jeefs/constants_generated.py`, `rust/jeefs-header/src/generated.rs` |
| Generator | `tools/jeefs_codegen/` (parser, validator, per-language generators) |

## Hard rules

1. **Never edit a generated file by hand.** Each carries a banner saying
   so; the `codegen-check` CI job and the local prek hook
   (`tools/check_codegen_sync.sh`) reject any drift between the committed
   artifacts and the specs — a hand edit cannot pass review.
2. **Never change the format anywhere except `docs/format/*.md`.** Commit
   the spec change and the regenerated artifacts together, in one commit,
   so the diff shows the format change in both forms. Enforcement is
   two-layered: CI rejects any tree where the artifacts and specs
   disagree; the prek hook additionally enforces it at commit time on
   machines where hooks are installed (see below).
3. **Wire-stability policy applies** ([ROADMAP.md](ROADMAP.md)): header
   formats v1-v3 are frozen; new fields arrive only via a new header
   version, through the RFC process (see the device-identity RFC #26 for
   the template).
4. Draft specs carry **no codegen metadata** (`STRUCT`/`SIZE`/`CRC_*`
   HTML comments) until accepted, so they cannot reach generated code by
   accident, and CI enumerates the spec files explicitly — no globbing
   (CMake has no codegen path at all).

## Workflow: changing the format

```bash
# 1. Edit the spec table / metadata
$EDITOR docs/format/header-v3.md

# 2. Regenerate all three artifacts
cd tools && python3 -m jeefs_codegen \
    --specs ../docs/format/header-common.md ../docs/format/header-v1.md \
            ../docs/format/header-v2.md ../docs/format/header-v3.md \
            ../docs/format/header-v4.md ../docs/format/device-identity-v1.md \
            ../docs/format/filesystem-v1.md \
    --c-output ../include/jeefs_generated.h \
    --py-output ../python/jeefs/constants_generated.py \
    --rs-output ../rust/jeefs-header/src/generated.rs

# 3. Verify (the prek hook runs this automatically on commit,
#    provided hooks are installed: `brew install prek && prek install`,
#    one time per clone)
sh tools/check_codegen_sync.sh

# 4. Commit the spec and the artifacts together
git add docs/format include/jeefs_generated.h \
        python/jeefs/constants_generated.py rust/jeefs-header/src/generated.rs
```

The validator refuses gaps or overlaps in the byte layout, sizes that do
not match their types, multi-byte fields without explicit endianness and
misplaced CRC fields — before anything is generated.

## No other regeneration paths

CMake deliberately has **no** codegen target: the manual command above
is the only way to regenerate, so nothing can overwrite the committed
artifacts as a side effect of a build or an IDE action.

## Why the spec is the source and not the C header

- The project ships native implementations in several languages; one
  constrained table parser feeds all generators, whereas C as a source
  would require a C parser for every other language.
- C cannot express the format: `uint16_t` says nothing about wire byte
  order, and CRC coverage is not a language concept. That metadata lives
  in the spec tables.
- The byte-level guarantee between languages does not rest on the
  generator anyway: the cross-language matrix and the golden vectors
  verify actual bytes in CI.
