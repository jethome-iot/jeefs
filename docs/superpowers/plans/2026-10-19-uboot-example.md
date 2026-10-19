# J100 U-Boot Example Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Verify PORTING.md in a real J100 U-Boot build and test the bounded identity reader.
**Architecture:** Environment-owned I/O adapter, unchanged byte-buffer JEEFS APIs, first-hop walker and dual CRC verification; stage into pinned upstream sources, never alter the user's checkout.
**Tech Stack:** C11, U-Boot Kbuild, AArch64 GCC, Python staging/build script, CMake tests, self-hosted CI.
**Spec:** ../specs/2026-10-19-uboot-example-design.md

## Global Constraints

- J100 selected by owner; pin U-Boot 0eaa4b337336dbbe93395d1f2ccc18937eaafea2.
- No EEPROM writes, flashing, environment updates or guessed hardware addresses.
- No new library I/O or callback API; callbacks belong only to the example environment.
- Preserve other checkouts and untracked AGENTS.md.

## Review Focus

- Short/failed I/O: return failure before accepting/printing identity.
- Outer file CRC and inner record CRC: independently gate acceptance.
- First file isn't device.id: stop without claiming full-chain absence.
- v1 header size: read budget is 796, not 540 bytes.
- Real Kbuild headers/CRC provider: test actual AArch64 linked U-Boot, not header stubs.

### Task 1: Bounded example reader
Files: examples/uboot/read_identity.{h,c}, tests/test_uboot_example.c, tests/CMakeLists.txt.
- [x] Write tests and observe RED for absent reader.
- [x] Implement example reader with validated board prefix, one walker hop and dual CRCs.
- [x] Run native and sanitizer tests; assert I/O budgets and failure paths.

### Task 2: Real U-Boot embedding
Files: examples/uboot/command.c, Makefile, build.py, source.json; tools/tests/test_uboot_build.py.
- [x] Write staging script safety tests (existing destination and wrong source revision) and observe RED.
- [x] Implement explicit command and pinned, non-destructive source staging/Kbuild integration.
- [x] Build the J100 AArch64 U-Boot ELF and verify required symbols.

### Task 3: CI and guide corrections
Files: .github/workflows/ci.yml, examples/uboot/README.md, docs/PORTING.md, docs/validation/2026-10-19-uboot-example.md, docs/ROADMAP.md.
- [x] Register the actual-build job with pinned source and self-hosted runner.
- [x] Document measured portability corrections and remaining hardware validation.
- [x] Run the full CMake suite, script tests and actionlint; retain outputs.
- [x] Independent final review, then fix substantive findings with regression evidence.

## Progress

Target confirmed by owner: J100, same U-Boot. Implementation proceeds natively.

Task 1: complete — absent reader RED, host reader GREEN; v1-v4, dual CRC, I/O failures and read budgets covered.
Task 2: complete — staging safety tests RED→GREEN; actual J100 AArch64 ELF linked, required symbols present, compiled source hashes match checkout.
Task 3: complete — CI job and guide corrections added; 330/330 CTest and 132 Python tests pass; ASan/UBSan and actionlint pass.
Final review: ignored Kbuild Makefile fixed with narrow exception and git check-ignore RED→GREEN. No other important findings. The reviewer's outstanding ELF gate was subsequently verified by the parent task.
Decision: retain explicit user-supplied I2C parameters and existing J100 provisioning behavior; no hardware writes or flashing.
Environment observation: first macOS bind-mounted build produced empty upstream lib/zlib/built-in.a; clean build with unchanged flags/configuration on a native Linux volume passed. Exact cause of the empty archive was not established; no source workaround was introduced.
