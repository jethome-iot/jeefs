// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * FS-core test suite: positive paths, chain integrity across delete,
 * negative paths on corrupted images (issues #5, #6, #7, #9, #14, #18).
 * Every scenario runs on its own freshly created image.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eepromerr.h"
#include "jeefs.h"

#define IMG TEST_DIR "/test_05_eeprom.bin"
#define IMG_SIZE 8192
#define HDR_V3 256

static void make_image(const char *path, uint16_t size, uint8_t fill) {
    FILE *f = fopen(path, "wb");
    assert(f != NULL);
    uint8_t *buf = malloc(size);
    assert(buf != NULL);
    memset(buf, fill, size);
    size_t written = fwrite(buf, 1, size, f);
    assert(written == size);
    free(buf);
    fclose(f);
}

static EEPROMDescriptor fresh_fs(uint16_t size, int version) {
    make_image(IMG, size, 0x00);
    EEPROMDescriptor ep = EEPROM_OpenEEPROM(IMG, size);
    assert(ep.eeprom_fid != -1);
    assert(EEPROM_FormatEEPROM(ep, version) == 0);
    return ep;
}

static void fill_pattern(uint8_t *buf, uint16_t n, uint8_t seed) {
    for (uint16_t i = 0; i < n; i++)
        buf[i] = (uint8_t)(seed + i);
}

static int16_t add_pattern(EEPROMDescriptor ep, const char *name, uint16_t n,
                           uint8_t seed) {
    uint8_t buf[512];
    assert(n <= sizeof(buf));
    fill_pattern(buf, n, seed);
    return EEPROM_AddFile(ep, name, buf, n);
}

static void assert_file(EEPROMDescriptor ep, const char *name, uint16_t n,
                        uint8_t seed) {
    uint8_t expect[512], got[512];
    assert(n <= sizeof(expect));
    fill_pattern(expect, n, seed);
    int16_t r = EEPROM_ReadFile(ep, name, got, sizeof(got));
    assert(r == (int16_t)n);
    assert(memcmp(expect, got, n) == 0);
}

// Raw corruption helper: overwrite bytes at an absolute offset.
static void poke(EEPROMDescriptor ep, uint16_t off, const void *data,
                 uint16_t n) {
    assert(eeprom_write(ep, (void *)data, n, off) == n);
}

static void test_format_and_header(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(EEPROM_HeaderCheckConsistency(ep) == 1);
    uint8_t hdr[HDR_V3];
    assert(EEPROM_GetHeader(ep, hdr, sizeof(hdr)) == 0);
    assert(memcmp(hdr, "JETHOME\0", 8) == 0);
    // buffer smaller than the header must be rejected
    uint8_t small[64];
    assert(EEPROM_GetHeader(ep, small, sizeof(small)) < 0);
    EEPROM_CloseEEPROM(ep);
    printf("  format/header: OK\n");
}

static void test_empty_fs_lists_zero(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == 0);
    EEPROM_CloseEEPROM(ep);
    printf("  empty list: OK\n");
}

static void test_add_list_read(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "alpha", 10, 1) == 10);
    assert(add_pattern(ep, "beta", 20, 2) == 20);
    assert(add_pattern(ep, "gamma", 30, 3) == 30);

    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == 3);
    assert(strcmp(names[0], "alpha") == 0);
    assert(strcmp(names[1], "beta") == 0);
    assert(strcmp(names[2], "gamma") == 0);

    assert_file(ep, "alpha", 10, 1);
    assert_file(ep, "beta", 20, 2);
    assert_file(ep, "gamma", 30, 3);

    // duplicate add: 0, content untouched
    assert(add_pattern(ep, "beta", 20, 99) == 0);
    assert_file(ep, "beta", 20, 2);

    // 15-char name round-trips NUL-terminated
    assert(add_pattern(ep, "abcdefghijklmno", 8, 4) == 8);
    assert(EEPROM_ListFiles(ep, names, 8) == 4);
    assert(strcmp(names[3], "abcdefghijklmno") == 0);

    // maxFiles above INT16_MAX must not be misread as negative
    char big_list[4][FILE_NAME_LENGTH + 1];
    (void)big_list;
    assert(EEPROM_ListFiles(ep, big_list, 65535) == 4);

    // invalid names
    uint8_t d[4] = {1, 2, 3, 4};
    assert(EEPROM_AddFile(ep, NULL, d, 4) == FILENAMENOTVALID);
    assert(EEPROM_AddFile(ep, "", d, 4) == FILENAMENOTVALID);
    assert(EEPROM_AddFile(ep, "abcdefghijklmnop", d, 4) == FILENAMENOTVALID);
    // invalid data
    assert(EEPROM_AddFile(ep, "x", NULL, 4) == BUFFERNOTVALID);
    assert(EEPROM_AddFile(ep, "x", d, 0) == BUFFERNOTVALID);
    EEPROM_CloseEEPROM(ep);
    printf("  add/list/read: OK\n");
}

