// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <string.h>
#include <zlib.h>
#include <assert.h>

#include "jeefs.h"
#include "eepromerr.h"
#include "debug.h"


// Internal functions

// use libz implementation of crc32
static uint32_t calculateCRC32(const uint8_t *data, size_t length);
static int16_t EEPROM_FindFile(EEPROMDescriptor eeprom_descriptor, const char *filename, JEEFSFileHeader *header, uint16_t *address);
static uint16_t EEPROM_getNextFileAddress(EEPROMDescriptor eeprom_descriptor, uint16_t currentAddress);
static inline bool EEPROM_ByteIsEmpty(char var);
static inline bool EEPROM_WordIsEmpty(uint16_t var);
static inline bool EEPROM_QWordIsEmpty(uint32_t var);


/*
 * JEEFS functions
 */

EEPROMDescriptor EEPROM_OpenEEPROM(const char *pathname, uint16_t eeprom_size) {
    EEPROMDescriptor desc;
    desc = eeprom_open(pathname, eeprom_size);
    if (desc.eeprom_fid == -1) return desc;  // handle error

    return desc;
}

JEEPROMHeader EEPROM_GetHeader(EEPROMDescriptor eeprom_descriptor) {
    JEEPROMHeader header;
    eeprom_read(eeprom_descriptor, &header, sizeof(JEEPROMHeader), 0);
    return header;
}


int EEPROM_CloseEEPROM(EEPROMDescriptor eeprom_descriptor) {
    return eeprom_close(eeprom_descriptor);
}

int16_t EEPROM_ListFiles(EEPROMDescriptor eeprom_descriptor, char fileList[][FILE_NAME_LENGTH], uint16_t maxFiles) {
    int16_t count = 0;
    uint16_t currentAddress = sizeof(JEEPROMHeader); // Assuming the EEPROM starts with the header

    while (count < maxFiles) {
        JEEFSFileHeader fileHeader;
        if (eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeader), currentAddress) != sizeof(JEEFSFileHeader)) {
            break; // End of list or error
        }
        strncpy(fileList[count], fileHeader.name, FILE_NAME_LENGTH);
        count++;
        currentAddress = EEPROM_getNextFileAddress(eeprom_descriptor, currentAddress);
        if (currentAddress == 0) {
            break; // End of file list
        }
    }

    return count;
}

int16_t EEPROM_ReadFile(EEPROMDescriptor eeprom_descriptor, const char *filename, uint8_t *buffer, uint16_t bufferSize) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    if (!buffer || bufferSize == 0)
        return BUFFERNOTVALID;

    JEEFSFileHeader fileHeader;
    uint16_t fileAddress;
    if (EEPROM_FindFile(eeprom_descriptor, filename, &fileHeader, &fileAddress) != 1) {
        return FILENOTFOUND; // File not found
    }
    if (fileHeader.dataSize > bufferSize) {
        return BUFFERNOTVALID; // Provided buffer is too small
    }

    ssize_t readSize = eeprom_read(eeprom_descriptor, buffer, fileHeader.dataSize, fileAddress + sizeof(JEEFSFileHeader));
    return readSize > 0 ? readSize : -1; // Return read bytes or 0 on error
}



