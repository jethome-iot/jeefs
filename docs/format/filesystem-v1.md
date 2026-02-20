# JEEFS Filesystem v1 — Linked-List File Storage

## Overview

JEEFS stores files as a singly-linked list immediately after the EEPROM header. Each file consists of a 24-byte file header followed by the file's data bytes. The last file has `nextFileAddress = 0`.

## File Header

<!-- STRUCT: JEEFSFileHeaderv1 -->
<!-- SIZE: 24 -->

| Offset | Size | Field           | Type       | Endianness    | Description                            |
|--------|------|-----------------|------------|---------------|----------------------------------------|
| 0-15   | 16   | name            | char[16]   | -             | Filename, null-terminated (max 15 ch.) |
| 16-17  | 2    | dataSize        | uint16_t   | little-endian | File data size in bytes                |
| 18-21  | 4    | crc32           | uint32_t   | little-endian | CRC32 of file data only (not header)   |
| 22-23  | 2    | nextFileAddress | uint16_t   | little-endian | Absolute offset of next file, 0 = end  |

## EEPROM Layout

```text
Offset 0:
+------------------------------------------+
| EEPROM Header (v1: 512B, v2/v3: 256B)   |
+------------------------------------------+
| File 1: FileHeader (24B)                 |
|   name | dataSize | crc32 | nextAddr    |
+------------------------------------------+
| File 1: Data (dataSize bytes)            |
+------------------------------------------+
| File 2: FileHeader (24B)                 |
|   nextFileAddress -> File 3              |
+------------------------------------------+
| File 2: Data                             |
+------------------------------------------+
| ...                                      |
+------------------------------------------+
| File N: FileHeader (24B)                 |
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
- Files are ordered by insertion time (not alphabetically).
- The first file header starts immediately after the EEPROM header (at offset = header size).

### Address Calculation

For a file at offset `A` with data size `D`:

- File data starts at: `A + 24` (immediately after file header)
- File data ends at: `A + 24 + D - 1`
- Next file expected at: `A + 24 + D`

The `nextFileAddress` must equal `A + sizeof(JEEFSFileHeaderv1) + dataSize` for a valid chain. A mismatch indicates corruption.

## CRC32

- **Coverage:** CRC32 is calculated over **file data only** (not the file header).
- **Algorithm:** Same IEEE 802.3 / zlib `crc32()` as the EEPROM header.
- **Written on:** file creation (`EEPROM_AddFile`) and file overwrite (`EEPROM_WriteFile`).
- **Validation:** Not currently verified on read (TODO in implementation).

## File Operations

### Creation (EEPROM_AddFile)

1. Verify file does not already exist.
2. Scan linked list to find end (or empty/corrupted slot).
3. Write file header (name, dataSize, crc32, nextFileAddress=0).
4. Write file data after header.
5. Update previous file's `nextFileAddress` to point to new file.

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
- **Max file size:** Limited by `uint16_t dataSize` = 65535 bytes (theoretical), practically limited by available EEPROM space.
- **Zero-size files:** Not allowed (`dataSize = 0` returns `BUFFERNOTVALID`).
- **File fragmentation:** Not supported — each file is contiguous.
- **Addressing:** `uint16_t` offsets — max EEPROM size 65535 bytes.

## Empty Space Detection

Both `0x00` and `0xFF` are treated as "empty":

- `EEPROM_ByteIsEmpty(b)`: `b == 0x00 || b == 0xFF`
- `EEPROM_WordIsEmpty(w)`: `w == 0x0000 || w == 0xFFFF`

A slot is empty if:

- First byte of filename is empty, OR
- `dataSize` is empty (0x0000 or 0xFFFF)
