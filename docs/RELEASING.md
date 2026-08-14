# Releasing

One version, one tag, zero manual publishing. CI does everything on a
`v*` tag push; there is no manual `twine`, `cargo publish` or release
drafting.

## Version source

`version.json` is the single source of truth. `tools/sync_version.py`
propagates it into `python/pyproject.toml`, `rust/jeefs-header/Cargo.toml`
and `include/jeefs_version.h`; `--check` mode (prek hook + the
`codegen-check` CI job) rejects any tree where they disagree.

## Procedure

```bash
# 1. Set the release version and sync it everywhere
$EDITOR version.json                 # e.g. stable: 0.3.0
python3 tools/sync_version.py
git commit -am "Release 0.3.0"

# 2. Land it on master through the normal PR flow (CI must be green)

# 3. Tag and push the tag
git tag v0.3.0
git push origin v0.3.0
```

The tag push runs the full test matrix plus the three hardening gates,
then — only if everything is green — the publish jobs:

| Job | What it does | Auth |
|-----|--------------|------|
| `publish-pypi` | builds sdist+wheel, uploads `jeefs` to PyPI | trusted publishing (OIDC), no token |
| `publish-crates` | `cargo publish --locked` for `jeefs-header` | `CRATES_TOKEN` repo secret |
| `publish-release` | GitHub Release with generated notes, sdist/wheel and `.crate` attached | `GITHUB_TOKEN` |

Every publish job independently verifies the tag against the package
version it publishes; a mismatched tag fails before anything uploads.

## Backfill / recovery

`publish-crates` also accepts `workflow_dispatch` for re-publishing a
crate when a tag ran before the job existed (used for 0.2.0). PyPI and
GitHub Releases have no dispatch path — retag (delete and push the tag
again) only if nothing was published, since neither registry allows
overwriting a released version.

## After the release

- bump `version.json` `dev` field (e.g. `0.3.1-dev`) in the next PR;
- move remaining open issues to the next milestone and close the
  released one.