int16_t EEPROM_WriteFile(EEPROMDescriptor eeprom_descriptor, const char *filename, const uint8_t *data, uint16_t dataSize) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    if (!data || dataSize == 0)
        return BUFFERNOTVALID;

    JEEFSFileHeader fileHeader;
    uint16_t fileAddress;
    if (EEPROM_FindFile(eeprom_descriptor, filename, &fileHeader, &fileAddress) != 1)
        // File not found
        return FILENOTFOUND;

    // File found
    if (fileHeader.dataSize != dataSize) {
        // Different size, delete, defrag, and create new file
        EEPROM_DeleteFile(eeprom_descriptor, filename);
        // not needed, already in EEPROM_DeleteFile
        // defragEEPROM(eeprom_descriptor);
        return EEPROM_AddFile(eeprom_descriptor, filename, data, dataSize);
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




int16_t EEPROM_AddFile(EEPROMDescriptor eeprom_descriptor, const char *filename, const uint8_t *data, uint16_t dataSize) {
    /**
     * 1. check filename
     * 2. check data & datasize
     * 3. check FindFile to existence
     * 4. find free space
     *    a) loop until found:
     *    - empty/broken file header
     *    - nextaddress is zero(FF)/overspace
     * 5. update previous file header if non zero
     * 6. write new file header
     * 7. write data
     *
     */

    if (!filename || strlen(filename) > FILE_NAME_LENGTH) {
        debug("EEPROM_AddFile: %s %s %u\n", "FILENAMENOTVALID", filename, dataSize);
        return FILENAMENOTVALID;
    }

    if (!data || dataSize == 0) {
        debug("EEPROM_AddFile: %s\n", "BUFFERNOTVALID");
        return BUFFERNOTVALID;
    }


    uint16_t currentAddress = sizeof(JEEPROMHeader); // Starting after the EEPROM header
    uint16_t previousAddress;
    JEEFSFileHeader currentFileHeader;

    if (EEPROM_FindFile(eeprom_descriptor, filename, &currentFileHeader, &previousAddress) == 1) {
        debug("EEPROM_AddFile: file already exists: %s\n", filename);
        // TODO: Update file or return error?
        return 0; // File already exists
    }
    previousAddress = 0;
    ssize_t readSize;

    debug("EEPROM_AddFile: file %s not found. add new\n", filename);

    while (!EEPROM_WordIsEmpty(currentAddress) && currentAddress < eeprom_descriptor.eeprom_size - sizeof(JEEFSFileHeader)) {

        readSize = eeprom_read(eeprom_descriptor, &currentFileHeader, sizeof(JEEFSFileHeader), currentAddress);

        // If the current slot is empty (first time adding a file or a deleted slot) or there's an error in reading
        if (readSize != sizeof(JEEFSFileHeader)) {
            debug("EEPROM_AddFile: read eeprom error %s %li != %li\n", filename, readSize, sizeof(JEEFSFileHeader));
            return EEPROMREADERROR; // Read error
        }

        if (EEPROM_ByteIsEmpty(currentFileHeader.name[0])
        || EEPROM_WordIsEmpty(currentFileHeader.dataSize)
        ||
            (   !EEPROM_WordIsEmpty(currentFileHeader.nextFileAddress)
                && currentFileHeader.nextFileAddress != currentFileHeader.dataSize + sizeof (JEEFSFileHeader) + currentAddress
                )
        ) {
            break; // Found empty slot or read error occurred
        }

        // TODO : read and check crc32 of data
        // if (currentFileHeader.crc32 == calculateCRC32(data, dataSize))

        previousAddress = currentAddress;
        // TODO: select corruption of all header or only nextFileAddress?
        /*if(currentFileHeader.nextFileAddress != currentFileHeader.dataSize + sizeof (JEEFSFileHeader)) {
            // TODO: fix corruption, maybe restore or return error?
            // return EEPROMCORRUPTED;
            currentFileHeader.nextFileAddress = 0;
            break; // Found empty slot or read error occurred
        }*/
        currentAddress = currentFileHeader.nextFileAddress; // Move to next file

    }

    // Exit from loop in search of empty space
    // assume that currentAddress is empty or corrupted
    // assume that previousAddress is valid or zero
    // so if previousAddress is zero then currentAddress is first file and do not update previous file header
    // if previousAddress is not zero then currentAddress is not first file and update previous file header
    // write code below:

    if (previousAddress) {
        // TODO: check on read error
        readSize = eeprom_read(eeprom_descriptor, &currentFileHeader, sizeof(JEEFSFileHeader), previousAddress);
        currentFileHeader.nextFileAddress = previousAddress + sizeof(JEEFSFileHeader) + currentFileHeader.dataSize;
        currentAddress = currentFileHeader.nextFileAddress;
    } else
        currentAddress = sizeof(JEEPROMHeader);

    // Check if there's enough space to write the new file
    if (currentAddress + sizeof(currentFileHeader) + dataSize >= eeprom_descriptor.eeprom_size) {
        debug("EEPROM_AddFile: not enough space %s %u %u eeprom_size: %lu\n", filename, currentAddress, dataSize, eeprom_descriptor.eeprom_size);
        return NOTENOUGHSPACE;  // Not enough space
    }

    // TODO: check on write error
    if (previousAddress)
        readSize = eeprom_write(eeprom_descriptor, &currentFileHeader, sizeof(JEEFSFileHeader), previousAddress);


    // Prepare and write the new file header
    memset(&currentFileHeader, 0, sizeof(JEEFSFileHeader));
    strncpy(currentFileHeader.name, filename, FILE_NAME_LENGTH);
    currentFileHeader.dataSize = dataSize;
    currentFileHeader.crc32 = calculateCRC32(data, dataSize);
    currentFileHeader.nextFileAddress = 0; // Currently, it's the last file


    // Write the new file header
    ssize_t writeSize;
    writeSize = eeprom_write(eeprom_descriptor, &currentFileHeader, sizeof(JEEFSFileHeader), currentAddress);
    if (writeSize != sizeof(JEEFSFileHeader)) {
        debug("EEPROM_AddFile: write lastFile header eeprom error %s %li != %li\n", filename, writeSize, sizeof(JEEFSFileHeader));
        return -1; // Write error
    }

    // Write the file data
    writeSize = eeprom_write(eeprom_descriptor, data, dataSize, currentAddress + sizeof(JEEFSFileHeader));
    if (writeSize != dataSize) {
        debug("EEPROM_AddFile: write data eeprom error %s %lu != %i\n", filename, writeSize, dataSize);
        return -1; // Write error
    }
    debug("EEPROM_AddFile: write data eeprom ok %s %li seek:%i\n", filename, writeSize, currentAddress);

    return (int16_t)(dataSize%INT16_MAX); // Return number of data bytes written
}



int16_t EEPROM_DeleteFile(EEPROMDescriptor descriptor, const char *filename) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    JEEFSFileHeader header;
    uint16_t address;

    int found = EEPROM_FindFile(descriptor, filename, &header, &address);
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
    uint8_t clearByte = EEPROM_EMPTYBYTE;
    for (uint16_t i = 0; i < shiftSize && (readAddress - i) < descriptor.eeprom_size; i++) {
        eeprom_write(descriptor, &clearByte, 1, readAddress - i - 1);
    }

    return 1;  // Successfully deleted
}

