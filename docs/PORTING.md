# Porting

How to embed the C library in U-Boot, the Linux kernel or MCU firmware.
The consumption model (#25): **the environment reads the EEPROM itself and
hands the library bytes** — every operation works on a caller-owned
`uint8_t *image, uint16_t size`; the library performs no I/O, allocates
nothing, uses no VLAs and keeps the stack bounded. Synchronization is the
caller's: operate on a buffer one context owns.

## What to compile

| Need | Files |
|------|-------|
| header + device-identity parsing | `src/jeefs_header.c`, `src/jeefs_devid.c` (+ `src/jeefs_crc32.c` for the default CRC) |
| file system operations | add `src/jeefs.c` |
| headers | `include/` (public surface: `jeefs.h`, `jeefs_header.h`, `jeefs_devid.h`, `jeefs_generated.h`, `jeefs_endian.h`, `jeefs_port.h`, `eepromerr.h`) |

The primary boot scenario — read the board header and `device.id` before
the OS — needs only the first row.

## CRC32 provider (include/jeefs_port.h)

`jeefs_crc32(buf, len)` must equal zlib `crc32(0, buf, len)` (IEEE 802.3,
reflected). Pick one:

| Define | Environment | Notes |
|--------|-------------|-------|
| *(none)* | anywhere | built-in table (`src/jeefs_crc32.c`), ~1 KiB `.rodata`; the default and the SPL/MCU fallback |
| `JEEFS_CRC32_ZLIB` | hosted builds | thin wrapper over `<zlib.h>` |
| `JEEFS_CRC32_UBOOT` | U-Boot proper | `lib/crc32.c` is derived from zlib — verified drop-in; `CONFIG_CRC32` is `def_bool y`. **SPL**: `SPL_CRC32` is conditional — use the default provider there |
| `JEEFS_CRC32_KERNEL` | Linux kernel | **must** wrap: `crc32_le(~0, buf, len) ^ ~0`; the plain `crc32_le` computes without the standard inversions and yields different values |

When a provider define is set, `src/jeefs_crc32.c` compiles to nothing —
it is safe to keep it in the file list unconditionally.

## Required environment surface

- Compiler-provided freestanding headers: `<stdint.h>`, `<stddef.h>`,
  `<stdbool.h>`.
- String functions (the kernel, U-Boot and newlib all provide them):
  `memcpy`, `memmove`, `memset`, `memcmp`, `strncmp`, `strncpy`,
  `strnlen`.
- Nothing else: no heap, no POSIX, no stdio (see logging below).

CI compiles the four core files with `-ffreestanding -Os -Wvla -Werror`
for aarch64 and riscv64 on every change (the deployment targets are all
little-endian; the wire codec is endian-correct regardless).

## Logging

`debug()` diagnostics are compiled out entirely unless `DEBUG` is
defined. With `DEBUG`, define `JEEFS_LOG(fmt, ...)` before `debug.h` is
included (or on the command line) to route output into the environment's
logger — `printk`, U-Boot `printf`, a HAL trace macro. Only the hosted
default pulls in `<stdio.h>`.

## Resource contract

- no heap, no VLAs; stack usage per operation is bounded by a few file
  headers (~100 bytes) — the image itself is the caller's buffer;
- payloads ≤ `INT16_MAX`; images ≤ 64 KiB (`uint16_t` addressing, 8 KiB
  by design);
- all errors are negative `EEPROMError` values (`eepromerr.h`) — the
  library never traps, asserts or exits;
- thread safety = external: one context owns the image during an
  operation.
