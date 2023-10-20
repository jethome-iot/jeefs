// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_EEPROMOPS_H
#define JEEFS_EEPROMOPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#pragma pack(push, 1)
typedef struct {
    int eeprom_fid;      // File descriptor for the EEPROM
    uint16_t eeprom_size; // Size of the EEPROM
    uint8_t reserved[2];  // Adjusted for alignment to 4 bytes
} EEPROMDescriptor; // sizeof(EEPROMDescriptor) = 8 bytes
#pragma pack(pop)

// EEPROM functions
ssize_t eeprom_read(EEPROMDescriptor eeprom_descriptor, void *buf, size_t count, uint16_t offset);
ssize_t eeprom_write(EEPROMDescriptor eeprom_descriptor, const void *buf, size_t count, uint16_t offset);
EEPROMDescriptor eeprom_open(const char *pathname, uint16_t eeprom_size);

int eeprom_close(EEPROMDescriptor eeprom_descriptor);



#endif //JEEFS_EEPROMOPS_H
