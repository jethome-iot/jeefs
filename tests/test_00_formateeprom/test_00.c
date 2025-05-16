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

void test0(void);

int main() {
    printf("Hello, World! DEBUG:%i\n",DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeaderv1) = %lu\n", sizeof(JEEPROMHeaderv1));
    printf("sizeof(JEEFSFileHeaderv1) = %lu\n", sizeof(JEEFSFileHeaderv1));

    char dir[1000];
    getcwd(dir, sizeof(dir));
    debug("TEST_DIR: %s TEST_FILENAME: %s TEST_EEPROM_PATH: %s TEST_EEPROM_FILENAME: %s TEST_EEPROM_SIZE: %d\ncur_dir: %s\n",
          TEST_DIR, TEST_FILENAME, TEST_EEPROM_PATH, TEST_EEPROM_FILENAME, TEST_EEPROM_SIZE, dir);


    // Prepare test eeprom file

    int teeprom = open(TEST_FULL_EEPROM_FILENAME, O_CREAT | O_RDWR, 0666);
    ftruncate(teeprom, 0);
    ftruncate(teeprom, TEST_EEPROM_SIZE);
    close(teeprom);

    test0();

    printf("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n Test 0 - passed\n+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
    return 0;
}


void test0(void) {
    EEPROMDescriptor ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);
    assert(("Check eeprom_open result", ep.eeprom_fid > 0));
    assert(("Check eeprom_open result size = 8192", ep.eeprom_size == TEST_EEPROM_SIZE));
    printf("EEPROM opened, size: %lu\n", ep.eeprom_size);




    int EEPROM_consistency = EEPROM_HeaderCheckConsistency(ep);
    printf("Check EEPROM_header: %i\n", EEPROM_consistency);

    assert("Check EEPROM_header non consistency on empty file" && EEPROM_consistency != 0);
    EEPROM_FormatEEPROM(ep,2);
    EEPROM_CloseEEPROM(ep);

    ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);
    EEPROM_consistency = EEPROM_HeaderCheckConsistency(ep);
    printf("Check EEPROM_header: %i\n", EEPROM_consistency);
    assert("\nCheck EEPROM_header consistency failed" && EEPROM_consistency == 0);

    int8_t buf[ep.eeprom_size],buf2[ep.eeprom_size];
    memset(buf, EEPROM_EMPTYBYTE, ep.eeprom_size);
    lseek(ep.eeprom_fid, 0, SEEK_SET);
    assert(read(ep.eeprom_fid, buf2, ep.eeprom_size)==ep.eeprom_size);
    printf("Check EEPROM data consistency\n");
    for(int i=sizeof(JEEPROMHeaderv1); i < ep.eeprom_size; i++) {
        assert("\nCheck EEPROM data consistency failed\n" && buf[i] == buf2[i]);
    }

    EEPROM_CloseEEPROM(ep);
}

