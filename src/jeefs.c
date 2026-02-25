// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <assert.h>
#include <string.h>
#include <zlib.h>

#include "jeefs.h"

#include <stdlib.h>

#include "debug.h"
#include "eepromerr.h"

// Internal functions

// use libz implementation of crc32
static uint32_t calculateCRC32(const uint8_t *data, size_t length);
static int inline EEPROM_GetHeaderSize(void *header);
static int inline EEPROM_GetHeaderSize_read(EEPROMDescriptor eeprom_descriptor);
static int16_t EEPROM_FindFile(EEPROMDescriptor eeprom_descriptor,
                               const char *filename, JEEFSFileHeaderv1 *header,
                               uint16_t *address);
static uint16_t EEPROM_getNextFileAddress(EEPROMDescriptor eeprom_descriptor,
                                          uint16_t currentAddress);
static inline bool EEPROM_ByteIsEmpty(char var);
static inline bool EEPROM_WordIsEmpty(uint16_t var);
static inline bool EEPROM_QWordIsEmpty(uint32_t var);

/*
 * JEEFS functions
 */

EEPROMDescriptor EEPROM_OpenEEPROM(const char *pathname, uint16_t eeprom_size) {
  EEPROMDescriptor desc;
  desc = eeprom_open(pathname, eeprom_size);
  if (desc.eeprom_fid == -1)
    return desc; // handle error

  return desc;
}

int inline EEPROM_GetHeaderSize_read(EEPROMDescriptor eeprom_descriptor) {
  JEEPROMHeaderversion header;
  eeprom_read(eeprom_descriptor, &header, sizeof(JEEPROMHeaderversion), 0);
  return EEPROM_GetHeaderSize(&header);
}

int inline EEPROM_GetHeaderSize(void *header) {
  if (strncmp(((JEEPROMHeaderversion *)header)->magic, MAGIC, MAGIC_LENGTH) !=
      0) {
    debug("EEPROM_GetHeaderSize: magic error %.8s\n",
          ((JEEPROMHeaderversion *)header)->magic);
    return -1; // Invalid header
  }
  int ver = ((JEEPROMHeaderversion *)header)->version;
  int size;
  switch (ver) {
  case 1:
    size = sizeof(JEEPROMHeaderv1);
    break;
  case 2:
    size = sizeof(JEEPROMHeaderv2);
    break;
  case 3:
    size = sizeof(JEEPROMHeaderv3);
    break;
  default:
    debug("EEPROM_GetHeaderSize: version error %u\n",
          ((JEEPROMHeaderversion *)header)->version);
    return -1; // Unsupported version
  }
  return size;
}

int EEPROM_GetHeader(EEPROMDescriptor eeprom_descriptor, void *header,
                     int size) {
  if (size < sizeof(JEEPROMHeaderversion)) {
    debug("EEPROM_GetHeader: size error %d<%d\n", size,
          sizeof(JEEPROMHeaderversion));
    return -1; // Invalid size
  }
  eeprom_read(eeprom_descriptor, header, sizeof(JEEPROMHeaderversion), 0);
  int _size = EEPROM_GetHeaderSize(header);
  if (_size < 0) {
    debug("EEPROM_GetHeader: header size error %d\n", _size);
    return -1; // Invalid header
  }
  if (size < _size) {
    debug("EEPROM_GetHeader: buffer size error %d<%d\n", size, _size);
    return -1; // Invalid size
  }

  eeprom_read(eeprom_descriptor, header, size, 0);
  return 0;
}

int EEPROM_CloseEEPROM(EEPROMDescriptor eeprom_descriptor) {
  return eeprom_close(eeprom_descriptor);
}

