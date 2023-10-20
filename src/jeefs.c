// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <string.h>
#include <zlib.h>
#include <assert.h>

#include "jeefs.h"
#include "err.h"
#include "debug.h"


// Internal functions

// use libz implementation of crc32
static uint32_t calculateCRC32(const uint8_t *data, size_t length);
static int16_t findFile(EEPROMDescriptor eeprom_descriptor, const char *filename, JEEFSFileHeader *header, uint16_t *address);
static int16_t getNextFileAddress(EEPROMDescriptor eeprom_descriptor, uint16_t currentAddress);

/*
 * JEEFS functions
 */

int16_t listFiles(EEPROMDescriptor eeprom_descriptor, char fileList[][FILE_NAME_LENGTH], uint16_t maxFiles) {
    uint16_t count = 0;
    uint16_t currentAddress = sizeof(EEPROMHeader); // Assuming the EEPROM starts with the header

    while (count < maxFiles) {
        JEEFSFileHeader fileHeader;
        if (eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeader), currentAddress) != sizeof(JEEFSFileHeader)) {
            break; // End of list or error
        }
        strncpy(fileList[count], fileHeader.name, FILE_NAME_LENGTH);
        count++;
        currentAddress = getNextFileAddress(eeprom_descriptor, currentAddress);
        if (currentAddress == 0) {
            break; // End of file list
        }
    }

    return count;
}

int16_t readFile(EEPROMDescriptor eeprom_descriptor, const char *filename, uint8_t *buffer, uint16_t bufferSize) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    if (!buffer || bufferSize == 0)
        return BUFFERNOTVALID;

    JEEFSFileHeader fileHeader;
    uint16_t fileAddress;
    if (findFile(eeprom_descriptor, filename, &fileHeader, &fileAddress) != 1) {
        return FILENOTFOUND; // File not found
    }
    if (fileHeader.dataSize > bufferSize) {
        return BUFFERNOTVALID; // Provided buffer is too small
    }
    ssize_t readSize = eeprom_read(eeprom_descriptor, buffer, bufferSize, fileAddress + sizeof(JEEFSFileHeader));
    return readSize > 0 ? readSize : 0; // Return read bytes or 0 on error
}


int16_t writeFile(EEPROMDescriptor eeprom_descriptor, const char *filename, const uint8_t *data, uint16_t dataSize) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    if (!data || dataSize == 0)
        return BUFFERNOTVALID;

    JEEFSFileHeader fileHeader;
    uint16_t fileAddress;
    if (findFile(eeprom_descriptor, filename, &fileHeader, &fileAddress) != 1)
        // File not found
        return FILENOTFOUND;

    // File found
    if (fileHeader.dataSize != dataSize) {
        // Different size, delete, defrag, and create new file
        deleteFile(eeprom_descriptor, filename);
        // not needed, already in deleteFile
        // defragEEPROM(eeprom_descriptor);
        return addFile(eeprom_descriptor, filename, data, dataSize);
    }

    // Overwrite the file content
    if (eeprom_write(eeprom_descriptor, data, dataSize, fileAddress + sizeof(JEEFSFileHeader)) != dataSize) {
        return 0;  // Write error
    }

    // Update the CRC
    fileHeader.crc32 = calculateCRC32(data, dataSize);
    eeprom_write(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeader), fileAddress);

    return dataSize;
}

inline bool isEmpty(char var) {
    return var == '\xFF' || var == '\0';
}

int16_t addFile(EEPROMDescriptor eeprom_descriptor, const char *filename, const uint8_t *data, uint16_t dataSize) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH) {
        debug("addFile: %s %s %u\n", "FILENAMENOTVALID", filename, dataSize);
        return FILENAMENOTVALID;
    }

    if (!data || dataSize == 0) {
        debug("addFile: %s\n", "BUFFERNOTVALID");
        return BUFFERNOTVALID;
    }

    JEEFSFileHeader newFileHeader;
    JEEFSFileHeader currentFileHeader;
    uint16_t existingFileAddress;
    if (findFile(eeprom_descriptor, filename, &currentFileHeader, &existingFileAddress) == 1) {
        debug("addFile: file already exists: %s\n", filename);
        return 0; // File already exists
    }

    debug("addFile: file %s not found. add new\n", filename);

    uint16_t currentAddress = sizeof(EEPROMHeader); // Starting after the EEPROM header

    while (currentAddress < eeprom_descriptor.eeprom_size - sizeof(JEEFSFileHeader)) {
        ssize_t readSize = eeprom_read(eeprom_descriptor, &currentFileHeader, sizeof(JEEFSFileHeader), currentAddress);

        // If the current slot is empty (first time adding a file or a deleted slot) or there's an error in reading
        if (readSize != sizeof(JEEFSFileHeader)) {
            debug("addFile: read eeprom error %s %li != %li\n", filename, readSize, sizeof(JEEFSFileHeader));
            return -1; // Read error
        }

        if (currentFileHeader.nextFileAddress == 0 || currentFileHeader.nextFileAddress > eeprom_descriptor.eeprom_size ||  isEmpty(currentFileHeader.name[0])) {
            break;
        }

        // Move to the next file
        assert(currentFileHeader.nextFileAddress == currentFileHeader.dataSize + sizeof(JEEFSFileHeader));
        currentAddress = currentFileHeader.nextFileAddress; // Move to next file
        // currentAddress += sizeof(JEEFSFileHeader) + currentFileHeader.dataSize;
    }

    // Check if there's enough space to write the new file
    if (currentAddress + sizeof(currentFileHeader) + dataSize >= eeprom_descriptor.eeprom_size) {
        return NOSPACEERROR;  // Not enough space
    }

    // Prepare and write the new file header
    memset(&newFileHeader, 0, sizeof(JEEFSFileHeader));
    strncpy(newFileHeader.name, filename, FILE_NAME_LENGTH);
    newFileHeader.dataSize = dataSize;
    newFileHeader.crc32 = calculateCRC32(data, dataSize);
    newFileHeader.nextFileAddress = 0; // Currently, it's the last file


    // Write the new file header
    if (eeprom_write(eeprom_descriptor, &newFileHeader, sizeof(JEEFSFileHeader), currentAddress) != sizeof(JEEFSFileHeader)) {
        return -1; // Write error
    }

    // Write the file data
    if (eeprom_write(eeprom_descriptor, data, dataSize, currentAddress + sizeof(JEEFSFileHeader)) != dataSize) {
        return -1; // Write error
    }

    return dataSize; // Return number of data bytes written
}


