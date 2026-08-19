// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * FS-core test suite: positive paths, chain integrity across delete,
 * negative paths on corrupted images (issues #5, #6, #7, #9, #14, #18).
 * Every scenario runs on its own freshly created image.
 */

/* Tests must assert in every build type, including -DNDEBUG ones. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "eepromerr.h"
#include "jeefs.h"

#define IMG_SIZE 8192
#define HDR_V3 256

/* The image is a plain caller-owned buffer (#25 variant A): tests fill,
 * corrupt and inspect it directly — no files, no descriptors. */
static uint8_t image[IMG_SIZE];
static uint16_t image_size = IMG_SIZE;

static uint8_t *fresh_fs(uint16_t size, int version) {
    memset(image, 0x00, sizeof(image));
    image_size = size;
    assert(EEPROM_FormatEEPROM(image, size, version) == 0);
    return image;
}

static void fill_pattern(uint8_t *buf, uint16_t n, uint8_t seed) {
    for (uint16_t i = 0; i < n; i++)
        buf[i] = (uint8_t) (seed + i);
}

static int16_t add_pattern(const char *name, uint16_t n, uint8_t seed) {
    uint8_t buf[512];
    assert(n <= sizeof(buf));
    fill_pattern(buf, n, seed);
    return EEPROM_AddFile(image, image_size, name, buf, n);
}

static void assert_file(const char *name, uint16_t n, uint8_t seed) {
    uint8_t expect[512], got[512];
    assert(n <= sizeof(expect));
    fill_pattern(expect, n, seed);
    int16_t r = EEPROM_ReadFile(image, image_size, name, got, sizeof(got));
    assert(r == (int16_t) n);
    assert(memcmp(expect, got, n) == 0);
}

// Raw corruption helper: overwrite bytes at an absolute offset.
static void poke(uint16_t off, const void *data, uint16_t n) { memcpy(image + off, data, n); }

// Wire fields are little-endian: encode explicitly so the corruption
// scenarios stay identical on a big-endian host.
static void poke_le16(uint16_t off, uint16_t v) {
    uint8_t le[2] = {(uint8_t) (v & 0xFF), (uint8_t) (v >> 8)};
    poke(off, le, 2);
}

static void poke_le32(uint16_t off, uint32_t v) {
    uint8_t le[4] = {(uint8_t) (v & 0xFF), (uint8_t) ((v >> 8) & 0xFF), (uint8_t) ((v >> 16) & 0xFF),
                     (uint8_t) ((v >> 24) & 0xFF)};
    poke(off, le, 4);
}

#define FHDR ((uint16_t) sizeof(JEEFSFileHeaderv1))
#define FHDR_CRC_OFF 24 // offsetof(JEEFSFileHeaderv1, headerCrc32)

// Reseal a file header after a raw poke. The scenarios below model states
// that were legally WRITTEN to the medium (a cycle link, an erased link, a
// stale link onto an empty slot), so their header CRC matches the content —
// each validation rule is exercised on its own, not shadowed by the CRC.
// Corruption detection by the header CRC has its own test.
static void reseal_hdr(uint16_t addr) { poke_le32(addr + FHDR_CRC_OFF, (uint32_t) crc32(0L, image + addr, FHDR_CRC_OFF)); }

// Force a raw fs_version byte and reseal the board-header CRC around it
// (the byte is covered by the header CRC).
static void force_fs_version(uint8_t v) {
    poke(10, &v, 1);
    poke_le32(252, (uint32_t) crc32(0L, image, 252)); // 256-byte headers only
}

static void test_format_and_header(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(EEPROM_HeaderCheckConsistency(image, image_size) == 1);
    uint8_t hdr[HDR_V3];
    assert(EEPROM_GetHeader(image, image_size, hdr, sizeof(hdr)) == 0);
    assert(memcmp(hdr, "JETHOME\0", 8) == 0);
    // buffer smaller than the header must be rejected
    uint8_t small[64];
    assert(EEPROM_GetHeader(image, image_size, small, sizeof(small)) < 0);
    printf("  format/header: OK\n");
}

static void test_empty_fs_lists_zero(void) {
    fresh_fs(IMG_SIZE, 3);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 0);
    printf("  empty list: OK\n");
}

