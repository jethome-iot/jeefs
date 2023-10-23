// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include "eepromops.h"
#include "debug.h"

// EEPROM functions
// Internal functions
static uint16_t eeprom_getsize(EEPROMDescriptor *eeprom_descriptor);

// External functions
/**
 * EEPROMDescriptor eeprom_open(const char *pathname, uint16_t eeprom_size)
 * @param pathname
 * @param eeprom_size
 * @return EEPROMDescriptor
 * @brief Open EEPROM file and return descriptor. If no file exists and eeprom_size is 0 return error.
 * If eeprom_size is 0 - try to get size from file.
 * If eeprom_size non zero - try to create file with size eeprom_size. (Not implemented yet)
 */

EEPROMDescriptor eeprom_open(const char *pathname, uint16_t eeprom_size) {
    EEPROMDescriptor descriptor;
    int _eeprom_flags = O_RDWR;
    if (eeprom_size > 0) {
        _eeprom_flags |= O_CREAT;
    }
    descriptor.eeprom_fid = open(pathname, _eeprom_flags, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP); // Using a hypothetical open function
    if (descriptor.eeprom_fid == -1) {
        // Handle error, maybe set the size to 0 or handle in a way specific to your use-case
        descriptor.eeprom_size = 0;
        return descriptor;
    }

    if (eeprom_size == 0) {
        if (!eeprom_getsize(&descriptor)) {
            debug("eeprom_getsize failed\n");
            descriptor.eeprom_size = 0;
            descriptor.eeprom_fid = -1;
        }
    } else {
        assert("eeprom_open with non-zero eeprom_size not implemented" && false);
        descriptor.eeprom_size = eeprom_size;
    }
    return descriptor;
}

ssize_t eeprom_read(EEPROMDescriptor eeprom_descriptor, void *buf, size_t count, uint16_t offset) {
    if (!lseek (eeprom_descriptor.eeprom_fid, offset, SEEK_SET))
        return -1;
    return read(eeprom_descriptor.eeprom_fid, buf, count);
}

ssize_t eeprom_write(EEPROMDescriptor eeprom_descriptor, const void *buf, size_t count, uint16_t offset) {
    if (!lseek(eeprom_descriptor.eeprom_fid, offset, SEEK_SET))
        return -1;
    if (offset + count > eeprom_descriptor.eeprom_size)
        return -1;
    return write(eeprom_descriptor.eeprom_fid, buf, count);
}

int eeprom_close(EEPROMDescriptor eeprom_descriptor) {
    return close(eeprom_descriptor.eeprom_fid);
}


uint16_t eeprom_getsize(EEPROMDescriptor *eeprom_descriptor) {
    // This is a mockup; you'd need to implement the method to get the actual size from the EEPROM or its specifications
    // For instance, using a method specific to your EEPROM hardware or OS APIs to determine the size.
    struct stat eeprom_fstat;
    if (!fstat(eeprom_descriptor->eeprom_fid, &eeprom_fstat))
        return -1;
    eeprom_descriptor->eeprom_size = eeprom_fstat.st_size;
    return eeprom_descriptor->eeprom_size; // Assuming 8K EEPROM for now
}
