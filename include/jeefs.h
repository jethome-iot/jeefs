// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_JEEFS_H
#define JEEFS_JEEFS_H

#include <stdbool.h>
#include <stdint.h>

#include "eepromops.h"
#include "jeefs_generated.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JEEFSFileHeader JEEFSFileHeaderv1

/**
 * Jethub EEPROM partition and file system
 *
 * API:
 * EEPROM_ListFiles() - Returns the number of files found. Populates fileList
 * with the names of the files. EEPROM_ReadFile() - Reads the data of the file
 * with the given filename into the buffer. EEPROM_WriteFile() - Overwrites the
 * data of an existing file with the given filename. EEPROM_AddFile() - Creates
 * a new file with the given filename and data. EEPROM_DeleteFile() - Deletes
 * the file with the given filename. defragEEPROM() - Compacts the EEPROM by
 * removing gaps caused by deleted files or fragmentation.
 * EEPROM_HeaderCheckConsistency() - Checks the integrity of the file system.
 *
 * Base principles:
 * - EEPROM is divided into files (partitions). Each partition has a name,
 * offset and size.
 * - file name limited to FILE_NAME_LENGTH
 * - files is linked by linked list
 * - files can't be zero size
 * - files can't be fragmented
 * - files on overwrite if size differs are deleted and new file is created
 * - auto defragmentation on every EEPROM_DeleteFile()
 */

// File system functions

// Returns the number of files found. Populates fileList with the names of the
// files.
int16_t EEPROM_ListFiles(EEPROMDescriptor eeprom_descriptor,
                         char fileList[][FILE_NAME_LENGTH], uint16_t maxFiles);

// Reads the data of the file with the given filename into the buffer.
// Return: read bytes count, 0 if file not found, <0 if error.
int16_t EEPROM_ReadFile(EEPROMDescriptor eeprom_descriptor,
                        const char *filename, uint8_t *buffer,
                        uint16_t bufferSize);

// Overwrites the data of an existing file with the given filename.
// Return: written bytes count, 0 if file not found, <0 if error.
int16_t EEPROM_WriteFile(EEPROMDescriptor eeprom_descriptor,
                         const char *filename, const uint8_t *data,
                         uint16_t dataSize);

// Creates a new file with the given filename and data.
// Return: written bytes count, 0 if file already exists, <0 if error.
int16_t EEPROM_AddFile(EEPROMDescriptor eeprom_descriptor, const char *filename,
                       const uint8_t *data, uint16_t dataSize);

// Deletes the file with the given filename.
// Return: 1 if file deleted, 0 if file not found, <0 if error.
int16_t EEPROM_DeleteFile(EEPROMDescriptor descriptor, const char *filename);

// Compacts the EEPROM by removing gaps caused by deleted files or
// fragmentation. Return: 1 if EEPROM compacted, 0 if no compaction is needed,
// <0 if error.
int16_t defragEEPROM(EEPROMDescriptor eeprom_descriptor);

// Checks the integrity of the file system.
// Return: 1 if the filesystem is consistent, 0 if the filesystem is
// inconsistent, <0 if error.
int16_t EEPROM_HeaderCheckConsistency(EEPROMDescriptor eeprom_descriptor);

// Set EEPROM_Header
int EEPROM_SetHeader(EEPROMDescriptor eeprom_descriptor, void *header);

int EEPROM_GetHeader(EEPROMDescriptor eeprom_descriptor, void *header,
                     int size);

EEPROMDescriptor EEPROM_OpenEEPROM(const char *pathname, uint16_t eeprom_size);
int EEPROM_CloseEEPROM(EEPROMDescriptor eeprom_descriptor);

/*
 * EEPROM_FormatEEPROM() - Formats the EEPROM with the specified version.
 * Returns 0 on success, -1 on error.
 */
int EEPROM_FormatEEPROM(EEPROMDescriptor ep, int version);

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_H
