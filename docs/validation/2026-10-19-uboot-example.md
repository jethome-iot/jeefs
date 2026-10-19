# J100 U-Boot example validation

Target selected by owner: J100, same U-Boot as the local uboot-jethome
checkout. Upstream source: https://github.com/u-boot/u-boot.git, commit
0eaa4b337336dbbe93395d1f2ccc18937eaafea2, jethub_j100_defconfig.
The original checkout remains unchanged and clean.

## Real embedding artifact

The actual AArch64 U-Boot ELF built and linked through Kbuild with the
real linux/types.h environment and U-Boot CRC provider. No fake U-Boot
headers or linker/configuration workarounds were used.

- ELF: build-uboot/validation/u-boot (local build artifact).
- SHA256: f419bec6356b49422b59787440155a7b266e898968ca12973fa0c0212dcd9964.
- ELF machine: AArch64.
- Required linked symbols: do_jeefs_identity,
  jeefs_example_read_identity, jeefs_walk_feed.
- All staged example/core/header source SHA256 values match this checkout.
- result.json, elf-header.txt, symbols.txt and compiled-source-sha256.txt
  are retained beside the ELF.

Local environment: one-off image based on the existing pinned
jethome-dev-host:ubuntu-24.04-r37493405514.1 plus the documented distro build
dependencies, GNU AArch64 toolchain, native Linux filesystem build volume.
The first macOS bind-mounted build produced an empty upstream zlib archive;
the clean Linux-volume build passed with identical Kbuild/linker flags and
board configuration. The empty archive's precise cause was not established.
No workaround for it enters the repository.

## Behavioral and repository checks

- Full CMake Release build and CTest: 330/330 passed.
- Host reader covers v1-v4; exact 796/540-byte read limits; board/file-header/
  file-payload/record CRC; unknown filesystem/record/signature versions;
  wrong record size; missing first-slot device.id; injected I/O failures;
  later-chain corruption intentionally outside the prefix contract.
- CLI number parser rejects overflow, signs, junk and out-of-range values.
- Focused ASan/UBSan reader test passed.
- Python package/codegen/staging tests: 132 passed.
- Staging refuses existing destinations, source-internal destinations and
  sources lacking the pinned commit.
- actionlint and clang-format checks passed; git diff --check passed.
- Independent review found an ignored Kbuild Makefile; fixed with a narrow
  .gitignore exception and an observed git check-ignore RED→GREEN check.
  No other important correctness findings were reported. The final real-ELF
  gate, pending during review, was subsequently verified by the parent task.

## Guide findings

- [GOTCHA] U-Boot stdint.h is a shim: include linux/types.h before JEEFS.
- [GOTCHA] Board-header/record APIs alone do not validate the file wrapper;
  use the walker and outer payload CRC too.
- [GOTCHA] 540 bytes applies to 256-byte headers; v1 needs 796.
- Clarify the uint16_t image-size ceiling as 65535 bytes.

Raw logs are retained locally as docs/validation/2026-10-19-uboot-*.log.
The new GitHub CI job is configured but has not run for this branch.
Hardware execution, EEPROM reads on an actual J100 and flashing have not
been performed; compilation and host tests do not substitute for them.
