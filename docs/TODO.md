# JEEFS — Tactical TODO

Strategic direction and freeze criteria live in [ROADMAP.md](ROADMAP.md),
whose "Next — ordered" section carries the current plan and its issues.
The 2026-08 audit backlog (#5–#30) is fully resolved; this file keeps only
small tactical items that have no issue of their own yet.

- **Human review of the Rust FS core** (`rust/jeefs-header/src/fs.rs`):
  in-place mutation logic is safety-critical, and the automated reviewers
  that passed it said as much. No defect is known — it is held by the
  shared mutation vectors and differential runs against C — but it has not
  been read by a person outside the change.

Everything else that was listed here — vector set, fuzzing campaigns, the
Go port, the U-Boot example — now has an issue and a place in the order;
see ROADMAP. TypeScript remains deferred with no issue: it follows Go's
path when a consumer materializes.