static void test_add_list_read(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("alpha", 10, 1) == 10);
    assert(add_pattern("beta", 20, 2) == 20);
    assert(add_pattern("gamma", 30, 3) == 30);

    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 3);
    assert(strcmp(names[0], "alpha") == 0);
    assert(strcmp(names[1], "beta") == 0);
    assert(strcmp(names[2], "gamma") == 0);

    assert_file("alpha", 10, 1);
    assert_file("beta", 20, 2);
    assert_file("gamma", 30, 3);

    // duplicate add: 0, content untouched
    assert(add_pattern("beta", 20, 99) == 0);
    assert_file("beta", 20, 2);

    // 15-char name round-trips NUL-terminated
    assert(add_pattern("abcdefghijklmno", 8, 4) == 8);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 4);
    assert(strcmp(names[3], "abcdefghijklmno") == 0);

    // maxFiles above INT16_MAX must not be misread as negative
    char big_list[4][JEEFS_FILE_NAME_LENGTH + 1];
    (void) big_list;
    assert(EEPROM_ListFiles(image, image_size, big_list, 65535) == 4);

    // invalid names
    uint8_t d[4] = {1, 2, 3, 4};
    assert(EEPROM_AddFile(image, image_size, NULL, d, 4) == FILENAMENOTVALID);
    assert(EEPROM_AddFile(image, image_size, "", d, 4) == FILENAMENOTVALID);
    assert(EEPROM_AddFile(image, image_size, "abcdefghijklmnop", d, 4) == FILENAMENOTVALID);
    // invalid data
    assert(EEPROM_AddFile(image, image_size, "x", NULL, 4) == BUFFERNOTVALID);
    assert(EEPROM_AddFile(image, image_size, "x", d, 0) == BUFFERNOTVALID);
    printf("  add/list/read: OK\n");
}

static void test_overwrite(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);
    assert(add_pattern("b", 20, 2) == 20);
    assert(add_pattern("c", 30, 3) == 30);

    // same size: in-place
    uint8_t nb[20];
    fill_pattern(nb, 20, 50);
    assert(EEPROM_WriteFile(image, image_size, "b", nb, 20) == 20);
    assert_file("b", 20, 50);
    assert_file("a", 10, 1);
    assert_file("c", 30, 3);

    // different size: delete + add, siblings intact
    uint8_t wb[40];
    fill_pattern(wb, 40, 60);
    assert(EEPROM_WriteFile(image, image_size, "b", wb, 40) == 40);
    assert_file("b", 40, 60);
    assert_file("a", 10, 1);
    assert_file("c", 30, 3);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 3);

    // missing file
    assert(EEPROM_WriteFile(image, image_size, "nope", nb, 20) == FILENOTFOUND);
    printf("  overwrite: OK\n");
}

// The audit's data-loss reproduction: A -> D -> B -> C, delete D, then add.
static void test_delete_chain_integrity(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("A", 16, 1) == 16);
    assert(add_pattern("D", 24, 2) == 24);
    assert(add_pattern("B", 32, 3) == 32);
    assert(add_pattern("C", 48, 4) == 48);

    assert(EEPROM_DeleteFile(image, image_size, "D") == 1);

    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 3);
    assert(strcmp(names[0], "A") == 0);
    assert(strcmp(names[1], "B") == 0);
    assert(strcmp(names[2], "C") == 0);
    assert_file("A", 16, 1);
    assert_file("B", 32, 3);
    assert_file("C", 48, 4);

    // adding after the delete must not clobber surviving files
    assert(add_pattern("E", 8, 5) == 8);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 4);
    assert_file("B", 32, 3);
    assert_file("C", 48, 4);
    assert_file("E", 8, 5);
    printf("  delete chain integrity: OK\n");
}

