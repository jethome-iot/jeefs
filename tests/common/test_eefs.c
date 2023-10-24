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



int main() {
    printf("Hello, World! DEBUG:%i\n",DEBUG);
    // print sizes of structures from jeefs.h
    printf("sizeof(EEPROMHeader) = %lu\n", sizeof(EEPROMHeader));
    printf("sizeof(JEEFSFileHeader) = %lu\n", sizeof(JEEFSFileHeader));

    // PREPARE: generate test files
    delete_files(TEST_DIR, TEST_FILENAME, 5);
    generate_files(TEST_DIR, TEST_FILENAME, 5, 0);


    EEPROMDescriptor ep=eeprom_open(TEST_EEPROM_PATH, 0);
    assert(("Check eeprom_open result", ep.eeprom_fid > 0));

    char filename[100];
    char filedata[8192];
    struct stat filestat;
    for (int i=0; i < 5; i++) {
        sprintf(filename, "%s/%s_%d", TEST_DIR, TEST_FILENAME, i);
        int fd=open(filename, O_RDONLY);
        fstat(fd, &filestat);
        assert (read(fd, filedata, filestat.st_size)>0);
        assert(addFile(ep, basename(filename), filedata, filestat.st_size) > 0);
        printf("File %d: %s size:%li\n", i, filename, filestat.st_size);
        close(fd);
        memset(filedata, 0, sizeof(filedata));
        memset(&filestat, 0, sizeof(filestat));
    }
    sleep(2);

    // END: delete test files
    delete_files(TEST_DIR, TEST_FILENAME, 5);
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
    if (!addFile(eeprom_descriptor, TEST_FILENAME, testData, strlen((char*)testData))) {
        printf("Error adding file to EEPROM.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }

    // Read the file and verify content
    uint8_t buffer[50];
    uint16_t actualSize;
    actualSize = readFile(eeprom_descriptor, TEST_FILENAME, buffer, sizeof(buffer));
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
    uint16_t fileCount = listFiles(eeprom_descriptor, fileList, 10);
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
    if (!deleteFile(eeprom_descriptor, TEST_FILENAME)) {
        printf("Error deleting file.\n");
        eeprom_close(eeprom_descriptor);
        return -1;
    }
    fileCount = listFiles(eeprom_descriptor, fileList, 10);
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