static void test_overwrite(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "a", 10, 1) == 10);
    assert(add_pattern(ep, "b", 20, 2) == 20);
    assert(add_pattern(ep, "c", 30, 3) == 30);

    // same size: in-place
    uint8_t nb[20];
    fill_pattern(nb, 20, 50);
    assert(EEPROM_WriteFile(ep, "b", nb, 20) == 20);
    assert_file(ep, "b", 20, 50);
    assert_file(ep, "a", 10, 1);
    assert_file(ep, "c", 30, 3);

    // different size: delete + add, siblings intact
    uint8_t wb[40];
    fill_pattern(wb, 40, 60);
    assert(EEPROM_WriteFile(ep, "b", wb, 40) == 40);
    assert_file(ep, "b", 40, 60);
    assert_file(ep, "a", 10, 1);
    assert_file(ep, "c", 30, 3);
    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == 3);

    // missing file
    assert(EEPROM_WriteFile(ep, "nope", nb, 20) == FILENOTFOUND);
    EEPROM_CloseEEPROM(ep);
    printf("  overwrite: OK\n");
}

// The audit's data-loss reproduction: A -> D -> B -> C, delete D, then add.
static void test_delete_chain_integrity(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "A", 16, 1) == 16);
    assert(add_pattern(ep, "D", 24, 2) == 24);
    assert(add_pattern(ep, "B", 32, 3) == 32);
    assert(add_pattern(ep, "C", 48, 4) == 48);

    assert(EEPROM_DeleteFile(ep, "D") == 1);

    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == 3);
    assert(strcmp(names[0], "A") == 0);
    assert(strcmp(names[1], "B") == 0);
    assert(strcmp(names[2], "C") == 0);
    assert_file(ep, "A", 16, 1);
    assert_file(ep, "B", 32, 3);
    assert_file(ep, "C", 48, 4);

    // adding after the delete must not clobber surviving files
    assert(add_pattern(ep, "E", 8, 5) == 8);
    assert(EEPROM_ListFiles(ep, names, 8) == 4);
    assert_file(ep, "B", 32, 3);
    assert_file(ep, "C", 48, 4);
    assert_file(ep, "E", 8, 5);
    EEPROM_CloseEEPROM(ep);
    printf("  delete chain integrity: OK\n");
}

static void test_delete_last_and_only(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "a", 10, 1) == 10);
    assert(add_pattern(ep, "b", 20, 2) == 20);
    assert(add_pattern(ep, "c", 30, 3) == 30);

    // delete last: predecessor becomes terminal
    assert(EEPROM_DeleteFile(ep, "c") == 1);
    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == 2);
    assert_file(ep, "a", 10, 1);
    assert_file(ep, "b", 20, 2);
    assert(add_pattern(ep, "d", 12, 4) == 12);
    assert(EEPROM_ListFiles(ep, names, 8) == 3);

    // delete down to empty
    assert(EEPROM_DeleteFile(ep, "a") == 1);
    assert(EEPROM_DeleteFile(ep, "b") == 1);
    assert(EEPROM_DeleteFile(ep, "d") == 1);
    assert(EEPROM_ListFiles(ep, names, 8) == 0);
    assert(add_pattern(ep, "fresh", 10, 7) == 10);
    assert(EEPROM_ListFiles(ep, names, 8) == 1);
    assert_file(ep, "fresh", 10, 7);

    assert(EEPROM_DeleteFile(ep, "nope") == FILENOTFOUND);
    EEPROM_CloseEEPROM(ep);
    printf("  delete last/only: OK\n");
}

