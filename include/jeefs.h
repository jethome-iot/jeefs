// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 */

#ifndef JEEFS_JEEFS_H
#define JEEFS_JEEFS_H

#include <stdint.h>

#include "jeefs_generated.h"

#ifdef __cplusplus
extern "C" {
#endif

#define JEEFSFileHeader JEEFSFileHeaderv1

/**
 * Jethub EEPROM partition and file system
 *
 * Every operation works directly on a caller-owned image buffer
 * (issue #25, variant A): the environment reads the EEPROM content
 * itself — U-Boot with its built-in access, the kernel, userspace
 * tooling — hands the bytes to the library, and writes the buffer
 * back if an operation mutated it. The library performs no I/O.
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
 * - file name limited to JEEFS_FILE_NAME_LENGTH
 * - files is linked by linked list
 * - files can't be zero size
 * - files can't be fragmented
 * - files on overwrite if size differs are deleted and new file is created
 * - auto defragmentation on every EEPROM_DeleteFile()
 */

// File system functions

// Returns the number of files found (>= 0) or a negative EEPROMError.
// Populates fileList with the NUL-terminated names of the files.
int16_t EEPROM_ListFiles(const uint8_t *image, uint16_t imageSize, char fileList[][JEEFS_FILE_NAME_LENGTH + 1],
                         uint16_t maxFiles);

// Reads the data of the file with the given filename into the buffer and
// verifies the stored CRC32.
// Return: read bytes count, FILENOTFOUND, FILENAMENOTVALID, BUFFERNOTVALID
// (too small), EEPROMCORRUPTED (bad chain or CRC mismatch).
int16_t EEPROM_ReadFile(const uint8_t *image, uint16_t imageSize, const char *filename, uint8_t *buffer,
                        uint16_t bufferSize);

// Overwrites the data of an existing file. Same size overwrites in place;
// a different size re-creates the file (it moves to the end of the chain —
// except JEEFS_DEVICE_ID_FILENAME, which re-inserts first, see AddFile).
// Free space is checked before the old content is destroyed.
// Return: written bytes count, FILENOTFOUND, FILENAMENOTVALID,
// NOTENOUGHSPACE (old file intact), BUFFERNOTVALID, EEPROMCORRUPTED.
int16_t EEPROM_WriteFile(uint8_t *image, uint16_t imageSize, const char *filename, const uint8_t *data,
                         uint16_t dataSize);

// Creates a new file with the given filename and data. New files go to the
// end of the chain; the reserved JEEFS_DEVICE_ID_FILENAME is the exception —
// it is inserted FIRST (the existing chain shifts up), so a boot environment
// can read the device identity as a bounded prefix of the image.
// Return: written bytes count, 0 if the file already exists,
// FILENAMENOTVALID, BUFFERNOTVALID, NOTENOUGHSPACE, EEPROMCORRUPTED.
int16_t EEPROM_AddFile(uint8_t *image, uint16_t imageSize, const char *filename, const uint8_t *data,
                       uint16_t dataSize);

// Deletes the file with the given filename and compacts the chain (the
// following files shift down; their links are rewritten).
// Return: 1 if deleted, FILENOTFOUND, FILENAMENOTVALID, EEPROMCORRUPTED.
int16_t EEPROM_DeleteFile(uint8_t *image, uint16_t imageSize, const char *filename);

// Checks the integrity of the EEPROM header.
// Return: 1 if consistent, 0 if inconsistent (bad magic/version/CRC or
// image shorter than the header).
int16_t EEPROM_HeaderCheckConsistency(const uint8_t *image, uint16_t imageSize);

// Recalculates the CRC32 inside the caller's header buffer and copies the
// header into the image. Return: 0 on success, BUFFERNOTVALID,
// EEPROMCORRUPTED (bad magic/version or image too small).
int EEPROM_SetHeader(uint8_t *image, uint16_t imageSize, void *header);

// Copies exactly the detected header (256/512 bytes) into the buffer.
// Return: 0 on success, BUFFERNOTVALID (buffer smaller than the header),
// EEPROMCORRUPTED.
int EEPROM_GetHeader(const uint8_t *image, uint16_t imageSize, void *header, int size);

/*
 * EEPROM_FormatEEPROM() - Formats the image with the specified header
 * version and clears the file area.
 * Returns 0 on success, negative EEPROMError on failure.
 */
int EEPROM_FormatEEPROM(uint8_t *image, uint16_t imageSize, int version);

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_H