int16_t EEPROM_ListFiles(EEPROMDescriptor eeprom_descriptor,
                         char fileList[][FILE_NAME_LENGTH], uint16_t maxFiles) {
  int16_t count = 0;
  uint16_t currentAddress = EEPROM_GetHeaderSize_read(
      eeprom_descriptor); // Assuming the EEPROM starts with the header

  while (count < maxFiles) {
    JEEFSFileHeaderv1 fileHeader;
    if (eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeaderv1),
                    currentAddress) != sizeof(JEEFSFileHeaderv1)) {
      break; // End of list or error
    }
    strncpy(fileList[count], fileHeader.name, FILE_NAME_LENGTH);
    count++;
    currentAddress =
        EEPROM_getNextFileAddress(eeprom_descriptor, currentAddress);
    if (currentAddress == 0) {
      break; // End of file list
    }
  }
  return count;
}

int16_t EEPROM_ReadFile(EEPROMDescriptor eeprom_descriptor,
                        const char *filename, uint8_t *buffer,
                        uint16_t bufferSize) {
  if (!filename || strlen(filename) > FILE_NAME_LENGTH)
    return FILENAMENOTVALID;

  if (!buffer || bufferSize == 0)
    return BUFFERNOTVALID;

  JEEFSFileHeaderv1 fileHeader;
  uint16_t fileAddress;
  if (EEPROM_FindFile(eeprom_descriptor, filename, &fileHeader, &fileAddress) !=
      1) {
    return FILENOTFOUND; // File not found
  }
  if (fileHeader.dataSize > bufferSize) {
    return BUFFERNOTVALID; // Provided buffer is too small
  }

  ssize_t readSize = eeprom_read(eeprom_descriptor, buffer, fileHeader.dataSize,
                                 fileAddress + sizeof(JEEFSFileHeaderv1));
  return readSize > 0 ? readSize : -1; // Return read bytes or 0 on error
}

int16_t EEPROM_WriteFile(EEPROMDescriptor eeprom_descriptor,
                         const char *filename, const uint8_t *data,
                         uint16_t dataSize) {
  if (!filename || strlen(filename) > FILE_NAME_LENGTH)
    return FILENAMENOTVALID;

  if (!data || dataSize == 0)
    return BUFFERNOTVALID;

  JEEFSFileHeaderv1 fileHeader;
  uint16_t fileAddress;
  if (EEPROM_FindFile(eeprom_descriptor, filename, &fileHeader, &fileAddress) !=
      1)
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
  if (eeprom_write(eeprom_descriptor, data, dataSize,
                   fileAddress + sizeof(JEEFSFileHeaderv1)) != dataSize) {
    return 0; // Write error
  }

  // Update the CRC
  fileHeader.crc32 = calculateCRC32(data, dataSize);
  eeprom_write(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeaderv1),
               fileAddress);

  return dataSize;
}

