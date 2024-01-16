// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_JEEFS_H
#define JEEFS_JEEFS_H

#include <stdint.h>
#include <stdbool.h>

#include "eepromops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAGIC               "JetHome"
#define HEADER_VERSION      1
#define FILE_NAME_LENGTH    15
#define MAGIC_LENGTH        8
#define SERIAL_LENGTH       16
#define USID_LENGTH         32
#define CPUID_LENGTH        32
#define BOARDNAME_LENGTH    31
#define BOARDVERSION_LENGTH 31
#define EEPROM_EMPTYBYTE    '\x00'

#define MAC_LENGTH         6

#pragma pack(push, 1)

// EEPROM header structure
typedef struct {
    char     magic[MAGIC_LENGTH];
    uint8_t  version;
    char     boardname[BOARDNAME_LENGTH+1];
    char     boardversion[BOARDVERSION_LENGTH+1];
    uint16_t modules[16];
    uint8_t  serial[SERIAL_LENGTH];
    uint8_t  mac[MAC_LENGTH];
    uint8_t  usid[USID_LENGTH];
    uint8_t  cpuid[CPUID_LENGTH];
    uint8_t  reserved[1];  // Adjusted for alignment to 4 bytes
    uint8_t  reserved2[256]; // for future use
    uint32_t crc32;
} JEEPROMHeader; // sizeof(JEEPROMHeader) = 512 bytes

// File header structure
typedef struct {
    char     name[FILE_NAME_LENGTH+1];
    uint16_t dataSize;
    uint32_t crc32;
    uint16_t nextFileAddress;
} JEEFSFileHeader; // 24 bytes

#pragma pack(pop)

/**
 * Jethub EEPROM partition and file system
 *
 * API:
 * EEPROM_ListFiles() - Returns the number of files found. Populates fileList with the names of the files.
 * EEPROM_ReadFile() - Reads the data of the file with the given filename into the buffer.
 * EEPROM_WriteFile() - Overwrites the data of an existing file with the given filename.
 * EEPROM_AddFile() - Creates a new file with the given filename and data.
 * EEPROM_DeleteFile() - Deletes the file with the given filename.
 * defragEEPROM() - Compacts the EEPROM by removing gaps caused by deleted files or fragmentation.
 * EEPROM_HeaderCheckConsistency() - Checks the integrity of the file system.
 *
 * Base principles:
 * - EEPROM is divided into files (partitions). Each partition has a name, offset and size.
 * - file name limited to FILE_NAME_LENGTH
 * - files is linked by linked list
 * - files can't be zero size
 * - files can't be fragmented
 * - files on overwrite if size differs are deleted and new file is created
 * - auto defragmentation on every EEPROM_DeleteFile()
 */

// File system functions

// Returns the number of files found. Populates fileList with the names of the files.
int16_t EEPROM_ListFiles(EEPROMDescriptor eeprom_descriptor, char fileList[][FILE_NAME_LENGTH], uint16_t maxFiles);

// Reads the data of the file with the given filename into the buffer.
// Return: read bytes count, 0 if file not found, <0 if error.
int16_t EEPROM_ReadFile(EEPROMDescriptor eeprom_descriptor, const char *filename, uint8_t *buffer, uint16_t bufferSize);

// Overwrites the data of an existing file with the given filename.
// Return: written bytes count, 0 if file not found, <0 if error.
int16_t EEPROM_WriteFile(EEPROMDescriptor eeprom_descriptor, const char *filename, const uint8_t *data, uint16_t dataSize);

// Creates a new file with the given filename and data.
// Return: written bytes count, 0 if file already exists, <0 if error.
int16_t EEPROM_AddFile(EEPROMDescriptor eeprom_descriptor, const char *filename, const uint8_t *data, uint16_t dataSize);

// Deletes the file with the given filename.
// Return: 1 if file deleted, 0 if file not found, <0 if error.
int16_t EEPROM_DeleteFile(EEPROMDescriptor descriptor, const char *filename);

// Compacts the EEPROM by removing gaps caused by deleted files or fragmentation.
// Return: 1 if EEPROM compacted, 0 if no compaction needed, <0 if error.
int16_t defragEEPROM(EEPROMDescriptor eeprom_descriptor);

// Checks the integrity of the file system.
// Return: 1 if file system is consistent, 0 if file system is inconsistent, <0 if error.
int16_t EEPROM_HeaderCheckConsistency(EEPROMDescriptor eeprom_descriptor);

// Set EEPROM_Header
int EEPROM_SetHeader(EEPROMDescriptor eeprom_descriptor, JEEPROMHeader header);

JEEPROMHeader EEPROM_GetHeader(EEPROMDescriptor eeprom_descriptor);

EEPROMDescriptor EEPROM_OpenEEPROM(const char *pathname, uint16_t eeprom_size);
int EEPROM_CloseEEPROM(EEPROMDescriptor eeprom_descriptor);

int EEPROM_FormatEEPROM(EEPROMDescriptor ep);

#ifdef __cplusplus
}
#endif

#endif //JEEFS_JEEFS_H