int16_t deleteFile(EEPROMDescriptor descriptor, const char *filename) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    JEEFSFileHeader header;
    uint16_t address;

    int found = findFile(descriptor, filename, &header, &address);
    if (found <= 0) {
        return FILENOTFOUND;  // File not found or error
    }

    // The address after the file we're deleting
    uint16_t nextAddress = address + sizeof(JEEFSFileHeader) + header.dataSize;

    // Move all subsequent files up to fill the space of the deleted file
    uint16_t shiftSize = sizeof(JEEFSFileHeader) + header.dataSize;
    uint8_t buffer[shiftSize];  // Using a buffer equal to the deleted file size for simplicity
    uint16_t readAddress = nextAddress;
    ssize_t bytesRead;

    while (readAddress < descriptor.eeprom_size) {
        bytesRead = eeprom_read(descriptor, buffer, shiftSize, readAddress);
        if (bytesRead <= 0) {
            // We've hit the end of our valid data or encountered an error
            break;
        }

        eeprom_write(descriptor, buffer, bytesRead, readAddress - shiftSize);

        readAddress += bytesRead;
    }

    // Clear out the remaining space
    uint8_t clearByte = 0xFF;
    for (uint16_t i = 0; i < shiftSize && (readAddress - i) < descriptor.eeprom_size; i++) {
        eeprom_write(descriptor, &clearByte, 1, readAddress - i - 1);
    }

    return 1;  // Successfully deleted
}

/*
int16_t deleteFile(EEPROMDescriptor eeprom_descriptor, const char *filename) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    JEEFSFileHeader fileHeaderToDelete;
    uint16_t fileAddressToDelete;
    if (findFile(eeprom_descriptor, filename, &fileHeaderToDelete, &fileAddressToDelete) != 1) {
        return 0; // File not found
    }

    uint16_t prevAddress = sizeof(EEPROMHeader);
    JEEFSFileHeader prevFileHeader;
    while (1) {
        JEEFSFileHeader fileHeader;
        ssize_t readSize = eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeader), prevAddress);
        if (readSize != sizeof(JEEFSFileHeader)) {
            return FILENOTFOUND; // Reached the end or an error occurred
        }

        if (fileHeader.nextFileAddress == fileAddressToDelete) {
            // Found the previous file that points to the file to delete
            prevFileHeader = fileHeader;
            break;
        }
        prevAddress = fileHeader.nextFileAddress; // Move to next file
    }

    // Update the previous file's nextFileAddress to skip the deleted file
    prevFileHeader.nextFileAddress = fileHeaderToDelete.nextFileAddress;
    eeprom_write(eeprom_descriptor, &prevFileHeader, sizeof(JEEFSFileHeader), prevAddress);

    // Zero out the deleted file's header for safety
    memset(&fileHeaderToDelete, 0, sizeof(JEEFSFileHeader));
    eeprom_write(eeprom_descriptor, &fileHeaderToDelete, sizeof(JEEFSFileHeader), fileAddressToDelete);

    // Defragment EEPROM
    defragEEPROM(eeprom_descriptor);

    return 1; // File deleted successfully
}
*/

static int16_t findFile(EEPROMDescriptor eeprom_descriptor, const char *filename, JEEFSFileHeader *header, uint16_t *address) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    uint16_t currentAddress = sizeof(EEPROMHeader); // Starting after the EEPROM header

    while (1) {
        JEEFSFileHeader fileHeader;
        ssize_t readSize = eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeader), currentAddress);
        if (readSize != sizeof(JEEFSFileHeader)) {
            return -1; // File not found or read error
        }

        if (strncmp(fileHeader.name, filename, FILE_NAME_LENGTH) == 0) {
            // Found the file
            if (header) {
                memcpy(header, &fileHeader, sizeof(JEEFSFileHeader));
            }
            if (address) {
                *address = currentAddress;
            }
            return 1; // File found
        }

        currentAddress = getNextFileAddress(eeprom_descriptor, currentAddress);
        if (currentAddress == 0) {
            break; // End of file list
        }
    }

    return 0; // File not found
}

uint32_t calculateCRC32(const uint8_t *data, size_t length) {
    return crc32(0L, data, length);
}

int16_t getNextFileAddress(EEPROMDescriptor eeprom_descriptor, uint16_t currentAddress) {
    JEEFSFileHeader fileHeader;
    if (eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeader), currentAddress) != sizeof(JEEFSFileHeader)) {
        return 0; // Error reading header
    }
    return fileHeader.nextFileAddress;
}
