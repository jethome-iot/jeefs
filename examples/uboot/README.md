# J100 U-Boot identity reader

A read-only `jeefs_identity` command for U-Boot proper. The example uses
Driver Model I²C, the real U-Boot CRC provider, and the JEEFS pull-model
walker. No EEPROM writes or U-Boot environment-variable updates. CRCs
verify integrity; the command does not verify ECDSA authenticity.

The selected target is J100 (`jethub_j100_defconfig`). The source pin in
[source.json](source.json) matches the inspected `uboot-jethome` checkout
and exists in the official U-Boot repository. CI exports that exact commit,
builds the AArch64 U-Boot ELF and checks that the example command, reader
and walker are actually linked. It does not use fake U-Boot headers.

## Build

Requirements: Python 3.12+, Git, GNU Make, host GCC, AArch64 GCC/binutils,
bison, flex, bc, OpenSSL/GnuTLS development headers, Python development
headers/setuptools/pyelftools and swig. Standard distro packages suffice.

Fetch the source described by source.json, or use a local Git repository
containing its pinned commit. Then, from the JEEFS repository:

```sh
python3 examples/uboot/build.py \
    --source /path/to/uboot-git \
    --stage /path/to/new-scratch-directory
```

The stage must be new and outside the source checkout. The script exports
tracked content at the pin, ignoring local working-tree modifications. It
copies only the example and required JEEFS sources/headers into the export,
adds the example under U-Boot proper's `CONFIG_CMD_I2C` guard, configures
J100 and builds `u-boot`. The source checkout remains unchanged. Result
metadata is written to `result.json`; the ELF is `build/u-boot` inside
the stage. No boot image is installed or flashed.

The Kbuild file force-includes U-Boot's `linux/types.h`: its own `stdint.h`
is a dummy shim, unlike the compiler-provided header used by standalone
freestanding builds. `JEEFS_CRC32_UBOOT` selects U-Boot's CRC functions,
so no separate built-in CRC object is linked. This example targets U-Boot
proper; SPL needs its own CRC/provider and I²C/resource configuration.

## Run on a board

```text
jeefs_identity <bus-decimal> <chip-hex> <offset-bytes:1|2> <image-bytes-decimal>
```

Supply the actual bus/address/address-width/capacity from the board's
hardware integration. No address is prescribed here: the inspected J100
DTS has I²C enabled but no EEPROM node. Capacity is 12–65535 bytes; one
address byte supports at most 256 bytes in this adapter (bank addressing
is outside the example). An 8 KiB EEPROM normally needs a two-byte offset,
but the hardware configuration remains the caller's responsibility.

Reads are split into at most 64-byte I²C transfers. The reader:

1. Reads the 12-byte version probe, then the rest of the board header and
   checks its CRC before trusting the filesystem prefix.
2. Feeds the first 28-byte file header through the walker, validating its
   CRC, name, size and link bounds against the supplied EEPROM capacity.
3. For first-slot `device.id`, reads exactly 256 bytes, checks the streamed
   file CRC, then the record's magic, versions and own CRC.
4. Prints bounded identity strings only after validation. If no device.id
   is present in the first slot, prints the valid board header and states
   that the rest of the chain was not searched. It never follows another
   file header or claims to have validated the complete filesystem.

Total successful identity reads: 540 bytes for v2/v3/v4, 796 for v1. Memory
is caller-owned header/record storage, one walker and a 28-byte header
window, rather than an 8 KiB image. The callback belongs only to this
example's environment; the library itself remains byte-buffer-only.

Host tests run the same reader with an environment-owned memory reader;
the real command/CRC/Kbuild integration is checked by the separate J100
build. Compilation and host tests are separate from hardware execution.