static void test_delete_last_and_only(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);
    assert(add_pattern("b", 20, 2) == 20);
    assert(add_pattern("c", 30, 3) == 30);

    // delete last: predecessor becomes terminal
    assert(EEPROM_DeleteFile(image, image_size, "c") == 1);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 2);
    assert_file("a", 10, 1);
    assert_file("b", 20, 2);
    assert(add_pattern("d", 12, 4) == 12);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 3);

    // delete down to empty
    assert(EEPROM_DeleteFile(image, image_size, "a") == 1);
    assert(EEPROM_DeleteFile(image, image_size, "b") == 1);
    assert(EEPROM_DeleteFile(image, image_size, "d") == 1);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 0);
    assert(add_pattern("fresh", 10, 7) == 10);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 1);
    assert_file("fresh", 10, 7);

    assert(EEPROM_DeleteFile(image, image_size, "nope") == FILENOTFOUND);
    printf("  delete last/only: OK\n");
}

static void test_corrupted_chain_terminates(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);
    assert(add_pattern("b", 20, 2) == 20);

    // nextFileAddress of "a" points back at "a": a cycle. Offset of the
    // nextFileAddress field inside JEEFSFileHeaderv1 is 22.
    poke_le16(HDR_V3 + 22, HDR_V3);
    reseal_hdr(HDR_V3);

    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == EEPROMCORRUPTED);
    uint8_t buf[64];
    assert(EEPROM_ReadFile(image, image_size, "b", buf, sizeof(buf)) == EEPROMCORRUPTED);
    assert(EEPROM_DeleteFile(image, image_size, "b") == EEPROMCORRUPTED);
    printf("  corrupted chain terminates: OK\n");
}

static void test_oversized_datasize_rejected(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);

    // dataSize of "a" -> 0xFF00: out of bounds for an 8K image. Offset of
    // dataSize inside JEEFSFileHeaderv1 is 16.
    poke_le16(HDR_V3 + 16, 0xFF00);
    reseal_hdr(HDR_V3);

    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == EEPROMCORRUPTED);
    assert(EEPROM_DeleteFile(image, image_size, "a") == EEPROMCORRUPTED);
    uint8_t buf[64];
    assert(EEPROM_ReadFile(image, image_size, "a", buf, sizeof(buf)) == EEPROMCORRUPTED);
    printf("  oversized dataSize rejected: OK\n");
}

// A non-terminal link must point where a file header can actually fit: a
// link to the exact EEPROM end passes naive contiguity but has no room for
// a successor — corrupted, and must not send DeleteFile into a relink loop.
static void test_link_to_eeprom_end_rejected(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);

    // dataSize of "a" -> spans to EEPROM end; next -> exactly eeprom_size
    poke_le16(HDR_V3 + 16, IMG_SIZE - HDR_V3 - FHDR);
    poke_le16(HDR_V3 + 22, IMG_SIZE);
    reseal_hdr(HDR_V3);

    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == EEPROMCORRUPTED);
    assert(EEPROM_DeleteFile(image, image_size, "a") == EEPROMCORRUPTED);
    printf("  link to EEPROM end rejected: OK\n");
}

static void test_data_crc_checked_on_read(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 32, 1) == 32);

    // flip one data byte (data starts right after the file header)
    uint8_t evil = 0xEE;
    poke(HDR_V3 + FHDR + 5, &evil, 1);

    uint8_t buf[64];
    assert(EEPROM_ReadFile(image, image_size, "a", buf, sizeof(buf)) == EEPROMCORRUPTED);
    printf("  data CRC on read: OK\n");
}

// RFC #14: an erased (0xFFFF) nextFileAddress terminates the chain like 0.
static void test_erased_next_is_terminal(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);
    assert(add_pattern("b", 20, 2) == 20);

    // erase the link of "b" (the last file): 0xFFFF instead of 0 — the
    // header was written wholesale onto erased media, so its CRC covers
    // the erased link (RFC #14)
    uint16_t b_addr = HDR_V3 + FHDR + 10;
    poke_le16(b_addr + 22, 0xFFFF);
    reseal_hdr(b_addr);

    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 2);
    assert_file("b", 20, 2);
    // adding after an erased terminal keeps the chain intact
    assert(add_pattern("c", 8, 3) == 8);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 3);
    assert_file("b", 20, 2);
    assert_file("c", 8, 3);
    printf("  erased next is terminal: OK\n");
}

