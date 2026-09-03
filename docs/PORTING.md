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
| partial reads on bounded RAM | add `src/jeefs_walk.c` (see below) |
| file system operations | add `src/jeefs.c` |
| headers | `include/` (public surface: `jeefs.h`, `jeefs_header.h`, `jeefs_devid.h`, `jeefs_walk.h`, `jeefs_generated.h`, `jeefs_endian.h`, `jeefs_port.h`, `eepromerr.h`) |

The primary boot scenario — read the board header and `device.id` before
the OS — needs only the first row: the header API is prefix-friendly
(detection needs 12 bytes, verification one 256/512-byte read), and with
`device.id` stored first the whole device identity is a bounded prefix of
256 + 28 + 256 = 540 bytes.

## CRC32 provider (include/jeefs_port.h)

`jeefs_crc32(buf, len)` must equal zlib `crc32(0, buf, len)` (IEEE 802.3,
reflected), and `jeefs_crc32_update(crc, buf, len)` is its running form
(`update(update(0, a), b) == jeefs_crc32(a||b)`) for stream consumers.
Pick one:

| Define | Environment | Notes |
|--------|-------------|-------|
| *(none)* | anywhere | built-in table (`src/jeefs_crc32.c`), ~1 KiB `.rodata`; the default and the SPL/MCU fallback |
| `JEEFS_CRC32_ZLIB` | hosted builds | thin wrapper over `<zlib.h>` |
| `JEEFS_CRC32_UBOOT` | U-Boot proper | `lib/crc32.c` is derived from zlib — verified drop-in; `CONFIG_CRC32` is `def_bool y`. **SPL**: `SPL_CRC32` is conditional — use the default provider there |
| `JEEFS_CRC32_KERNEL` | Linux kernel | **must** wrap: `crc32_le(~0, buf, len) ^ ~0`; the plain `crc32_le` computes without the standard inversions and yields different values |

When a provider define is set, `src/jeefs_crc32.c` compiles to nothing —
it is safe to keep it in the file list unconditionally.

## Partial reads on bounded RAM (include/jeefs_walk.h)

When buffering the whole image is too expensive (U-Boot SPL, small MCUs),
locate any file with the pull-model walker: the library owns the state
machine and the validation (header CRC before any field is trusted, exact
contiguity, bounds), the environment owns every read — no callbacks, no
I/O inside the library:

```c
JEEFSWalk w;
uint8_t hdr[sizeof(JEEFSFileHeaderv1)];
jeefs_walk_begin(&w, prefix, prefix_len, image_size, "wifi.conf");
uint32_t off; uint16_t len;
while (jeefs_walk_want(&w, &off, &len) == 1) {
    env_read(off, hdr, len);                  /* 28 bytes per hop */
    if (jeefs_walk_feed(&w, hdr, len) != 0)
        break;
}
/* JEEFS_WALK_FOUND: stream the data from w.file_offset in windows and
 * verify incrementally: crc = jeefs_crc32_update(crc, chunk, n); compare
 * with w.file_crc32 at the end. */
```

`prefix` is the board-header prefix the environment already read (>= 12
bytes); verifying the board-header CRC first stays the caller's job. One
scope note: the walker validates every header it visits up to and
including the match, then stops — `EEPROM_ReadFile` additionally
validates the rest of the chain. Peak
RAM is the walker struct plus one 28-byte header window plus the data
window of your choice — about 130 bytes total with a 64-byte window,
against 8 KiB for the buffer-centric API. Locating and stream-verifying a
small file on an 8 KiB image costs a few hundred bytes of reads; writes
stay buffer-centric by design.

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

## Rust firmware

The `jeefs-header` crate carries the same model, so Rust firmware needs
no C in the build:

```bash
cargo add jeefs-header --no-default-features
```

That pins whatever version the build resolves; the bare `no_std` build
gives the header and `device.id` APIs plus
`jeefs_header::fs` — `format`, `files`, `read_file`, `add_file`,
`write_file`, `delete_file`, `header_check_consistency` over a
caller-owned `&mut [u8]`, allocating nothing. Failures are `FsError`
values, one per `EEPROMError` code; the crate never panics on image
content (validated by the differential fuzzing described below).

Add `--features alloc` when the firmware has an allocator and wants
to rebuild a whole image at once (`jeefs_header::image`) instead of
editing in place; `std` additionally brings `std::error::Error`
integration and is the default for host tooling.

The C `jeefs_walk.h` walker has no Rust counterpart: a target too small
to buffer its EEPROM should call the C walker. Everything else is
interchangeable — the Rust FS port is held to the C core byte for byte
by `tests/cross-language/fs_vectors` (ctest `fs_mutation_c_matches_rs`),
which replays the same scenarios, plus generated random ones, through
both implementations and compares the resulting images.
