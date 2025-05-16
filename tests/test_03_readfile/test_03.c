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
    printf("Test 03! DEBUG:%i\n",DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeader) = %lu\n", sizeof(JEEPROMHeaderv2));
    printf("sizeof(JEEFSFileHeader) = %lu\n", sizeof(JEEFSFileHeader));

    char dir[1000];
    getcwd(dir, sizeof(dir));
    debug("TEST_DIR: %s TEST_FILENAME: %s TEST_EEPROM_PATH: %s TEST_EEPROM_FILENAME: %s TEST_EEPROM_SIZE: %d\ncur_dir: %s\n",
          TEST_DIR, TEST_FILENAME, TEST_EEPROM_PATH, TEST_EEPROM_FILENAME, TEST_EEPROM_SIZE, dir);


    // END: delete test files
    //delete_files(TEST_DIR, TEST_FILENAME, 5);
    return 0;
}
