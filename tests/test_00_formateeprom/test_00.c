// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 */

/* Tests must assert in every build type, including -DNDEBUG ones. */
#undef NDEBUG
#include <assert.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEBUG 1

#include "debug.h"
#include "eepromerr.h"
#include "jeefs.h"
#include "tests-common.h"

void test0(int version, int expected_header_size);

static uint8_t image[TEST_EEPROM_SIZE];

static void prepare_eeprom_file(void) {
    int teeprom = open(TEST_FULL_EEPROM_FILENAME, O_CREAT | O_RDWR, 0666);
    assert(teeprom != -1);
    assert(ftruncate(teeprom, 0) == 0);
    assert(ftruncate(teeprom, TEST_EEPROM_SIZE) == 0);
    close(teeprom);
}

int main() {
    printf("Hello, World! DEBUG:%i\n", DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeaderv1) = %lu\n", sizeof(JEEPROMHeaderv1));
    printf("sizeof(JEEPROMHeaderv2) = %lu\n", sizeof(JEEPROMHeaderv2));
    printf("sizeof(JEEPROMHeaderv3) = %lu\n", sizeof(JEEPROMHeaderv3));
    printf("sizeof(JEEFSFileHeaderv1) = %lu\n", sizeof(JEEFSFileHeaderv1));

    char dir[1000];
    assert(getcwd(dir, sizeof(dir)) != NULL);
    debug("TEST_DIR: %s TEST_FILENAME: %s TEST_EEPROM_PATH: %s TEST_EEPROM_FILENAME: %s TEST_EEPROM_SIZE: %d\ncur_dir: "
          "%s\n",
          TEST_DIR, TEST_FILENAME, TEST_EEPROM_PATH, TEST_EEPROM_FILENAME, TEST_EEPROM_SIZE, dir);

    // Test v1 format
    prepare_eeprom_file();
    test0(1, sizeof(JEEPROMHeaderv1));
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 (v1) - "
           "passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    // Test v2 format
    prepare_eeprom_file();
    test0(2, sizeof(JEEPROMHeaderv2));
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 (v2) - "
           "passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    // Test v3 format
    prepare_eeprom_file();
    test0(3, sizeof(JEEPROMHeaderv3));
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 (v3) - "
           "passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    // Test v4 format (byte layout identical to v3, version byte = 4)
    prepare_eeprom_file();
    test0(4, sizeof(JEEPROMHeaderv4));
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 (v4) - "
           "passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    // Re-format with v1 for subsequent tests (test_01, test_02)
    prepare_eeprom_file();
    assert(image_load(TEST_FULL_EEPROM_FILENAME, image, TEST_EEPROM_SIZE) == 0);
    EEPROM_FormatEEPROM(image, TEST_EEPROM_SIZE, 1);
    assert(image_save(TEST_FULL_EEPROM_FILENAME, image, TEST_EEPROM_SIZE) == 0);

    return 0;
}


void test0(int version, int expected_header_size) {
    printf("\n--- Testing format with header version %d (header size %d) ---\n", version, expected_header_size);

    assert("Check image load" && image_load(TEST_FULL_EEPROM_FILENAME, image, TEST_EEPROM_SIZE) == 0);

    int EEPROM_consistency = EEPROM_HeaderCheckConsistency(image, TEST_EEPROM_SIZE);
    printf("Check EEPROM_header: %i\n", EEPROM_consistency);

    assert("Check EEPROM_header non consistency on empty file" && EEPROM_consistency == 0);
    EEPROM_FormatEEPROM(image, TEST_EEPROM_SIZE, version);
    assert(image_save(TEST_FULL_EEPROM_FILENAME, image, TEST_EEPROM_SIZE) == 0);

    assert(image_load(TEST_FULL_EEPROM_FILENAME, image, TEST_EEPROM_SIZE) == 0);
    EEPROM_consistency = EEPROM_HeaderCheckConsistency(image, TEST_EEPROM_SIZE);
    printf("test00: Check EEPROM_header (v%d): %i\n", version, EEPROM_consistency);
    assert("\nCheck EEPROM_header consistency failed" && EEPROM_consistency == 1);

    printf("Check EEPROM data consistency (after header at offset %d)\n", expected_header_size);
    for (size_t i = (size_t) expected_header_size; i < TEST_EEPROM_SIZE; i++) {
        assert("\nCheck EEPROM data consistency failed\n" && image[i] == JEEFS_EMPTYBYTE);
    }
}
