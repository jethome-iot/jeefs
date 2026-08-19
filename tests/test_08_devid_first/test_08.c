// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * The reserved device-identity file (device.id) always occupies the first
 * slot after the header, so a boot environment can read the whole identity
 * as a bounded prefix (issues #80, #81). EEPROM_AddFile inserts it first,
 * shifting an existing chain up; every other operation preserves the
 * position naturally.
 */

/* Tests must assert in every build type, including -DNDEBUG ones. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "eepromerr.h"
#include "jeefs.h"

#define IMG_SIZE 8192
#define HDR_V4 256

static uint8_t image[IMG_SIZE];
static uint16_t image_size = IMG_SIZE;

static uint8_t *fresh_fs(uint16_t size) {
    memset(image, 0x00, sizeof(image));
    image_size = size;
    assert(EEPROM_FormatEEPROM(image, size, 4) == 0);
    return image;
}

static void fill_pattern(uint8_t *buf, uint16_t n, uint8_t seed) {
    for (uint16_t i = 0; i < n; i++)
        buf[i] = (uint8_t) (seed + i);
}

static int16_t add_pattern(const char *name, uint16_t n, uint8_t seed) {
    uint8_t buf[1024];
    assert(n <= sizeof(buf));
    fill_pattern(buf, n, seed);
    return EEPROM_AddFile(image, image_size, name, buf, n);
}

static void assert_file(const char *name, uint16_t n, uint8_t seed) {
    uint8_t expect[1024], got[1024];
    assert(n <= sizeof(expect));
    fill_pattern(expect, n, seed);
    int16_t r = EEPROM_ReadFile(image, image_size, name, got, sizeof(got));
    assert(r == (int16_t) n);
    assert(memcmp(expect, got, n) == 0);
}

// The identity file must sit immediately after the header: the name field
// is the first member of the file header, compare it raw at that offset.
static void assert_devid_first(void) {
    assert(memcmp(image + HDR_V4, JEEFS_DEVICE_ID_FILENAME, sizeof(JEEFS_DEVICE_ID_FILENAME)) == 0);
}

static void assert_list(const char *const *names, int16_t count) {
    char got[8][JEEFS_FILE_NAME_LENGTH + 1];
    assert(EEPROM_ListFiles(image, image_size, got, 8) == count);
    for (int16_t i = 0; i < count; i++)
        assert(strcmp(got[i], names[i]) == 0);
}

static void test_add_to_empty_fs(void) {
    fresh_fs(IMG_SIZE);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == 256);
    assert_devid_first();
    assert_file(JEEFS_DEVICE_ID_FILENAME, 256, 0x10);
    printf("  add to empty fs: OK\n");
}

static void test_insert_into_populated_fs(void) {
    fresh_fs(IMG_SIZE);
    assert(add_pattern("alpha", 100, 1) == 100);
    assert(add_pattern("beta", 200, 2) == 200);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == 256);

    assert_devid_first();
    const char *want[] = {JEEFS_DEVICE_ID_FILENAME, "alpha", "beta"};
    assert_list(want, 3);
    assert_file(JEEFS_DEVICE_ID_FILENAME, 256, 0x10);
    assert_file("alpha", 100, 1);
    assert_file("beta", 200, 2);
    printf("  insert into populated fs: OK\n");
}

static void test_regular_add_keeps_devid_first(void) {
    fresh_fs(IMG_SIZE);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == 256);
    assert(add_pattern("alpha", 50, 3) == 50);

    assert_devid_first();
    const char *want[] = {JEEFS_DEVICE_ID_FILENAME, "alpha"};
    assert_list(want, 2);
    assert_file("alpha", 50, 3);
    printf("  regular add keeps it first: OK\n");
}

static void test_delete_keeps_devid_first(void) {
    fresh_fs(IMG_SIZE);
    assert(add_pattern("alpha", 100, 1) == 100);
    assert(add_pattern("beta", 200, 2) == 200);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == 256);

    assert(EEPROM_DeleteFile(image, image_size, "alpha") == 1);
    assert_devid_first();
    const char *want[] = {JEEFS_DEVICE_ID_FILENAME, "beta"};
    assert_list(want, 2);
    assert_file(JEEFS_DEVICE_ID_FILENAME, 256, 0x10);
    assert_file("beta", 200, 2);
    printf("  delete keeps it first: OK\n");
}

static void test_existing_devid_returns_zero(void) {
    fresh_fs(IMG_SIZE);
    assert(add_pattern("alpha", 100, 1) == 100);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == 256);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x20) == 0); // already exists

    assert_devid_first();
    assert_file(JEEFS_DEVICE_ID_FILENAME, 256, 0x10); // content untouched
    assert_file("alpha", 100, 1);
    printf("  existing device.id untouched: OK\n");
}

static void test_insert_not_enough_space(void) {
    fresh_fs(1024); // 768 bytes of file area
    assert(add_pattern("alpha", 700, 1) == 700);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == NOTENOUGHSPACE);

    // The image must be untouched by the failed insert.
    const char *want[] = {"alpha"};
    assert_list(want, 1);
    assert_file("alpha", 700, 1);
    printf("  not enough space: OK\n");
}

static void test_write_same_size_stays_first(void) {
    fresh_fs(IMG_SIZE);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == 256);
    assert(add_pattern("alpha", 100, 1) == 100);

    uint8_t buf[256];
    fill_pattern(buf, sizeof(buf), 0x30);
    assert(EEPROM_WriteFile(image, image_size, JEEFS_DEVICE_ID_FILENAME, buf, sizeof(buf)) == 256);

    assert_devid_first();
    assert_file(JEEFS_DEVICE_ID_FILENAME, 256, 0x30);
    assert_file("alpha", 100, 1);
    printf("  same-size write stays first: OK\n");
}

static void test_write_diff_size_reinserts_first(void) {
    fresh_fs(IMG_SIZE);
    assert(add_pattern(JEEFS_DEVICE_ID_FILENAME, 256, 0x10) == 256);
    assert(add_pattern("alpha", 100, 1) == 100);

    // A different size re-creates the file; it must come back FIRST, not last.
    uint8_t buf[300];
    fill_pattern(buf, sizeof(buf), 0x40);
    assert(EEPROM_WriteFile(image, image_size, JEEFS_DEVICE_ID_FILENAME, buf, sizeof(buf)) == 300);

    assert_devid_first();
    const char *want[] = {JEEFS_DEVICE_ID_FILENAME, "alpha"};
    assert_list(want, 2);
    assert_file(JEEFS_DEVICE_ID_FILENAME, 300, 0x40);
    assert_file("alpha", 100, 1);
    printf("  diff-size write reinserts first: OK\n");
}

int main(void) {
    printf("test_08_devid_first:\n");
    test_add_to_empty_fs();
    test_insert_into_populated_fs();
    test_regular_add_keeps_devid_first();
    test_delete_keeps_devid_first();
    test_existing_devid_returns_zero();
    test_insert_not_enough_space();
    test_write_same_size_stays_first();
    test_write_diff_size_reinserts_first();
    printf("test_08_devid_first: all OK\n");
    return 0;
}
