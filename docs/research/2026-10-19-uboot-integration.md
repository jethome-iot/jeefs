# U-Boot integration discovery

Status: owner selected J100 (same U-Boot). Use the inspected local tree
and its publicly available pinned revision for the example. Working branch codex/uboot-example
starts at merged master 1f3f41d (#124).

## Premises

- Issue #111 requires a real production embedding, a compilable example,
  walker-based reads of board identity and device.id, and CI validation.
- docs/PORTING.md lists compiler-provided stdint.h/size types as the required
  environment surface and a U-Boot CRC provider, but its current CI smoke
  compiles outside the U-Boot include/build environment.
- Local /Volumes/Devel/jUBOOT/uboot-jethome is at master 0eaa4b33733.
  include/stdint.h is a dummy including limits.h; it does not define uint32_t.
- A minimal syntax-only probe containing '#include <stdint.h>' followed by
  'uint32_t jeefs_uboot_type_probe;' fails with unknown type name uint32_t
  when that U-Boot include directory is selected. Output is retained locally
  in docs/validation/2026-10-19-uboot-stdint-probe.log.
- The inspected J80 board code (board/amlogic/jethub-j80/jethub-j80.c)
  obtains serial, USID and MAC from eFuse through meson_sm_read_efuse.
  Neither inspected J80/J100 board implementation contains JEEFS integration.
  This does not establish whether a target board has an EEPROM.

## Execution trace

The JEEFS headers include stdint.h. In the standalone freestanding build
this resolves to compiler headers; inside the inspected U-Boot tree it
resolves to U-Boot's dummy header. JEEFS therefore needs the target's normal
integer-type environment made available when its translation units compile.
The precise Kbuild integration must be tested in the selected production tree.

## Conclusions

- The standalone freestanding CI smoke is insufficient evidence for an
  actual U-Boot build. Verify the integration with the target's headers,
  Kbuild flags and CRC provider rather than inventing compatibility stubs.
- The target board, source revision and environment-owned EEPROM read path
  are required inputs. Do not guess I2C addresses, device offsets or the
  boot stage, and do not alter another U-Boot checkout before selecting them.
- The primary bounded-prefix case is 540 bytes for v2/v3/v4; v1 has a
  512-byte board header, so its equivalent prefix is 796 bytes. The example
  must state which layouts it accepts.

## Rejected paths

- Do not count a host build with fake U-Boot headers as production embedding.
- Do not pick J80/J100 solely because their sources happen to be local.
- Do not infer an EEPROM layout from the current eFuse provisioning code.

## Portability findings

- [GOTCHA] U-Boot's stdint.h is a shim: include linux/types.h first in the
  integration Kbuild flags, without changing the portable library headers.
- [GOTCHA] Header + record parsing alone skips the file wrapper's validation;
  boot-time extraction also needs the walker and the outer payload CRC.
- [GOTCHA] The 540-byte prefix applies only to 256-byte headers; v1 needs 796.
- [DECISION] Use an opt-in command with explicit chip parameters. Do not add
  an invented EEPROM node or replace J100's existing MAC fallback behavior.

## Validation outcome

The actual J100 AArch64 ELF linked on the pinned U-Boot source. Required
command/reader/walker symbols are present, and the staged source hashes
match the working checkout. The original U-Boot checkout remains clean.
See ../validation/2026-10-19-uboot-example.md. No board was flashed or run.
