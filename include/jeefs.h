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
 * with the NUL-terminated names of the files. EEPROM_ReadFile() - Reads the
 * data of the file with the given filename into the buffer and verifies its
 * CRC32. EEPROM_WriteFile() - Overwrites the data of an existing file.
 * EEPROM_AddFile() - Creates a new file with the given filename and data.
 * EEPROM_DeleteFile() - Deletes the file and compacts the chain.
 * EEPROM_HeaderCheckConsistency() - Checks the integrity of the header.
 *
 * Errors are negative EEPROMError values (eepromerr.h). Payloads are
 * limited to INT16_MAX bytes: the int16_t return type carries byte counts.
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

// Returns the number of files found (>= 0) or a negative EEPROMError.
// Populates fileList with the NUL-terminated names of the files.
int16_t EEPROM_ListFiles(EEPROMDescriptor eeprom_descriptor,
                         char fileList[][FILE_NAME_LENGTH + 1],
                         uint16_t maxFiles);

// Reads the data of the file with the given filename into the buffer and
// verifies the stored CRC32.
// Return: read bytes count, FILENOTFOUND, FILENAMENOTVALID, BUFFERNOTVALID
// (too small), EEPROMCORRUPTED (bad chain or CRC mismatch), EEPROMREADERROR.
int16_t EEPROM_ReadFile(EEPROMDescriptor eeprom_descriptor,
                        const char *filename, uint8_t *buffer,
                        uint16_t bufferSize);

// Overwrites the data of an existing file. Same size overwrites in place;
// a different size re-creates the file (it moves to the end of the chain).
// Free space is checked before the old content is destroyed.
// Return: written bytes count, FILENOTFOUND, FILENAMENOTVALID,
// NOTENOUGHSPACE (old file intact), BUFFERNOTVALID, EEPROMCORRUPTED,
// EEPROMREADERROR, EEPROMWRITEERROR.
int16_t EEPROM_WriteFile(EEPROMDescriptor eeprom_descriptor,
                         const char *filename, const uint8_t *data,
                         uint16_t dataSize);

// Creates a new file with the given filename and data.
// Return: written bytes count, 0 if the file already exists,
// FILENAMENOTVALID, BUFFERNOTVALID, NOTENOUGHSPACE, EEPROMCORRUPTED,
// EEPROMREADERROR, EEPROMWRITEERROR.
int16_t EEPROM_AddFile(EEPROMDescriptor eeprom_descriptor, const char *filename,
                       const uint8_t *data, uint16_t dataSize);

// Deletes the file with the given filename and compacts the chain (the
// following files shift down; their links are rewritten).
// Return: 1 if deleted, FILENOTFOUND, FILENAMENOTVALID, EEPROMCORRUPTED,
// EEPROMREADERROR, EEPROMWRITEERROR.
int16_t EEPROM_DeleteFile(EEPROMDescriptor descriptor, const char *filename);

// Checks the integrity of the EEPROM header.
// Return: 1 if consistent, 0 if inconsistent (bad magic/version/CRC),
// EEPROMREADERROR.
int16_t EEPROM_HeaderCheckConsistency(EEPROMDescriptor eeprom_descriptor);

// Recalculates the CRC32 inside the caller's header image and writes it to
// the EEPROM. Return: 0 on success, BUFFERNOTVALID, EEPROMCORRUPTED (bad
// magic/version), EEPROMWRITEERROR.
int EEPROM_SetHeader(EEPROMDescriptor eeprom_descriptor, void *header);

// Reads exactly the detected header (256/512 bytes) into the buffer.
// Return: 0 on success, BUFFERNOTVALID (buffer smaller than the header),
// EEPROMCORRUPTED, EEPROMREADERROR.
int EEPROM_GetHeader(EEPROMDescriptor eeprom_descriptor, void *header,
                     int size);

EEPROMDescriptor EEPROM_OpenEEPROM(const char *pathname, uint16_t eeprom_size);
int EEPROM_CloseEEPROM(EEPROMDescriptor eeprom_descriptor);

/*
 * EEPROM_FormatEEPROM() - Formats the EEPROM with the specified version.
 * Returns 0 on success, negative EEPROMError on failure.
 */
int EEPROM_FormatEEPROM(EEPROMDescriptor ep, int version);

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_H