static void test_erased_free_space_0xff(void) {
    // Erased medium: everything after the formatted header reads 0xFF.
    memset(image, 0xFF, sizeof(image));
    image_size = IMG_SIZE;
    assert(EEPROM_FormatEEPROM(image, image_size, 3) == 0);
    // Re-erase the file area to 0xFF (format may have zero-filled it; both
    // fills are legal empty space per issue #14).
    uint8_t ff[64];
    memset(ff, 0xFF, sizeof(ff));
    for (uint16_t off = HDR_V3; off < IMG_SIZE; off += sizeof(ff))
        poke(off, ff, sizeof(ff));

    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 0);
    assert(add_pattern("a", 10, 1) == 10);
    assert(add_pattern("b", 20, 2) == 20);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 2);
    assert_file("a", 10, 1);
    assert_file("b", 20, 2);
    printf("  0xFF erased free space: OK\n");
}

// A written name with an empty dataSize (0x0000 or 0xFFFF) is corruption,
// not an empty slot (RFC #14: validity is decided by checks, not content).
static void test_empty_datasize_is_corruption(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);
    poke_le16(HDR_V3 + 16, 0x0000);
    reseal_hdr(HDR_V3); // rule fires on its own, not via the header CRC
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == EEPROMCORRUPTED);
    printf("  empty dataSize is corruption: OK\n");
}

// Fuzz finding (PR #75): a file whose link points at a valid-but-empty
// slot is a LEGAL terminal (the iterator ends on the empty name), but
// DeleteFile classified it as mid-chain and walked raw headers past the
// chain — corrupting the image or writing out of bounds.
static void test_delete_with_link_to_empty_slot(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("A", 10, 1) == 10);
    assert(add_pattern("B", 20, 2) == 20);

    // B's link: 0 -> its own end (an empty slot) — still a valid chain
    uint16_t b_addr = HDR_V3 + FHDR + 10;
    uint16_t b_end = (uint16_t) (b_addr + FHDR + 20);
    poke_le16(b_addr + 22, b_end);
    reseal_hdr(b_addr);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 2);

    assert(EEPROM_DeleteFile(image, image_size, "B") == 1);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 1);
    assert(strcmp(names[0], "A") == 0);
    assert_file("A", 10, 1);
    printf("  delete with link to empty slot: OK\n");
}

// The reproducer geometry: delete the FIRST file while its successor
// (the last real file) links to an empty slot — the compaction rewrite
// must stop at the moved-region boundary.
static void test_delete_first_with_successor_linking_empty(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("cfg", 22, 1) == 22);
    assert(add_pattern("wifi", 27, 2) == 27);

    uint16_t wifi_addr = HDR_V3 + FHDR + 22;
    uint16_t wifi_end = (uint16_t) (wifi_addr + FHDR + 27);
    poke_le16(wifi_addr + 22, wifi_end); // link into the empty slot
    reseal_hdr(wifi_addr);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 2);

    assert(EEPROM_DeleteFile(image, image_size, "cfg") == 1);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 1);
    assert(strcmp(names[0], "wifi") == 0);
    assert_file("wifi", 27, 2);

    // and the freed span must read as empty space, not stale headers
    assert(add_pattern("new", 8, 3) == 8);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 2);
    printf("  delete first, successor links empty slot: OK\n");
}

static void test_nospace_is_atomic(void) {
    // 512-byte image, v2 header (256): room for one ~200-byte file.
    fresh_fs(512, 2);

    uint8_t big[300];
    fill_pattern(big, sizeof(big), 9);
    assert(add_pattern("f", 200, 1) == 200);
    assert(EEPROM_AddFile(image, image_size, "g", big, sizeof(big)) == NOTENOUGHSPACE);

    // growing "f" beyond free space must not destroy it
    assert(EEPROM_WriteFile(image, image_size, "f", big, sizeof(big)) == NOTENOUGHSPACE);
    assert_file("f", 200, 1);
    printf("  NOTENOUGHSPACE atomicity: OK\n");
}

