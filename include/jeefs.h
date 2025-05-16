// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_JEEFS_H
#define JEEFS_JEEFS_H

#include <stdbool.h>
#include <stdint.h>

#include "eepromops.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAGIC "JetHome"
#define HEADER_VERSION 1
#define FILE_NAME_LENGTH 15
#define MAC_LENGTH 6
#define MAGIC_LENGTH 8
#define SERIAL_LENGTH 32
#define USID_LENGTH 32
#define CPUID_LENGTH 32
#define BOARDNAME_LENGTH 31
#define BOARDVERSION_LENGTH 31
#define EEPROM_EMPTYBYTE '\x00'

#pragma pack(push, 1)

// EEPROM header structure for version check
typedef struct {
  char magic[MAGIC_LENGTH]; // 8 bytes
  uint8_t version;          // 1 byte
  uint8_t reserved1[3];     // 3 bytes align
} JEEPROMHeaderversion;

// EEPROM header structure v.1
typedef struct {
  char magic[MAGIC_LENGTH];                   // 8 bytes
  uint8_t version;                            // 1 byte
  uint8_t reserved1[3];                       // 3 bytes align
  char boardname[BOARDNAME_LENGTH + 1];       // 32 bytes
  char boardversion[BOARDVERSION_LENGTH + 1]; // 32 bytes
  uint8_t serial[SERIAL_LENGTH];              // 32 bytes
  uint8_t usid[USID_LENGTH];                  // 32 bytes
  uint8_t cpuid[CPUID_LENGTH];                // 32 bytes
  uint8_t mac[MAC_LENGTH];                    // 6 bytes
  uint8_t reserved2[2];                       // 2 bytes for extended MAC
  uint16_t modules[16];                       // 32 bytes
  uint8_t reserved3[296];                     // for future use
  uint32_t crc32;
} JEEPROMHeaderv1; // sizeof(JEEPROMHeader) = 512 bytes

// EEPROM header structure v.2 (256-byte version)
typedef struct {
  char magic[MAGIC_LENGTH];                   // 8 bytes
  uint8_t version;                            // 1 byte
  uint8_t reserved1[3];                       // 3 bytes VERSION align
  char boardname[BOARDNAME_LENGTH + 1];       // 32 bytes
  char boardversion[BOARDVERSION_LENGTH + 1]; // 32 bytes
  uint8_t serial[SERIAL_LENGTH];              // 32 bytes
  uint8_t usid[USID_LENGTH];                  // 32 bytes
  uint8_t cpuid[CPUID_LENGTH];                // 32 bytes
  uint8_t mac[MAC_LENGTH];                    // 6 bytes
  uint8_t reserved2[2];                       // 2 bytes for extended MAC
  uint8_t reserved3[72];                      // 72 bytes for future use
  uint32_t crc32;                             // 4 bytes header CRC
} JEEPROMHeaderv2; // sizeof(JEEPROMHeader) = 256 bytes

union JEEPROMHeaderu {
  JEEPROMHeaderversion version;
  JEEPROMHeaderv1 v1;
  JEEPROMHeaderv2 v2;
};
// File header structure
typedef struct {
  char name[FILE_NAME_LENGTH + 1]; // 16 bytes
  uint16_t dataSize;               // 2 bytes
  uint32_t crc32;                  // 4 bytes
  uint16_t nextFileAddress;        // 2 bytes
} JEEFSFileHeaderv1;               // 24 bytes

#pragma pack(pop)

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
