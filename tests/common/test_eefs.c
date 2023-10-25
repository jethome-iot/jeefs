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


int main() {
    printf("Hello, World! DEBUG:%i\n",DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(JEEPROMHeader) = %lu\n", sizeof(JEEPROMHeader));
    printf("sizeof(JEEFSFileHeader) = %lu\n", sizeof(JEEFSFileHeader));

    char dir[1000];
    getcwd(dir, sizeof(dir));

    debug("TEST_DIR: %s TEST_FILENAME: %s TEST_EEPROM_PATH: %s TEST_EEPROM_FILENAME: %s TEST_EEPROM_SIZE: %d\ncur_dir: %s\n",
          TEST_DIR, TEST_FILENAME, TEST_EEPROM_PATH, TEST_EEPROM_FILENAME, TEST_EEPROM_SIZE, dir);

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

    char filename[100];
    uint8_t filedata[8192];
    uint16_t filesize;
    int i, err;
    for (i=0; i < sizeof(test_files); i++) {
        sprintf(filename, "%s_%d", TEST_FILENAME, i);
        printf("!!!!++++ Add new file %s\n",filename);
        filesize = strlen(test_files[i])+1;
        memcpy(filedata, test_files[i],filesize);

        err = EEPROM_AddFile(ep, filename, filedata, strlen(test_files[i])+1);
        if (err <= 0)
            break;

        printf("File %d: %s size:%li\n", i, filename, filesize);
        memset(filedata, 0, sizeof(filedata));
    }
    assert("Check EEPROM_AddFile failed" && i == 11);
    assert("Check EEPROM_AddFile return error" && err == NOTENOUGHSPACE);
    sleep(2);

    // END: delete test files
    //delete_files(TEST_DIR, TEST_FILENAME, 5);
    EEPROM_CloseEEPROM(ep);
    return 0;
}



// delete generated files
// args: path to dir, basename, number of files
// return: 0 if success, -1 if error


int test_lib() {
    EEPROMDescriptor eeprom_descriptor;

    // Open EEPROM
    eeprom_descriptor = eeprom_open(TEST_EEPROM_PATH, 1);
    if (eeprom_descriptor.eeprom_fid < 0) {
        perror("Error opening EEPROM");
        return -1;
    }

    // Add a file to EEPROM
    const uint8_t testData[] = "Hello, EEPROM!";
    if (!EEPROM_AddFile(eeprom_descriptor, TEST_FILENAME, testData, strlen((char *) testData))) {
        printf("Error adding file to EEPROM.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }

    // Read the file and verify content
    uint8_t buffer[50];
    uint16_t actualSize;
    actualSize = EEPROM_ReadFile(eeprom_descriptor, TEST_FILENAME, buffer, sizeof(buffer));
    if (!actualSize) {
        printf("Error reading file from EEPROM.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }
    buffer[actualSize] = '\0';  // Null terminate for easy string comparison
    if (strcmp((char*)buffer, (char*)testData) != 0) {
        printf("File content mismatch.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }

    // List files
    char fileList[10][FILE_NAME_LENGTH];
    uint16_t fileCount = EEPROM_ListFiles(eeprom_descriptor, fileList, 10);
    bool fileFound = false;
    for (int i = 0; i < fileCount; i++) {
        if (strcmp(fileList[i], TEST_FILENAME) == 0) {
            fileFound = true;
            break;
        }
    }
    if (!fileFound) {
        printf("Test file not listed.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }

    // Delete file and ensure it's no longer listed
    if (!EEPROM_DeleteFile(eeprom_descriptor, TEST_FILENAME)) {
        printf("Error deleting file.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }
    fileCount = EEPROM_ListFiles(eeprom_descriptor, fileList, 10);
    fileFound = false;
    for (int i = 0; i < fileCount; i++) {
        if (strcmp(fileList[i], TEST_FILENAME) == 0) {
            fileFound = true;
            break;
        }
    }
    if (fileFound) {
        printf("Test file still listed after deletion.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }

    // Close EEPROM
    eeprom_close(eeprom_descriptor);

    printf("All tests passed.\n");
    return 0;
}