static void test_set_header_roundtrip(void) {
    fresh_fs(IMG_SIZE, 3);
    uint8_t hdr[HDR_V3];
    assert(EEPROM_GetHeader(image, image_size, hdr, sizeof(hdr)) == 0);
    // boardname lives at offset 12
    memcpy(hdr + 12, "test-board", 11);
    assert(EEPROM_SetHeader(image, image_size, hdr) == 0);

    uint8_t back[HDR_V3];
    assert(EEPROM_GetHeader(image, image_size, back, sizeof(back)) == 0);
    assert(memcmp(back + 12, "test-board", 11) == 0);
    assert(EEPROM_HeaderCheckConsistency(image, image_size) == 1);

    // bad magic is rejected
    uint8_t junk[HDR_V3];
    memset(junk, 0xAB, sizeof(junk));
    assert(EEPROM_SetHeader(image, image_size, junk) < 0);
    printf("  SetHeader roundtrip: OK\n");
}

static void test_consistency_detects_bad_crc(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(EEPROM_HeaderCheckConsistency(image, image_size) == 1);
    uint8_t evil = 0x5A;
    poke(253, &evil, 1); // inside the v3 crc32 field (252-255)
    assert(EEPROM_HeaderCheckConsistency(image, image_size) == 0);
    printf("  consistency detects bad CRC: OK\n");
}

// Wire-format lock: multi-byte fields are little-endian on the medium
// regardless of host byte order. On an LE host this passes with or without
// LE accessors in the core — it turns RED on big-endian targets running
// the pre-fix code (the BE CI job is issue #20/#24 scope).
static void test_wire_format_is_le(void) {
    fresh_fs(IMG_SIZE, 3);
    const uint8_t data[3] = {0xAA, 0xBB, 0xCC};
    assert(EEPROM_AddFile(image, image_size, "wire", data, 3) == 3);
    assert(add_pattern("second", 8, 7) == 8);

    // file header of "wire": dataSize @16 LE, crc32 @18 LE, next @22 LE
    const uint8_t *raw = image + HDR_V3;
    assert(raw[16] == 0x03 && raw[17] == 0x00);
    uint32_t expect_crc = (uint32_t) crc32(0L, data, 3);
    assert(raw[18] == (expect_crc & 0xFF));
    assert(raw[19] == ((expect_crc >> 8) & 0xFF));
    assert(raw[20] == ((expect_crc >> 16) & 0xFF));
    assert(raw[21] == ((expect_crc >> 24) & 0xFF));
    // next = 256 + 28 + 3 = 287 = 0x011F
    assert(raw[22] == 0x1F && raw[23] == 0x01);

    // headerCrc32 @24 is the LE encoding of crc32(header bytes 0..23)
    uint32_t expect_hcrc = (uint32_t) crc32(0L, raw, 24);
    assert(raw[24] == (expect_hcrc & 0xFF));
    assert(raw[25] == ((expect_hcrc >> 8) & 0xFF));
    assert(raw[26] == ((expect_hcrc >> 16) & 0xFF));
    assert(raw[27] == ((expect_hcrc >> 24) & 0xFF));

    // v3 header CRC field @252 is the LE encoding of crc32(bytes 0..251)
    uint8_t hdr[256];
    assert(EEPROM_GetHeader(image, image_size, hdr, sizeof(hdr)) == 0);
    uint32_t hdr_crc = (uint32_t) crc32(0L, hdr, 252);
    assert(hdr[252] == (hdr_crc & 0xFF));
    assert(hdr[253] == ((hdr_crc >> 8) & 0xFF));
    assert(hdr[254] == ((hdr_crc >> 16) & 0xFF));
    assert(hdr[255] == ((hdr_crc >> 24) & 0xFF));
    printf("  wire format is LE: OK\n");
}

// The header CRC catches corruption the field checks cannot: a flipped
// name byte used to read as a silent "different file".
static void test_header_crc_detects_corruption(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("victim", 10, 1) == 10);

    uint8_t evil = 'X';
    poke(HDR_V3 + 1, &evil, 1); // name[1], NOT resealed: real corruption
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == EEPROMCORRUPTED);
    printf("  header CRC detects corruption: OK\n");
}