static void test_corrupted_chain_terminates(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "a", 10, 1) == 10);
    assert(add_pattern(ep, "b", 20, 2) == 20);

    // nextFileAddress of "a" points back at "a": a cycle. Offset of the
    // nextFileAddress field inside JEEFSFileHeaderv1 is 22.
    uint16_t self = HDR_V3;
    poke(ep, HDR_V3 + 22, &self, 2);

    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == EEPROMCORRUPTED);
    uint8_t buf[64];
    assert(EEPROM_ReadFile(ep, "b", buf, sizeof(buf)) == EEPROMCORRUPTED);
    assert(EEPROM_DeleteFile(ep, "b") == EEPROMCORRUPTED);
    EEPROM_CloseEEPROM(ep);
    printf("  corrupted chain terminates: OK\n");
}

static void test_oversized_datasize_rejected(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "a", 10, 1) == 10);

    // dataSize of "a" -> 0xFF00: out of bounds for an 8K image. Offset of
    // dataSize inside JEEFSFileHeaderv1 is 16.
    uint16_t huge = 0xFF00;
    poke(ep, HDR_V3 + 16, &huge, 2);

    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == EEPROMCORRUPTED);
    assert(EEPROM_DeleteFile(ep, "a") == EEPROMCORRUPTED);
    uint8_t buf[64];
    assert(EEPROM_ReadFile(ep, "a", buf, sizeof(buf)) == EEPROMCORRUPTED);
    EEPROM_CloseEEPROM(ep);
    printf("  oversized dataSize rejected: OK\n");
}

// A non-terminal link must point where a file header can actually fit: a
// link to the exact EEPROM end passes naive contiguity but has no room for
// a successor — corrupted, and must not send DeleteFile into a relink loop.
static void test_link_to_eeprom_end_rejected(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "a", 10, 1) == 10);

    // dataSize of "a" -> spans to EEPROM end; next -> exactly eeprom_size
    uint16_t span = IMG_SIZE - HDR_V3 - 24;
    uint16_t end = IMG_SIZE;
    poke(ep, HDR_V3 + 16, &span, 2);
    poke(ep, HDR_V3 + 22, &end, 2);

    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == EEPROMCORRUPTED);
    assert(EEPROM_DeleteFile(ep, "a") == EEPROMCORRUPTED);
    EEPROM_CloseEEPROM(ep);
    printf("  link to EEPROM end rejected: OK\n");
}

static void test_data_crc_checked_on_read(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "a", 32, 1) == 32);

    // flip one data byte (data starts right after the 24-byte file header)
    uint8_t evil = 0xEE;
    poke(ep, HDR_V3 + 24 + 5, &evil, 1);

    uint8_t buf[64];
    assert(EEPROM_ReadFile(ep, "a", buf, sizeof(buf)) == EEPROMCORRUPTED);
    EEPROM_CloseEEPROM(ep);
    printf("  data CRC on read: OK\n");
}

// RFC #14: an erased (0xFFFF) nextFileAddress terminates the chain like 0.
static void test_erased_next_is_terminal(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(add_pattern(ep, "a", 10, 1) == 10);
    assert(add_pattern(ep, "b", 20, 2) == 20);

    // erase the link of "b" (the last file): 0xFFFF instead of 0
    uint16_t b_addr = HDR_V3 + 24 + 10;
    uint16_t erased = 0xFFFF;
    poke(ep, b_addr + 22, &erased, 2);

    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == 2);
    assert_file(ep, "b", 20, 2);
    // adding after an erased terminal keeps the chain intact
    assert(add_pattern(ep, "c", 8, 3) == 8);
    assert(EEPROM_ListFiles(ep, names, 8) == 3);
    assert_file(ep, "b", 20, 2);
    assert_file(ep, "c", 8, 3);
    EEPROM_CloseEEPROM(ep);
    printf("  erased next is terminal: OK\n");
}

