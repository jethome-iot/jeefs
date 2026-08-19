# JEEFS Filesystem v1 — Linked-List File Storage

## Overview

JEEFS stores files as a singly-linked list immediately after the EEPROM header. Each file consists of a 28-byte file header followed by the file's data bytes. The last file has `nextFileAddress = 0`.

## Filesystem version

The filesystem is versioned by the `fs_version` byte at **offset 10 of the
board header** (a named field in headers v3/v4; the same offset is
reserved-zero in the obsolete v1/v2 layouts, which therefore read as 0):

- `0` — no filesystem: the file area is treated as empty regardless of its
  content (every image written before the field existed carries 0 and an
  empty file area);
- `1` — the layout described by this document;
- any other value — unsupported: readers report an error rather than guess.

Mutating operations stamp `1` (and refresh the board-header CRC) when the
byte is `0`; readers of a `0` image see zero files.

## File Header

<!-- STRUCT: JEEFSFileHeaderv1 -->
<!-- SIZE: 28 -->

| Offset | Size | Field           | Type       | Endianness    | Description                            |
|--------|------|-----------------|------------|---------------|----------------------------------------|
| 0-15   | 16   | name            | char[16]   | -             | Filename, null-terminated (max 15 ch.) |
| 16-17  | 2    | dataSize        | uint16_t   | little-endian | File data size in bytes                |
| 18-21  | 4    | crc32           | uint32_t   | little-endian | CRC32 of file data only (not header)   |
| 22-23  | 2    | nextFileAddress | uint16_t   | little-endian | Absolute offset of next file, 0 = end  |
| 24-27  | 4    | headerCrc32     | uint32_t   | little-endian | CRC32 of header bytes 0-23             |

The first 24 bytes keep the pre-versioning layout exactly; `headerCrc32`
closes the one unprotected span in the format — without it a bit flip in
`name` read as a silent "file not found". Every header write (creation,
link rewrite during insert/delete, data-CRC refresh on overwrite)
recomputes `headerCrc32`; readers verify it on every visited header.

## EEPROM Layout

```text
Offset 0:
+------------------------------------------+
| EEPROM Header (v1: 512B, v2/v3/v4: 256B)|
+------------------------------------------+
| File 1: FileHeader (28B)                 |
|   name | dataSize | crc32 |             |
|   nextAddr | headerCrc32                |
+------------------------------------------+
| File 1: Data (dataSize bytes)            |
+------------------------------------------+
| File 2: FileHeader (28B)                 |
|   nextFileAddress -> File 3              |
+------------------------------------------+
| File 2: Data                             |
+------------------------------------------+
| ...                                      |
+------------------------------------------+
| File N: FileHeader (28B)                 |
|   nextFileAddress = 0 (end marker)       |
+------------------------------------------+
| File N: Data                             |
+------------------------------------------+
| Free space (0x00)                        |
+------------------------------------------+
```

## Linked List Mechanics

- Files are stored as a **singly-linked list** (forward traversal only).
- `nextFileAddress` contains the **absolute byte offset** within the EEPROM of the next file's header.
- `nextFileAddress = 0` marks the **last file** in the chain.
- Files are ordered by insertion time (not alphabetically) — with one
  exception: the reserved `device.id` file always occupies the first slot
  (see [Creation](#creation-eeprom_addfile) below).
- The first file header starts immediately after the EEPROM header (at offset = header size).

### Address Calculation

For a file at offset `A` with data size `D`:

- File data starts at: `A + 28` (immediately after file header)
- File data ends at: `A + 28 + D - 1`
- Next file expected at: `A + 28 + D`

The `nextFileAddress` must equal `A + sizeof(JEEFSFileHeaderv1) + dataSize` for a valid chain. A mismatch indicates corruption.

## CRC32

Two checksums per file, both the same IEEE 802.3 / zlib `crc32()` algorithm
as the EEPROM header:

- **`crc32` (data):** covers the file data only. Written on file creation
  (`EEPROM_AddFile`) and overwrite (`EEPROM_WriteFile`); verified on every
  `EEPROM_ReadFile()` — a mismatch returns `EEPROMCORRUPTED`.
- **`headerCrc32` (header):** covers header bytes 0-23. Recomputed on every
  header write — creation, link rewrite during insertion or
  delete-compaction, data-CRC refresh on overwrite; verified on every
  header visited during a chain walk — a mismatch returns
  `EEPROMCORRUPTED`.

## File Operations

### Creation (EEPROM_AddFile)

1. Verify file does not already exist.
2. Scan linked list to find end (or empty/corrupted slot).
3. Write file header (name, dataSize, crc32, nextFileAddress=0,
   headerCrc32 over bytes 0-23).
4. Write file data after header.
5. Update previous file's `nextFileAddress` (and its `headerCrc32`) to
   point to the new file.
6. Stamp `fs_version = 1` in the board header (refreshing the header CRC)
   when the byte was 0.

The reserved name `device.id`
([device-identity-v1.md](device-identity-v1.md)) is the one exception:
it is inserted as the **first** file — the existing chain shifts up by
the file's span and every moved link is rewritten — so a boot
environment can read the device identity as a bounded prefix of the
image (header + one file header + the 256-byte record). The other
operations preserve the position naturally: delete-compaction keeps
relative order, a same-size overwrite stays in place, and a
different-size overwrite re-creates the file through the same insertion
rule.

### Overwrite (EEPROM_WriteFile)

- **Same size:** Data is overwritten in place, CRC32 recalculated.
- **Different size:** File is deleted and re-added (triggers defragmentation).

### Deletion (EEPROM_DeleteFile)

1. Find file by name.
2. Calculate `shiftSize = sizeof(FileHeader) + dataSize`.
3. Shift all subsequent data forward by `shiftSize` bytes.
4. Clear freed space at end with `0x00`.
5. Defragmentation is **automatic** — no gaps are left.

### Enumeration (EEPROM_ListFiles)

Traverse linked list from header end, collecting file names until `nextFileAddress = 0`.

## Constraints

- **Max filename:** 15 characters (+ null terminator = 16 bytes).
- **Max file size:** 32767 bytes (INT16_MAX): the int16_t API returns carry byte counts, so larger payloads are rejected with `BUFFERNOTVALID`.
- **Zero-size files:** Not allowed (`dataSize = 0` returns `BUFFERNOTVALID`).
- **File fragmentation:** Not supported — each file is contiguous.
- **Addressing:** `uint16_t` offsets — max EEPROM size 65535 bytes.

## Empty Space Detection

Both `0x00` and `0xFF` are treated as "empty":

- a byte is empty when `b == 0x00 || b == 0xFF`
- a 16-bit field is empty when `w == 0x0000 || w == 0xFFFF`

A slot is empty if the first byte of the filename is empty. A header with
a written name but a bad `headerCrc32` (or an empty `dataSize`, 0x0000 or
0xFFFF) is treated as **corruption**, not as an empty slot: silently
reusing such a slot would destroy whatever wrote the
name. An erased `nextFileAddress` (0xFFFF) in an otherwise valid header
terminates the chain exactly like 0.

The two values come from different domains: bytes inside written
structures are zero-padded (`0x00`), while unwritten space of an erased
medium reads `0xFF`. Readers accept both as "empty"; the validity of a
written record is established by CRC and bounds checks, never by content
heuristics alone.
