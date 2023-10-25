// SPDX-License-Identifier: (GPL-2.0+ or MIT)
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

void test1();

void test2();

int main() {
    printf("Hello, World! DEBUG:%i\n",DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeader) = %lu\n", sizeof(JEEPROMHeader));
    printf("sizeof(JEEFSFileHeader) = %lu\n", sizeof(JEEFSFileHeader));

    char dir[1000];
    getcwd(dir, sizeof(dir));
    debug("TEST_DIR: %s TEST_FILENAME: %s TEST_EEPROM_PATH: %s TEST_EEPROM_FILENAME: %s TEST_EEPROM_SIZE: %d\ncur_dir: %s\n",
          TEST_DIR, TEST_FILENAME, TEST_EEPROM_PATH, TEST_EEPROM_FILENAME, TEST_EEPROM_SIZE, dir);

    // Test 1: open, write, close
    test1();

    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 1 - passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    return 0;
}


void test1() {
    EEPROMDescriptor ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);
    assert(("Check eeprom_open result", ep.eeprom_fid > 0));
    assert(("Check eeprom_open result size = 8192", ep.eeprom_size == TEST_EEPROM_SIZE));
    printf("EEPROM opened, size: %lu\n", ep.eeprom_size);

    int EEPROM_consistency = EEPROM_HeaderCheckConsistency(ep);
    printf("Check EEPROM_header: %i\n", EEPROM_consistency);
    EEPROM_FormatEEPROM(ep);
    if (EEPROM_consistency < 0) {
        printf("EEPROM header is not consistent, format EEPROM\n");
        // TODO: check error code
        EEPROM_FormatEEPROM(ep);
    }
    EEPROM_CloseEEPROM(ep);
    ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);

    char filename[100];
    uint8_t filedata[8192];
    uint16_t filesize;
    int i, err;
    for (i=0; i < sizeof(test_files)/sizeof (char *); i++) {
        sprintf(filename, "%s_%d", TEST_FILENAME, i);
        printf("!!!!++++ Add new file %s\n",filename);
        filesize = strlen(test_files[i])+1;
        memcpy(filedata, test_files[i],filesize);

        err = EEPROM_AddFile(ep, filename, filedata, strlen(test_files[i])+1);
        if (err <= 0)
            break;

        printf("File %d: %s size:%i\n", i, filename, filesize);
        memset(filedata, 0, sizeof(filedata));
    }
    assert("Check EEPROM_AddFile failed" && i == 11);
    assert("Check EEPROM_AddFile return error" && err == NOTENOUGHSPACE);
    EEPROM_CloseEEPROM(ep);

}

