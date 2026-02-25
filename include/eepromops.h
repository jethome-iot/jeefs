// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_EEPROMOPS_H
#define JEEFS_EEPROMOPS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct {
  int eeprom_fid;     // File descriptor for the EEPROM
  size_t eeprom_size; // Size of the EEPROM
  /*uint8_t reserved[2];  // Adjusted for alignment to 4 bytes */
} EEPROMDescriptor;
#pragma pack(pop)

// EEPROM functions
EEPROMDescriptor eeprom_open(const char *pathname, uint16_t eeprom_size);
int eeprom_close(EEPROMDescriptor eeprom_descriptor);

ssize_t eeprom_read(EEPROMDescriptor eeprom_descriptor, void *buf,
                    uint16_t count, uint16_t offset);
uint16_t eeprom_write(EEPROMDescriptor eeprom_descriptor, const void *buf,
                      uint16_t count, uint16_t offset);

#ifdef __cplusplus
}
#endif

#endif // JEEFS_EEPROMOPS_H