int16_t EEPROM_FindFile(EEPROMDescriptor eeprom_descriptor, const char *filename, JEEFSFileHeader *header, uint16_t *address) {
    if (!filename || strlen(filename) > FILE_NAME_LENGTH)
        return FILENAMENOTVALID;

    uint16_t currentAddress = sizeof(JEEPROMHeader); // Starting after the EEPROM header

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

        currentAddress = EEPROM_getNextFileAddress(eeprom_descriptor, currentAddress);
        if (currentAddress == 0) {
            break; // End of file list
        }
    }

    return 0; // File not found
}

inline uint32_t calculateCRC32(const uint8_t *data, size_t length) {
    return crc32(0L, data, length);
}

uint16_t EEPROM_getNextFileAddress(EEPROMDescriptor eeprom_descriptor, uint16_t currentAddress) {
    JEEFSFileHeader fileHeader;
    if (eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeader), currentAddress) != sizeof(JEEFSFileHeader)) {
        return 0; // Error reading header
    }
    return fileHeader.nextFileAddress;
}


int EEPROM_SetHeader(EEPROMDescriptor eeprom_descriptor, JEEPROMHeader header) {
    header.crc32 = calculateCRC32((uint8_t *) &header, sizeof(JEEPROMHeader) - sizeof(header.crc32));
    return eeprom_write(eeprom_descriptor, &header, sizeof(JEEPROMHeader), 0) == sizeof(JEEPROMHeader);
}

int16_t EEPROM_HeaderCheckConsistency(EEPROMDescriptor eeprom_descriptor)
{
    JEEPROMHeader header = EEPROM_GetHeader(eeprom_descriptor);
    uint32_t crc32old = header.crc32;
    //header.crc32 = 0;
    // check header with magic "JetHome" in begin:
    if (strncmp(header.magic, "JetHome", 7) != 0) {
        debug("EEPROM_HeaderCheckConsistency: magic error %.8s\n", header.magic);
        return -1;
    }
    uint32_t crc32_calc = calculateCRC32((uint8_t *) &header, sizeof(JEEPROMHeader) - sizeof((&header)->crc32));
    if (crc32_calc != crc32old) {
        debug("EEPROM_HeaderCheckConsistency: crc32 error %u != %u\n", crc32_calc, crc32old);
        return -1;
    }
    return 0;
}

int EEPROM_FormatEEPROM(EEPROMDescriptor ep){
    uint8_t buffer[ep.eeprom_size];
    JEEPROMHeader *header = (JEEPROMHeader *) buffer;

    memset(buffer, EEPROM_EMPTYBYTE, ep.eeprom_size);
    //memset(&header, 0, sizeof(JEEPROMHeader));
    strncpy(header->magic, "JetHome", 7);
    header->crc32 = calculateCRC32((uint8_t *)header, sizeof(JEEPROMHeader) - sizeof(header->crc32));
    debug("EEPROM_FormatEEPROM: crc32: %x buffer size:%lu header size: %lu\n", header->crc32, ep.eeprom_size, sizeof(*header));
    /*EEPROM_SetHeader(ep, header);*/
    eeprom_write(ep, &buffer, ep.eeprom_size, 0);
    return 1;
}

inline bool EEPROM_ByteIsEmpty(char var) {
    return var == '\xFF' || var == '\0';
}
inline bool EEPROM_WordIsEmpty(uint16_t var) {
    return var == 0xFFFF || var == 0x0000;
}

inline bool EEPROM_QWordIsEmpty(uint32_t var) {
    return var == 0xFFFFFFFF || var == 0x00000000;
}
