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

void test1();

void test2();

int main() {
    printf("Hello, World! DEBUG:%i\n",DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeader) = %lu\n", sizeof(JEEPROMHeaderv2));
    printf("sizeof(JEEFSFileHeader) = %lu\n", sizeof(JEEFSFileHeader));

    test2();
    return 0;
}

void test2 (){
    EEPROMDescriptor ep = EEPROM_OpenEEPROM(TEST_FULL_EEPROM_FILENAME, 0);
    assert(("Check eeprom_open result", ep.eeprom_fid > 0));
    assert(("Check eeprom_open result size = 8192", ep.eeprom_size == TEST_EEPROM_SIZE));
    printf("EEPROM opened, size: %lu\n", ep.eeprom_size);

    int EEPROM_consistency = EEPROM_HeaderCheckConsistency(ep);
    printf("Check EEPROM_header: %i\n", EEPROM_consistency);
    assert("Check EEPROM_header consistency" && EEPROM_consistency == 0);

    char filename[100];
    uint8_t filedata[8192];
    uint16_t filesize;
    int i, err;
    printf("sizeof(test_files) = %lu\n", sizeof(test_files)/sizeof (char *));
    for (i=0; i < sizeof(test_files)/sizeof (char *); i++) {
        sprintf(filename, "%s_%d", TEST_FILENAME, i);
        printf("!!!!++++ read file %s\n",filename);
        fflush(stdout);

        err = EEPROM_ReadFile(ep, filename, filedata, 1);
        if (i < 10)
        {
            assert("Check EEPROM_ReadFile buffer size" && err == BUFFERNOTVALID);
        } else
            assert("Check file not exists" && err == FILENOTFOUND);

        memset(filedata, 0, sizeof(filedata));
        err = EEPROM_ReadFile(ep, filename, filedata, sizeof(filedata));

        if (i < 10)
        {
            assert ("Check file exists" && err > 0);
            assert ("Check filesize" && strlen(test_files[i])+1 == err);

            filesize = strlen(test_files[i])+1;
            debug("readed: %i filesize = %i cmp = %i\n", err, filesize, memcmp(filedata, test_files[i],filesize));
            //fflush(stdout);
            assert("Compare file with original" && memcmp(filedata, test_files[i],filesize) == 0);
        } else
            assert("Check file not exists" && err == FILENOTFOUND);
        printf("File %d: %s size:%i on eeprom:%i checked ok\n", i, filename, filesize, err);
    }
    EEPROM_CloseEEPROM(ep);

}