// fs_version = 0: the file area is empty regardless of content; the first
// write stamps the current version and starts a fresh chain.
static void test_fs_version_zero_is_empty(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("old", 10, 1) == 10);

    force_fs_version(0);
    assert(EEPROM_HeaderCheckConsistency(image, image_size) == 1);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 0);
    uint8_t buf[64];
    assert(EEPROM_ReadFile(image, image_size, "old", buf, sizeof(buf)) == FILENOTFOUND);

    assert(add_pattern("fresh", 8, 2) == 8);
    assert(image[10] == 1); // stamped back
    assert(EEPROM_HeaderCheckConsistency(image, image_size) == 1);
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 1);
    assert(strcmp(names[0], "fresh") == 0);
    assert_file("fresh", 8, 2);
    printf("  fs_version 0 reads empty, write restamps: OK\n");
}

// An fs_version this build does not know is an explicit error, not a guess.
static void test_fs_version_unknown_rejected(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("a", 10, 1) == 10);

    force_fs_version(2);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == FSVERSIONNOTSUPPORTED);
    uint8_t buf[64];
    assert(EEPROM_ReadFile(image, image_size, "a", buf, sizeof(buf)) == FSVERSIONNOTSUPPORTED);
    assert(add_pattern("b", 4, 2) == FSVERSIONNOTSUPPORTED);
    printf("  unknown fs_version rejected: OK\n");
}

// SetHeader carries board identity only: a caller-built header with a zero
// fs_version must not make an existing filesystem invisible.
static void test_set_header_preserves_fs_version(void) {
    fresh_fs(IMG_SIZE, 3);
    assert(add_pattern("keep", 10, 1) == 10);

    uint8_t hdr[HDR_V3];
    assert(EEPROM_GetHeader(image, image_size, hdr, sizeof(hdr)) == 0);
    hdr[10] = 0; // caller "forgot" the filesystem version
    assert(EEPROM_SetHeader(image, image_size, hdr) == 0);

    assert(image[10] == 1);
    assert(EEPROM_HeaderCheckConsistency(image, image_size) == 1);
    char names[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, names, 8) == 1);
    assert_file("keep", 10, 1);
    printf("  SetHeader preserves fs_version: OK\n");
}

static void test_consistency_short_image(void) {
    // an image too small to even hold the version block is simply
    // inconsistent — with no I/O there is no read-error class (#25)
    memset(image, 0x00, sizeof(image));
    assert(EEPROM_HeaderCheckConsistency(image, 8) == 0);
    printf("  consistency short image: OK\n");
}

static void test_oversized_payload_rejected(void) {
    fresh_fs(IMG_SIZE, 3);
    uint8_t *big = malloc(40000);
    assert(big != NULL);
    memset(big, 1, 40000);
    // int16_t cannot represent the byte count: reject up front
    assert(EEPROM_AddFile(image, image_size, "big", big, 40000) == BUFFERNOTVALID);
    free(big);
    printf("  oversized payload rejected: OK\n");
}

int main(void) {
    printf("test_05 fscore:\n");
    test_format_and_header();
    test_empty_fs_lists_zero();
    test_add_list_read();
    test_overwrite();
    test_delete_chain_integrity();
    test_delete_last_and_only();
    test_corrupted_chain_terminates();
    test_oversized_datasize_rejected();
    test_link_to_eeprom_end_rejected();
    test_data_crc_checked_on_read();
    test_erased_next_is_terminal();
    test_erased_free_space_0xff();
    test_empty_datasize_is_corruption();
    test_delete_with_link_to_empty_slot();
    test_delete_first_with_successor_linking_empty();
    test_nospace_is_atomic();
    test_set_header_roundtrip();
    test_consistency_detects_bad_crc();
    test_wire_format_is_le();
    test_header_crc_detects_corruption();
    test_fs_version_zero_is_empty();
    test_fs_version_unknown_rejected();
    test_set_header_preserves_fs_version();
    test_consistency_short_image();
    test_oversized_payload_rejected();
    printf("test_05: OK\n");
    return 0;
}
