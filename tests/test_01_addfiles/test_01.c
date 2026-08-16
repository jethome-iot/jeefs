// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 */

/* Tests must assert in every build type, including -DNDEBUG ones. */
#undef NDEBUG
#include <assert.h>
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

static uint8_t image[TEST_EEPROM_SIZE];

void test1();

void test2();

int main() {
    printf("Hello, World! DEBUG:%i\n", DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeaderv1) = %lu\n", sizeof(JEEPROMHeaderv2));
    printf("sizeof(JEEFSFileHeader) = %lu\n", sizeof(JEEFSFileHeader));

    char dir[1000];
    assert(getcwd(dir, sizeof(dir)) != NULL);
    debug("TEST_DIR: %s TEST_FILENAME: %s TEST_EEPROM_PATH: %s TEST_EEPROM_FILENAME: %s TEST_EEPROM_SIZE: %d\ncur_dir: "
          "%s\n",
          TEST_DIR, TEST_FILENAME, TEST_EEPROM_PATH, TEST_EEPROM_FILENAME, TEST_EEPROM_SIZE, dir);

    // Test 1: open, write, close
    test1();

    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 1 - "
           "passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    return 0;
}


void test1() {
    assert("Check image load" && image_load(TEST_FULL_EEPROM_FILENAME, image, TEST_EEPROM_SIZE) == 0);

    int EEPROM_consistency = EEPROM_HeaderCheckConsistency(image, TEST_EEPROM_SIZE);
    printf("Check EEPROM_header: %i\n", EEPROM_consistency);
    EEPROM_FormatEEPROM(image, TEST_EEPROM_SIZE, 1);
    if (EEPROM_consistency < 0) {
        printf("EEPROM header is not consistent, format EEPROM\n");
        // TODO: check error code
        EEPROM_FormatEEPROM(image, TEST_EEPROM_SIZE, 1);
    }


    char filename[100];
    uint8_t filedata[8192];
    uint16_t filesize;
    int err;
    size_t i;
    for (i = 0; i < sizeof(test_files) / sizeof(char *); i++) {
        sprintf(filename, "%s_%zu", TEST_FILENAME, i);
        printf("!!!!++++ Add new file %s\n", filename);
        filesize = strlen(test_files[i]) + 1;
        memcpy(filedata, test_files[i], filesize);

        err = EEPROM_AddFile(image, TEST_EEPROM_SIZE, filename, filedata, strlen(test_files[i]) + 1);
        printf("EEPROM_AddFile: %i\n", err);
        fflush(stdout);
        if (err == NOTENOUGHSPACE)
            break; // EEPROM full: expected once ~10 files are in
        assert("Check EEPROM_AddFile wrote the file" && err == (int) filesize);

        printf("File %zu: %s size:%i\n", i, filename, filesize);
        memset(filedata, 0, sizeof(filedata));
    }
    printf("Files count:%zu\n", i);
    assert("Check adds stopped on NOTENOUGHSPACE" && err == NOTENOUGHSPACE);
    assert("Check the expected number of files fit" && i == 10);
    int consistency = EEPROM_HeaderCheckConsistency(image, TEST_EEPROM_SIZE);
    assert("Check EEPROM_header consistency after adds" && consistency == 1);

    // persist for test_02 (the fixture file carries state between binaries)
    assert("Check image save" && image_save(TEST_FULL_EEPROM_FILENAME, image, TEST_EEPROM_SIZE) == 0);
}