int16_t EEPROM_AddFile(EEPROMDescriptor eeprom_descriptor, const char *filename,
                       const uint8_t *data, uint16_t dataSize) {
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

  uint16_t headerSize = EEPROM_GetHeaderSize_read(eeprom_descriptor);
  uint16_t currentAddress = headerSize; // Starting after the EEPROM header
  uint16_t previousAddress;
  JEEFSFileHeaderv1 currentFileHeader;

  if (EEPROM_FindFile(eeprom_descriptor, filename, &currentFileHeader,
                      &previousAddress) == 1) {
    debug("EEPROM_AddFile: file already exists: %s\n", filename);
    // TODO: Update file or return error?
    return 0; // File already exists
  }
  previousAddress = 0;
  ssize_t readSize;

  debug("EEPROM_AddFile: file %s not found. add new\n", filename);

  while (!EEPROM_WordIsEmpty(currentAddress) &&
         currentAddress <
             eeprom_descriptor.eeprom_size - sizeof(JEEFSFileHeaderv1)) {

    readSize = eeprom_read(eeprom_descriptor, &currentFileHeader,
                           sizeof(JEEFSFileHeaderv1), currentAddress);

    // If the current slot is empty (first time adding a file or a deleted slot)
    // or there's an error in reading
    if (readSize != sizeof(JEEFSFileHeaderv1)) {
      debug("EEPROM_AddFile: read eeprom error %s %li != %li\n", filename,
            readSize, sizeof(JEEFSFileHeader));
      return EEPROMREADERROR; // Read error
    }

    if (EEPROM_ByteIsEmpty(currentFileHeader.name[0]) ||
        EEPROM_WordIsEmpty(currentFileHeader.dataSize) ||
        (!EEPROM_WordIsEmpty(currentFileHeader.nextFileAddress) &&
         currentFileHeader.nextFileAddress != currentFileHeader.dataSize +
                                                  sizeof(JEEFSFileHeaderv1) +
                                                  currentAddress)) {
      break; // Found empty slot or read error occurred
    }

    // TODO : read and check crc32 of data
    // if (currentFileHeader.crc32 == calculateCRC32(data, dataSize))

    previousAddress = currentAddress;
    // TODO: select corruption of all header or only nextFileAddress?
    /*if(currentFileHeader.nextFileAddress != currentFileHeader.dataSize +
    sizeof (JEEFSFileHeader)) {
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
  // so if previousAddress is zero then currentAddress is first file and do not
  // update previous file header if previousAddress is not zero then
  // currentAddress is not first file and update previous file header write code
  // below:

  if (previousAddress) {
    // TODO: check on read error
    readSize = eeprom_read(eeprom_descriptor, &currentFileHeader,
                           sizeof(JEEFSFileHeaderv1), previousAddress);
    currentFileHeader.nextFileAddress = previousAddress +
                                        sizeof(JEEFSFileHeaderv1) +
                                        currentFileHeader.dataSize;
    currentAddress = currentFileHeader.nextFileAddress;
  } else
    currentAddress = headerSize;

  // Check if there's enough space to write the new file
  if (currentAddress + sizeof(currentFileHeader) + dataSize >=
      eeprom_descriptor.eeprom_size) {
    debug("EEPROM_AddFile: not enough space %s %u %u eeprom_size: %lu\n",
          filename, currentAddress, dataSize, eeprom_descriptor.eeprom_size);
    return NOTENOUGHSPACE; // Not enough space
  }

  // TODO: check on write error
  if (previousAddress)
    readSize = eeprom_write(eeprom_descriptor, &currentFileHeader,
                            sizeof(JEEFSFileHeaderv1), previousAddress);

  // Prepare and write the new file header
  memset(&currentFileHeader, 0, sizeof(JEEFSFileHeaderv1));
  strncpy(currentFileHeader.name, filename, FILE_NAME_LENGTH);
  currentFileHeader.dataSize = dataSize;
  currentFileHeader.crc32 = calculateCRC32(data, dataSize);
  currentFileHeader.nextFileAddress = 0; // Currently, it's the last file

  // Write the new file header
  ssize_t writeSize;
  writeSize = eeprom_write(eeprom_descriptor, &currentFileHeader,
                           sizeof(JEEFSFileHeaderv1), currentAddress);
  if (writeSize != sizeof(JEEFSFileHeaderv1)) {
    debug("EEPROM_AddFile: write lastFile header eeprom error %s %li != %li\n",
          filename, writeSize, sizeof(JEEFSFileHeader));
    return -1; // Write error
  }

  // Write the file data
  writeSize = eeprom_write(eeprom_descriptor, data, dataSize,
                           currentAddress + sizeof(JEEFSFileHeaderv1));
  if (writeSize != dataSize) {
    debug("EEPROM_AddFile: write data eeprom error %s %lu != %i\n", filename,
          writeSize, dataSize);
    return -1; // Write error
  }
  debug("EEPROM_AddFile: write data eeprom ok %s %li seek:%i\n", filename,
        writeSize, currentAddress);

  return (int16_t)(dataSize % INT16_MAX); // Return number of data bytes written
}

int16_t EEPROM_DeleteFile(EEPROMDescriptor descriptor, const char *filename) {
  if (!filename || strlen(filename) > FILE_NAME_LENGTH)
    return FILENAMENOTVALID;

  JEEFSFileHeaderv1 header;
  uint16_t address;

  int found = EEPROM_FindFile(descriptor, filename, &header, &address);
  if (found <= 0) {
    return FILENOTFOUND; // File not found or error
  }

  // The address after the file we're deleting
  uint16_t nextAddress = address + sizeof(JEEFSFileHeaderv1) + header.dataSize;

  // Move all subsequent files up to fill the space of the deleted file
  uint16_t shiftSize = sizeof(JEEFSFileHeaderv1) + header.dataSize;
  uint8_t buffer[shiftSize]; // Using a buffer equal to the deleted file size
                             // for simplicity
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
  for (uint16_t i = 0;
       i < shiftSize && (readAddress - i) < descriptor.eeprom_size; i++) {
    eeprom_write(descriptor, &clearByte, 1, readAddress - i - 1);
  }

  return 1; // Successfully deleted
}

int16_t EEPROM_FindFile(EEPROMDescriptor eeprom_descriptor,
                        const char *filename, JEEFSFileHeaderv1 *header,
                        uint16_t *address) {
  if (!filename || strlen(filename) > FILE_NAME_LENGTH)
    return FILENAMENOTVALID;

  int headerSize = EEPROM_GetHeaderSize_read(eeprom_descriptor);
  if (headerSize < 0)
    return -1;
  uint16_t currentAddress = (uint16_t)headerSize;

  while (1) {
    JEEFSFileHeaderv1 fileHeader;
    ssize_t readSize = eeprom_read(eeprom_descriptor, &fileHeader,
                                   sizeof(JEEFSFileHeaderv1), currentAddress);
    if (readSize != sizeof(JEEFSFileHeaderv1)) {
      return -1; // File not found or read error
    }

    if (strncmp(fileHeader.name, filename, FILE_NAME_LENGTH) == 0) {
      // Found the file
      if (header) {
        memcpy(header, &fileHeader, sizeof(JEEFSFileHeaderv1));
      }
      if (address) {
        *address = currentAddress;
      }
      return 1; // File found
    }

    currentAddress =
        EEPROM_getNextFileAddress(eeprom_descriptor, currentAddress);
    if (currentAddress == 0) {
      break; // End of file list
    }
  }

  return 0; // File not found
}

inline uint32_t calculateCRC32(const uint8_t *data, size_t length) {
  return crc32(0L, data, length);
}

uint16_t EEPROM_getNextFileAddress(EEPROMDescriptor eeprom_descriptor,
                                   uint16_t currentAddress) {
  JEEFSFileHeaderv1 fileHeader;
  if (eeprom_read(eeprom_descriptor, &fileHeader, sizeof(JEEFSFileHeaderv1),
                  currentAddress) != sizeof(JEEFSFileHeaderv1)) {
    return 0; // Error reading header
  }
  return fileHeader.nextFileAddress;
}

int EEPROM_SetHeader(EEPROMDescriptor eeprom_descriptor, void *header) {
  if (!header) {
    debug("EEPROM_SetHeader: %s\n", "HEADERNOTVALID");
    return -1; // @TODO: errno
  }
  JEEPROMHeaderversion *h = (JEEPROMHeaderversion *)header;
  if (strncmp(h->magic, MAGIC, MAGIC_LENGTH) != 0) {
    debug("EEPROM_SetHeader: magic error %s\n", h->magic);
    return -1; // @TODO: errno
  }
  int size;
  if (h->version == 1) {
    ((JEEPROMHeaderv1 *)header)->crc32 = calculateCRC32(
        (uint8_t *)header,
        sizeof(JEEPROMHeaderv1) - sizeof(((JEEPROMHeaderv1 *)header)->crc32));
    size = sizeof(JEEPROMHeaderv1);
  } else if (h->version == 2) {
    ((JEEPROMHeaderv2 *)header)->crc32 = calculateCRC32(
        (uint8_t *)header,
        sizeof(JEEPROMHeaderv2) - sizeof(((JEEPROMHeaderv2 *)header)->crc32));
    size = sizeof(JEEPROMHeaderv2);
  } else if (h->version == 3) {
    ((JEEPROMHeaderv3 *)header)->crc32 = calculateCRC32(
        (uint8_t *)header,
        sizeof(JEEPROMHeaderv3) - sizeof(((JEEPROMHeaderv3 *)header)->crc32));
    size = sizeof(JEEPROMHeaderv3);
  } else {
    debug("EEPROM_SetHeader: %s\n", "UNKNOWNVERSION");
    return -1; // @TODO: errno
  }
  return -(eeprom_write(eeprom_descriptor, header, size, 0) ==
           size); // @TODO: errno
}

int16_t EEPROM_HeaderCheckConsistency(EEPROMDescriptor eeprom_descriptor) {
  int headersize = EEPROM_GetHeaderSize_read(eeprom_descriptor);
  if (headersize < 0) {
    debug("EEPROM_HeaderCheckConsistency: invalid header\n");
    return -1;
  }
  union JEEPROMHeader header;
  int result = EEPROM_GetHeader(eeprom_descriptor, &header, headersize);
  if (result < 0) {
    debug("EEPROM_HeaderCheckConsistency: read error\n");
    return -1;
  }
  uint32_t crc32old = 0;
  uint32_t crc32_calc = 0;

  switch (header.version.version) {
  case 1:
    crc32old = header.v1.crc32;
    crc32_calc = calculateCRC32((uint8_t *)&header,
                                headersize - sizeof(header.v1.crc32));
    break;
  case 2:
    crc32old = header.v2.crc32;
    crc32_calc = calculateCRC32((uint8_t *)&header,
                                headersize - sizeof(header.v2.crc32));
    break;
  case 3:
    crc32old = header.v3.crc32;
    crc32_calc = calculateCRC32((uint8_t *)&header,
                                headersize - sizeof(header.v3.crc32));
    break;
  }
  if (!crc32old || (crc32_calc != crc32old)) {
    debug("EEPROM_HeaderCheckConsistency: crc32 error %u != %u\n", crc32_calc,
          crc32old);
    return -1;
  }
  return 0;
}

// Format EEPROM
int EEPROM_FormatEEPROM(EEPROMDescriptor ep, int version) {
  uint8_t buffer[ep.eeprom_size];
  memset(buffer, EEPROM_EMPTYBYTE, ep.eeprom_size);
  union JEEPROMHeader *header = (union JEEPROMHeader *)buffer;
  memcpy(header->version.magic, MAGIC, MAGIC_LENGTH);
  header->version.version = version;
  switch (version) {
  case 1:
    header->v1.crc32 = calculateCRC32(
        (uint8_t *)header, sizeof(JEEPROMHeaderv1) - sizeof(header->v1.crc32));
    debug("EEPROM_FormatEEPROM: crc32: %x buffer size:%lu header size: %lu\n",
          header->v1.crc32, ep.eeprom_size, sizeof(header->v1));
    break;
  case 2:
    header->v2.crc32 = calculateCRC32(
        (uint8_t *)header, sizeof(JEEPROMHeaderv2) - sizeof(header->v2.crc32));
    debug("EEPROM_FormatEEPROM: crc32: %x buffer size:%lu header size: %lu\n",
          header->v2.crc32, ep.eeprom_size, sizeof(header->v2));
    break;
  case 3:
    header->v3.signature_version = JEEFS_SIG_NONE;
    header->v3.timestamp = 0;
    memset(header->v3.signature, 0, SIGNATURE_FIELD_SIZE);
    header->v3.crc32 = calculateCRC32(
        (uint8_t *)header, sizeof(JEEPROMHeaderv3) - sizeof(header->v3.crc32));
    debug("EEPROM_FormatEEPROM: crc32: %x buffer size:%lu header size: %lu\n",
          header->v3.crc32, ep.eeprom_size, sizeof(header->v3));
    break;
  default:
    debug("EEPROM_FormatEEPROM: %s\n", "UNKNOWNVERSION");
    return -1;
  }
  eeprom_write(ep, buffer, ep.eeprom_size, 0);
  return 0;
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
