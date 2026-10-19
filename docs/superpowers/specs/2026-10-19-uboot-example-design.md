# J100 U-Boot identity example

## Scope

Implement #111 using the owner's selected J100 target and the existing
U-Boot source at /Volumes/Devel/jUBOOT/uboot-jethome. Its current commit
0eaa4b337336dbbe93395d1f2ccc18937eaafea2 is also available from the official
https://github.com/u-boot/u-boot repository. Pin that commit in CI.
Do not modify the user's U-Boot checkout, flash a board or replace its
existing misc_init_r provisioning logic. Hardware behavior is a separate,
not-yet-performed validation step.

## Environment and public example

Add examples/uboot/read_identity.c and .h, plus command.c. The reader is
example code, not a new library storage API: the example's environment
supplies a read operation/context and owns all I/O; the JEEFS library still
only receives byte buffers. A U-Boot adapter implements that operation with
dm_i2c_read, splitting reads into at most 64-byte transfers. The command
jeefs_identity takes explicit decimal bus, hex seven-bit chip address,
decimal offset-byte count (1 or 2) and decimal image byte count (<=65535).
Do not guess a J100 EEPROM bus, address or DT node; the inspected DTS has
no EEPROM node. No EEPROM writes, environment updates or ECDSA verification.

The reader fills a caller-owned result containing the board header union,
its length, a DeviceIdentityV1 record and a has_device flag. Return distinct
example-level status values for invalid arguments, I/O failure, invalid
board header, unsupported/bad filesystem, payload CRC and record validation.
A missing device.id in the first slot is a normal board-only result;
never claim to have searched the rest of the filesystem.

Read the 12-byte version probe, then the remainder of the detected header,
and verify its CRC. Support v1-v4 (512/256 bytes). Use jeefs_walk_begin,
want/feed for exactly the first file header. If the first slot matches
DEVICE_ID_FILENAME, require exactly 256 payload bytes and stream them in
64-byte chunks, checking the file payload CRC and then the record's own
magic/version/signature-version/CRC. Never follow a second file-header
request: the normative device.id-first invariant makes that unnecessary.
Current headers require at most 540 EEPROM bytes read; v1 needs 796.
The walker validates links using the complete supplied EEPROM capacity.
Printing identity values happens only after the complete requested result
has passed validation; use bounded string widths. Missing device identity
prints the valid board header plus an explicit first-slot-only message.

## Real build verification

Add a Kbuild Makefile compiling command/reader and the existing header,
record and walker sources. Force-include U-Boot linux/types.h before the
JEEFS headers: U-Boot's own stdint.h is an empty compatibility shim, and
standalone freestanding compilation does not exercise this. Select the
real U-Boot CRC provider; do not substitute fake crc.h or string/type headers.

A staging/build script exports the pinned U-Boot revision into a new
scratch directory, copies the example and required JEEFS files, registers
its Kbuild directory under U-Boot proper's CMD_I2C guard and configures
jethub_j100_defconfig. Build the actual AArch64 U-Boot ELF and require its
symbol table to contain the command, example reader and walker symbols.
Use a fresh stage path; refuse overwriting an existing tree. Re-run through
an explicit build target/script, not undocumented manual compiler flags.

CI uses self-hosted runners, installs needed distro build dependencies,
fetches the pinned public U-Boot commit and runs that same script. No
vendored U-Boot source or toolchain binaries enter this repository.

## Behavioral validation

Host tests compile the same example reader against the real JEEFS core,
with an environment-owned memory reader that records offsets, lengths and
failures. No fake U-Boot headers are used. Cover v1-v4, erased media,
header CRC, bad fs_version, missing/first-slot-only device identity, wrong
record size, file-header CRC, file-payload CRC, inner-record CRC/versions,
short capacity and injected I/O failures. Assert byte-read budgets and no
read beyond the first identity payload. Build and run under the existing
sanitizer path too.

Record every correction to PORTING.md as a finding, including the integer
type shim, the missing walker requirement for locating/validating the file
wrapper, and the 540-byte claim applying to 256-byte headers (v1: 796).
Document required build tools, pinned source and hardware limitations.
