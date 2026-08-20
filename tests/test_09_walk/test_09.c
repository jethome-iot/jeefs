// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * Pull-model walker (#81): the walker owns the state machine and the
 * validation, the environment owns every read. These tests feed the
 * walker from an in-memory image built by the FS API and check that it
 * agrees with the in-memory iterator on every terminal state.
 */

/* Tests must assert in every build type, including -DNDEBUG ones. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "eepromerr.h"
#include "jeefs.h"
#include "jeefs_port.h"
#include "jeefs_walk.h"

#define IMG_SIZE 2048
#define HDR 256

static uint8_t image[IMG_SIZE];

static void fill_pattern(uint8_t *buf, uint16_t n, uint8_t seed) {
    for (uint16_t i = 0; i < n; i++)
        buf[i] = (uint8_t) (seed + i);
}

static void fresh_fs_with(const char *const *names, const uint16_t *sizes, int count) {
    memset(image, 0, sizeof(image));
    assert(EEPROM_FormatEEPROM(image, IMG_SIZE, 4) == 0);
    for (int i = 0; i < count; i++) {
        uint8_t buf[512];
        assert(sizes[i] <= sizeof(buf));
        fill_pattern(buf, sizes[i], (uint8_t) (i + 1));
        assert(EEPROM_AddFile(image, IMG_SIZE, names[i], buf, sizes[i]) == (int16_t) sizes[i]);
    }
}

// Drive the walker to a terminal state, feeding it from the image and
// counting the header reads. Returns the terminal state.
static int16_t drive(JEEFSWalk *w, int *hops) {
    uint32_t off;
    uint16_t len;
    if (hops)
        *hops = 0;
    while (jeefs_walk_want(w, &off, &len) == 1) {
        assert(len == sizeof(JEEFSFileHeaderv1));
        assert(off + len <= IMG_SIZE);
        if (hops)
            (*hops)++;
        if (jeefs_walk_feed(w, image + off, len) != 0)
            break;
    }
    return w->state;
}

static void test_found_matches_readfile(void) {
    const char *names[] = {"alpha", "beta", "gamma"};
    const uint16_t sizes[] = {40, 200, 64};
    fresh_fs_with(names, sizes, 3);

    JEEFSWalk w;
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "beta") == 0);
    int hops = 0;
    assert(drive(&w, &hops) == JEEFS_WALK_FOUND);
    assert(hops == 2); // alpha, then beta

    uint8_t expect[512];
    int16_t r = EEPROM_ReadFile(image, IMG_SIZE, "beta", expect, sizeof(expect));
    assert(r == 200);
    assert(w.file_size == 200);
    assert(memcmp(image + w.file_offset, expect, 200) == 0);
    // stream verification via the running CRC form
    uint32_t crc = 0;
    for (uint32_t o = 0; o < w.file_size; o += 64) {
        uint32_t n = w.file_size - o < 64 ? w.file_size - o : 64;
        crc = jeefs_crc32_update(crc, image + w.file_offset + o, n);
    }
    assert(crc == w.file_crc32);
    printf("  found matches ReadFile: OK\n");
}

static void test_not_found(void) {
    const char *names[] = {"alpha", "beta"};
    const uint16_t sizes[] = {10, 20};
    fresh_fs_with(names, sizes, 2);

    JEEFSWalk w;
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "nosuch") == 0);
    int hops = 0;
    assert(drive(&w, &hops) == JEEFS_WALK_NOTFOUND);
    assert(hops == 2); // alpha, beta; a zero link ends without another read
    printf("  not found: OK\n");
}

static void test_device_id_is_one_hop(void) {
    const char *names[] = {"cfg", "wifi"};
    const uint16_t sizes[] = {16, 8};
    fresh_fs_with(names, sizes, 2);
    uint8_t rec[256];
    fill_pattern(rec, sizeof(rec), 7);
    assert(EEPROM_AddFile(image, IMG_SIZE, JEEFS_DEVICE_ID_FILENAME, rec, sizeof(rec)) == 256);

    JEEFSWalk w;
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, JEEFS_DEVICE_ID_FILENAME) == 0);
    int hops = 0;
    assert(drive(&w, &hops) == JEEFS_WALK_FOUND);
    assert(hops == 1); // insert-first guarantees the bounded prefix
    assert(w.file_offset == HDR + sizeof(JEEFSFileHeaderv1));
    printf("  device.id in one hop: OK\n");
}

static void test_fs_version_gates(void) {
    const char *names[] = {"alpha"};
    const uint16_t sizes[] = {10};
    fresh_fs_with(names, sizes, 1);

    JEEFSWalk w;
    image[10] = 0; // walker trusts the caller-verified prefix bytes
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "alpha") == 0);
    assert(drive(&w, NULL) == JEEFS_WALK_NOTFOUND); // no filesystem: zero hops
    image[10] = 2;
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "alpha") == FSVERSIONNOTSUPPORTED);
    printf("  fs_version gate: OK\n");
}

static void test_corruption_detected(void) {
    const char *names[] = {"alpha", "beta"};
    const uint16_t sizes[] = {10, 20};
    fresh_fs_with(names, sizes, 2);
    image[HDR + 1] ^= 0x40; // flip a name byte, CRC not resealed

    JEEFSWalk w;
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "beta") == 0);
    assert(drive(&w, NULL) == EEPROMCORRUPTED);
    printf("  corruption detected: OK\n");
}

static void test_begin_rejects_bad_input(void) {
    const char *names[] = {"alpha"};
    const uint16_t sizes[] = {10};
    fresh_fs_with(names, sizes, 1);

    JEEFSWalk w;
    assert(jeefs_walk_begin(&w, image, 8, IMG_SIZE, "alpha") == BUFFERNOTVALID); // prefix < 12
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "") == FILENAMENOTVALID);
    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "name-way-too-long") == FILENAMENOTVALID);
    uint8_t junk[16];
    memset(junk, 0xAB, sizeof(junk));
    assert(jeefs_walk_begin(&w, junk, sizeof(junk), IMG_SIZE, "alpha") == EEPROMCORRUPTED);

    assert(jeefs_walk_begin(&w, image, HDR, IMG_SIZE, "alpha") == 0);
    uint8_t chunk[sizeof(JEEFSFileHeaderv1)];
    memcpy(chunk, image + HDR, sizeof(chunk));
    assert(jeefs_walk_feed(&w, chunk, 12) == BUFFERNOTVALID); // wrong feed length
    printf("  begin/feed input validation: OK\n");
}

static void test_crc32_update_equivalence(void) {
    uint8_t buf[300];
    fill_pattern(buf, sizeof(buf), 0x5A);
    uint32_t whole = jeefs_crc32(buf, sizeof(buf));
    uint32_t split = jeefs_crc32_update(0, buf, 100);
    split = jeefs_crc32_update(split, buf + 100, 1);
    split = jeefs_crc32_update(split, buf + 101, 199);
    assert(split == whole);
    assert(jeefs_crc32_update(0, buf, 0) == 0); // empty update is identity
    printf("  crc32_update equivalence: OK\n");
}

int main(void) {
    printf("test_09_walk:\n");
    test_found_matches_readfile();
    test_not_found();
    test_device_id_is_one_hop();
    test_fs_version_gates();
    test_corruption_detected();
    test_begin_rejects_bad_input();
    test_crc32_update_equivalence();
    printf("test_09_walk: all OK\n");
    return 0;
}