static void test_erased_free_space_0xff(void) {
    // Erased medium: everything after the formatted header reads 0xFF.
    make_image(IMG, IMG_SIZE, 0xFF);
    EEPROMDescriptor ep = EEPROM_OpenEEPROM(IMG, IMG_SIZE);
    assert(ep.eeprom_fid != -1);
    assert(EEPROM_FormatEEPROM(ep, 3) == 0);
    // Re-erase the file area to 0xFF (format may have zero-filled it; both
    // fills are legal empty space per issue #14).
    uint8_t ff[64];
    memset(ff, 0xFF, sizeof(ff));
    for (uint16_t off = HDR_V3; off < IMG_SIZE; off += sizeof(ff))
        poke(ep, off, ff, sizeof(ff));

    char names[8][FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(ep, names, 8) == 0);
    assert(add_pattern(ep, "a", 10, 1) == 10);
    assert(add_pattern(ep, "b", 20, 2) == 20);
    assert(EEPROM_ListFiles(ep, names, 8) == 2);
    assert_file(ep, "a", 10, 1);
    assert_file(ep, "b", 20, 2);
    EEPROM_CloseEEPROM(ep);
    printf("  0xFF erased free space: OK\n");
}

static void test_nospace_is_atomic(void) {
    // 512-byte image, v2 header (256): room for one ~200-byte file.
    make_image(IMG, 512, 0x00);
    EEPROMDescriptor ep = EEPROM_OpenEEPROM(IMG, 512);
    assert(ep.eeprom_fid != -1);
    assert(EEPROM_FormatEEPROM(ep, 2) == 0);

    uint8_t big[300];
    fill_pattern(big, sizeof(big), 9);
    assert(add_pattern(ep, "f", 200, 1) == 200);
    assert(EEPROM_AddFile(ep, "g", big, sizeof(big)) == NOTENOUGHSPACE);

    // growing "f" beyond free space must not destroy it
    assert(EEPROM_WriteFile(ep, "f", big, sizeof(big)) == NOTENOUGHSPACE);
    assert_file(ep, "f", 200, 1);
    EEPROM_CloseEEPROM(ep);
    printf("  NOTENOUGHSPACE atomicity: OK\n");
}

static void test_set_header_roundtrip(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    uint8_t hdr[HDR_V3];
    assert(EEPROM_GetHeader(ep, hdr, sizeof(hdr)) == 0);
    // boardname lives at offset 12
    memcpy(hdr + 12, "test-board", 11);
    assert(EEPROM_SetHeader(ep, hdr) == 0);

    uint8_t back[HDR_V3];
    assert(EEPROM_GetHeader(ep, back, sizeof(back)) == 0);
    assert(memcmp(back + 12, "test-board", 11) == 0);
    assert(EEPROM_HeaderCheckConsistency(ep) == 1);

    // bad magic is rejected
    uint8_t junk[HDR_V3];
    memset(junk, 0xAB, sizeof(junk));
    assert(EEPROM_SetHeader(ep, junk) < 0);
    EEPROM_CloseEEPROM(ep);
    printf("  SetHeader roundtrip: OK\n");
}

static void test_consistency_detects_bad_crc(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    assert(EEPROM_HeaderCheckConsistency(ep) == 1);
    uint8_t evil = 0x5A;
    poke(ep, 253, &evil, 1); // inside the v3 crc32 field (252-255)
    assert(EEPROM_HeaderCheckConsistency(ep) == 0);
    EEPROM_CloseEEPROM(ep);
    printf("  consistency detects bad CRC: OK\n");
}

static void test_oversized_payload_rejected(void) {
    EEPROMDescriptor ep = fresh_fs(IMG_SIZE, 3);
    uint8_t *big = malloc(40000);
    assert(big != NULL);
    memset(big, 1, 40000);
    // int16_t cannot represent the byte count: reject up front
    assert(EEPROM_AddFile(ep, "big", big, 40000) == BUFFERNOTVALID);
    free(big);
    EEPROM_CloseEEPROM(ep);
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
    test_nospace_is_atomic();
    test_set_header_roundtrip();
    test_consistency_detects_bad_crc();
    test_oversized_payload_rejected();
    printf("test_05: OK\n");
    return 0;
}
