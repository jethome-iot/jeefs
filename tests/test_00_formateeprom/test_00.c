// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#define DEBUG 1

#include "jeefs.h"
#include "tests-common.h"
#include "debug.h"
#include "eepromerr.h"

void test0(int version, int expected_header_size);

static void prepare_eeprom_file(void) {
    int teeprom = open(TEST_FULL_EEPROM_FILENAME, O_CREAT | O_RDWR, 0666);
    ftruncate(teeprom, 0);
    ftruncate(teeprom, TEST_EEPROM_SIZE);
    close(teeprom);
}

int main() {
    printf("Hello, World! DEBUG:%i\n",DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeaderv1) = %lu\n", sizeof(JEEPROMHeaderv1));
    printf("sizeof(JEEPROMHeaderv2) = %lu\n", sizeof(JEEPROMHeaderv2));
    printf("sizeof(JEEPROMHeaderv3) = %lu\n", sizeof(JEEPROMHeaderv3));
    printf("sizeof(JEEFSFileHeaderv1) = %lu\n", sizeof(JEEFSFileHeaderv1));

    char dir[1000];
    getcwd(dir, sizeof(dir));
    debug("TEST_DIR: %s TEST_FILENAME: %s TEST_EEPROM_PATH: %s TEST_EEPROM_FILENAME: %s TEST_EEPROM_SIZE: %d\ncur_dir: %s\n",
          TEST_DIR, TEST_FILENAME, TEST_EEPROM_PATH, TEST_EEPROM_FILENAME, TEST_EEPROM_SIZE, dir);

    // Test v1 format
    prepare_eeprom_file();
    test0(1, sizeof(JEEPROMHeaderv1));
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 (v1) - passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    // Test v2 format
    prepare_eeprom_file();
    test0(2, sizeof(JEEPROMHeaderv2));
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 (v2) - passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    // Test v3 format
    prepare_eeprom_file();
    test0(3, sizeof(JEEPROMHeaderv3));
    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 (v3) - passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");

    // Re-format with v1 for subsequent tests (test_01, test_02)
    prepare_eeprom_file();
    EEPROMDescriptor ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);
    EEPROM_FormatEEPROM(ep, 1);
    EEPROM_CloseEEPROM(ep);

    return 0;
}


void test0(int version, int expected_header_size) {
    printf("\n--- Testing format with header version %d (header size %d) ---\n",
           version, expected_header_size);

    EEPROMDescriptor ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);
    assert(("Check eeprom_open result", ep.eeprom_fid > 0));
    assert(("Check eeprom_open result size = 8192", ep.eeprom_size == TEST_EEPROM_SIZE));
    printf("EEPROM opened, size: %lu\n", ep.eeprom_size);

    int EEPROM_consistency = EEPROM_HeaderCheckConsistency(ep);
    printf("Check EEPROM_header: %i\n", EEPROM_consistency);

    assert("Check EEPROM_header non consistency on empty file" && EEPROM_consistency != 0);
    EEPROM_FormatEEPROM(ep, version);
    EEPROM_CloseEEPROM(ep);

    ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);
    EEPROM_consistency = EEPROM_HeaderCheckConsistency(ep);
    printf("test00: Check EEPROM_header (v%d): %i\n", version, EEPROM_consistency);
    assert("\nCheck EEPROM_header consistency failed" && EEPROM_consistency == 0);

    int8_t buf[ep.eeprom_size], buf2[ep.eeprom_size];
    memset(buf, EEPROM_EMPTYBYTE, ep.eeprom_size);
    lseek(ep.eeprom_fid, 0, SEEK_SET);
    assert(read(ep.eeprom_fid, buf2, ep.eeprom_size) == ep.eeprom_size);
    printf("Check EEPROM data consistency (after header at offset %d)\n", expected_header_size);
    for (int i = expected_header_size; i < ep.eeprom_size; i++) {
        assert("\nCheck EEPROM data consistency failed\n" && buf[i] == buf2[i]);
    }

    EEPROM_CloseEEPROM(ep);
}

