# JEEFS — Tactical TODO

Strategic direction and freeze criteria live in [ROADMAP.md](ROADMAP.md).
The 2026-08 audit backlog (#5–#30) is fully resolved; this file keeps only
small tactical items that have no issue of their own yet.

- **Extend the vector set**: `v3_header_secp192r1` (48-byte signature,
  zero-padded tail), all-zero and all-FF MAC variants, v1/v2 vectors with
  non-trivial USID/CPUID. Mechanical — `test-vectors/generate_vectors.py`
  + the NxN matrix pick new `.json` files up automatically.
- **Longer fuzzing campaigns**: the CI job is a bounded 60s smoke; corpus
  growth and overnight runs are manual for now.
- New language ports (Go, TypeScript) and `examples/uboot/` — tracked in
  [ROADMAP.md](ROADMAP.md), gated on the
  [implementation contract](IMPLEMENTATION_CONTRACT.md) checklist.
