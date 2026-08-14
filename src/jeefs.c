// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023-2026 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 */

#include <string.h>
#include <zlib.h>

#include "jeefs.h"

#include "debug.h"
#include "eepromerr.h"
#include "jeefs_header.h"

/*
 * The file chain is contiguous by construction (filesystem-v1.md): the
 * first file header starts right after the EEPROM header, every next file
 * starts at previous + sizeof(JEEFSFileHeaderv1) + dataSize, and
 * nextFileAddress duplicates that address (0 terminates the chain). Every
 * walk below re-validates the invariant before trusting any field, so a
 * single corrupted byte terminates the operation with EEPROMCORRUPTED
 * instead of hanging or writing through a wild pointer. Empty bytes are
 * 0x00 or 0xFF (erased medium) — issue #14.
 */

// Fixed-size scratch for chunked copy/fill: no VLAs, bounded stack (#7).
#define JEEFS_CHUNK 64

// Internal helpers. Header parsing (version detection, sizes, header CRC)
// lives in jeefs_header.c — this file only adds I/O around it. The local
// calculateCRC32 covers file *data* checksums; it moves to the port layer
// together with the header library's copy (#24).

static uint32_t calculateCRC32(const uint8_t *data, size_t length);
static int EEPROM_GetHeaderSize_read(EEPROMDescriptor eeprom_descriptor);
static inline bool EEPROM_ByteIsEmpty(char var);

// Validated chain iterator.
typedef struct {
  uint16_t addr;      // current file header address (valid after step == 1)
  uint16_t prev;      // predecessor header address, 0 = current is first
  uint16_t next_addr; // where the next step will read
  JEEFSFileHeaderv1 hdr;
} JEEFSIter;

static int16_t iter_begin(EEPROMDescriptor ep, JEEFSIter *it) {
  int header_size = EEPROM_GetHeaderSize_read(ep);
  if (header_size < 0)
    return EEPROMCORRUPTED;
  it->addr = 0;
  it->prev = 0;
  it->next_addr = (uint16_t)header_size;
  memset(&it->hdr, 0, sizeof(it->hdr));
  return 0;
}

// Advance to the next file. Returns 1 when a validated file header is
// loaded into it->hdr, 0 at the end of the chain, negative EEPROMError.
static int16_t iter_step(EEPROMDescriptor ep, JEEFSIter *it) {
  if (it->next_addr == 0)
    return 0; // previous file was terminal

  uint32_t start = it->next_addr;
  if (start + sizeof(JEEFSFileHeaderv1) > ep.eeprom_size)
    return 0; // no room for another header: clean end

  JEEFSFileHeaderv1 hdr;
  if (eeprom_read(ep, &hdr, sizeof(hdr), (uint16_t)start) != sizeof(hdr))
    return EEPROMREADERROR;

  if (EEPROM_ByteIsEmpty(hdr.name[0]))
    return 0; // unwritten slot (0x00 or 0xFF): end of chain

  // A written header must carry a terminated name and a sane size.
  if (hdr.name[FILE_NAME_LENGTH] != '\0')
    return EEPROMCORRUPTED;
  if (hdr.dataSize == 0 || hdr.dataSize == 0xFFFF)
    return EEPROMCORRUPTED;

  uint32_t end = start + sizeof(JEEFSFileHeaderv1) + hdr.dataSize;
  if (end > ep.eeprom_size)
    return EEPROMCORRUPTED;

  // An erased link (0xFFFF) terminates the chain like 0 — RFC #14: the
  // file may have been written onto erased media without a link update.
  if (hdr.nextFileAddress == 0xFFFF)
    hdr.nextFileAddress = 0;

  // Contiguity: the link either terminates or names exactly the next slot,
  // and a claimed successor must have room for its own header. This also
  // makes cycles impossible — addresses strictly increase.
  if (hdr.nextFileAddress != 0 &&
      (hdr.nextFileAddress != end ||
       end + sizeof(JEEFSFileHeaderv1) > ep.eeprom_size))
    return EEPROMCORRUPTED;

  it->prev = it->addr;
  it->addr = (uint16_t)start;
  it->next_addr = hdr.nextFileAddress;
  it->hdr = hdr;
  return 1;
}

// Walk the whole chain. Returns 1 if filename was found (iterator left on
// it), 0 if not found, negative EEPROMError. chain_end receives the first
// byte past the last file's data (== first free byte); with no files it is
// the header size. found (optional) receives the match while the walk
// continues to the end, so chain_end is always complete.
static int16_t chain_walk(EEPROMDescriptor ep, const char *filename,
                          JEEFSIter *found, uint32_t *chain_end) {
  JEEFSIter it;
  int16_t ret = iter_begin(ep, &it);
  if (ret < 0)
    return ret;

  uint32_t end = it.next_addr;
  int16_t hit = 0;
  uint16_t guard = 0;
  const uint16_t max_files =
      (uint16_t)(ep.eeprom_size / sizeof(JEEFSFileHeaderv1)) + 1;

  while ((ret = iter_step(ep, &it)) == 1) {
    if (++guard > max_files)
      return EEPROMCORRUPTED; // defense in depth, unreachable by invariant
    end = (uint32_t)it.addr + sizeof(JEEFSFileHeaderv1) + it.hdr.dataSize;
    if (filename &&
        strncmp(it.hdr.name, filename, FILE_NAME_LENGTH + 1) == 0) {
      hit = 1;
      if (found)
        *found = it;
    }
  }
  if (ret < 0)
    return ret;
  if (chain_end)
    *chain_end = end;
  return hit;
}

static bool filename_valid(const char *filename) {
  if (!filename)
    return false;
  size_t len = strnlen(filename, FILE_NAME_LENGTH + 1);
  return len > 0 && len <= FILE_NAME_LENGTH;
}

// Chunked copy of [src, src+count) to [dst, dst+count) with dst < src.
static int16_t move_down(EEPROMDescriptor ep, uint32_t src, uint32_t dst,
                         uint32_t count) {
  uint8_t chunk[JEEFS_CHUNK];
  while (count > 0) {
    uint16_t n = count > sizeof(chunk) ? (uint16_t)sizeof(chunk)
                                       : (uint16_t)count;
    if (eeprom_read(ep, chunk, n, (uint16_t)src) != n)
      return EEPROMREADERROR;
    if (eeprom_write(ep, chunk, n, (uint16_t)dst) != n)
      return EEPROMWRITEERROR;
    src += n;
    dst += n;
    count -= n;
  }
  return 0;
}

static int16_t fill_bytes(EEPROMDescriptor ep, uint32_t start, uint32_t count,
                          uint8_t value) {
  uint8_t chunk[JEEFS_CHUNK];
  memset(chunk, value, sizeof(chunk));
  while (count > 0) {
    uint16_t n = count > sizeof(chunk) ? (uint16_t)sizeof(chunk)
                                       : (uint16_t)count;
    if (eeprom_write(ep, chunk, n, (uint16_t)start) != n)
      return EEPROMWRITEERROR;
    start += n;
    count -= n;
  }
  return 0;
}

/*
 * Public API
 */

EEPROMDescriptor EEPROM_OpenEEPROM(const char *pathname, uint16_t eeprom_size) {
  return eeprom_open(pathname, eeprom_size);
}

int EEPROM_CloseEEPROM(EEPROMDescriptor eeprom_descriptor) {
  return eeprom_close(eeprom_descriptor);
}

static int EEPROM_GetHeaderSize_read(EEPROMDescriptor eeprom_descriptor) {
  JEEPROMHeaderversion header;
  if (eeprom_read(eeprom_descriptor, &header, sizeof(header), 0) !=
      sizeof(header))
    return -1;
  int version =
      jeefs_header_detect_version((const uint8_t *)&header, sizeof(header));
  if (version < 0)
    return -1;
  return jeefs_header_size(version);
}

int EEPROM_GetHeader(EEPROMDescriptor eeprom_descriptor, void *header,
                     int size) {
  if (!header || size < (int)sizeof(JEEPROMHeaderversion))
    return BUFFERNOTVALID;

  int header_size = EEPROM_GetHeaderSize_read(eeprom_descriptor);
  if (header_size < 0)
    return EEPROMCORRUPTED;
  if (size < header_size)
    return BUFFERNOTVALID;

  if (eeprom_read(eeprom_descriptor, header, (uint16_t)header_size, 0) !=
      header_size)
    return EEPROMREADERROR;
  return 0;
}

int EEPROM_SetHeader(EEPROMDescriptor eeprom_descriptor, void *header) {
  if (!header)
    return BUFFERNOTVALID;

  int version = jeefs_header_detect_version((const uint8_t *)header,
                                            sizeof(JEEPROMHeaderversion));
  if (version < 0)
    return EEPROMCORRUPTED;
  int size = jeefs_header_size(version);

  if (jeefs_header_update_crc((uint8_t *)header, (size_t)size) != 0)
    return EEPROMCORRUPTED;

  if (eeprom_write(eeprom_descriptor, header, (uint16_t)size, 0) != size)
    return EEPROMWRITEERROR;
  return 0;
}

int16_t EEPROM_HeaderCheckConsistency(EEPROMDescriptor eeprom_descriptor) {
  JEEPROMHeaderversion vh;
  if (eeprom_read(eeprom_descriptor, &vh, sizeof(vh), 0) != sizeof(vh))
    return EEPROMREADERROR; // an I/O failure is not "inconsistent"
  int version =
      jeefs_header_detect_version((const uint8_t *)&vh, sizeof(vh));
  if (version < 0)
    return 0; // bad magic/version: inconsistent
  int header_size = jeefs_header_size(version);

  union JEEPROMHeader header;
  if (eeprom_read(eeprom_descriptor, &header, (uint16_t)header_size, 0) !=
      header_size)
    return EEPROMREADERROR;

  return jeefs_header_verify_crc((const uint8_t *)&header,
                                 (size_t)header_size) == 0
             ? 1
             : 0;
}

int16_t EEPROM_ListFiles(EEPROMDescriptor eeprom_descriptor,
                         char fileList[][FILE_NAME_LENGTH + 1],
                         uint16_t maxFiles) {
  if (!fileList)
    return BUFFERNOTVALID;

  JEEFSIter it;
  int16_t ret = iter_begin(eeprom_descriptor, &it);
  if (ret < 0)
    return ret;

  int16_t count = 0;
  while ((ret = iter_step(eeprom_descriptor, &it)) == 1) {
    if ((uint16_t)count >= maxFiles)
      return count; // list full; the rest is still a valid chain
    memcpy(fileList[count], it.hdr.name, FILE_NAME_LENGTH);
    fileList[count][FILE_NAME_LENGTH] = '\0';
    count++;
  }
  return ret < 0 ? ret : count;
}

int16_t EEPROM_ReadFile(EEPROMDescriptor eeprom_descriptor,
                        const char *filename, uint8_t *buffer,
                        uint16_t bufferSize) {
  if (!filename_valid(filename))
    return FILENAMENOTVALID;
  if (!buffer || bufferSize == 0)
    return BUFFERNOTVALID;

  JEEFSIter found;
  int16_t ret = chain_walk(eeprom_descriptor, filename, &found, NULL);
  if (ret < 0)
    return ret;
  if (ret == 0)
    return FILENOTFOUND;

  if (found.hdr.dataSize > INT16_MAX)
    return EEPROMCORRUPTED; // count not representable in the return type
  if (found.hdr.dataSize > bufferSize)
    return BUFFERNOTVALID;

  uint16_t data_addr = found.addr + sizeof(JEEFSFileHeaderv1);
  if (eeprom_read(eeprom_descriptor, buffer, found.hdr.dataSize, data_addr) !=
      found.hdr.dataSize)
    return EEPROMREADERROR;

  if (calculateCRC32(buffer, found.hdr.dataSize) != found.hdr.crc32)
    return EEPROMCORRUPTED;

  return (int16_t)found.hdr.dataSize;
}

int16_t EEPROM_AddFile(EEPROMDescriptor eeprom_descriptor, const char *filename,
                       const uint8_t *data, uint16_t dataSize) {
  if (!filename_valid(filename))
    return FILENAMENOTVALID;
  if (!data || dataSize == 0 || dataSize > INT16_MAX)
    return BUFFERNOTVALID;

  JEEFSIter last;
  uint32_t chain_end;
  int16_t ret = chain_walk(eeprom_descriptor, filename, &last, &chain_end);
  if (ret < 0)
    return ret;
  if (ret == 1)
    return 0; // file already exists

  if (chain_end + sizeof(JEEFSFileHeaderv1) + dataSize >
      eeprom_descriptor.eeprom_size)
    return NOTENOUGHSPACE;

  // Write the new file completely before linking it into the chain.
  JEEFSFileHeaderv1 hdr;
  memset(&hdr, 0, sizeof(hdr));
  strncpy(hdr.name, filename, FILE_NAME_LENGTH);
  hdr.dataSize = dataSize;
  hdr.crc32 = calculateCRC32(data, dataSize);
  hdr.nextFileAddress = 0;

  uint16_t new_addr = (uint16_t)chain_end;
  if (eeprom_write(eeprom_descriptor, &hdr, sizeof(hdr), new_addr) !=
      sizeof(hdr))
    return EEPROMWRITEERROR;
  if (eeprom_write(eeprom_descriptor, (void *)data, dataSize,
                   new_addr + sizeof(hdr)) != dataSize)
    return EEPROMWRITEERROR;

  // Link the predecessessor (the last existing file), if any. The first file
  // needs no link: its position is implied by the header size.
  int header_size = EEPROM_GetHeaderSize_read(eeprom_descriptor);
  if (new_addr != (uint16_t)header_size) {
    // chain_walk left `last` on the final file only when it did not match;
    // re-walk to fetch the last header explicitly for clarity and safety.
    JEEFSIter it;
    ret = iter_begin(eeprom_descriptor, &it);
    if (ret < 0)
      return ret;
    JEEFSIter tail = it;
    while ((ret = iter_step(eeprom_descriptor, &it)) == 1)
      if (it.addr != new_addr)
        tail = it;
    if (ret < 0)
      return ret;
    tail.hdr.nextFileAddress = new_addr;
    if (eeprom_write(eeprom_descriptor, &tail.hdr, sizeof(tail.hdr),
                     tail.addr) != sizeof(tail.hdr))
      return EEPROMWRITEERROR;
  }

  return (int16_t)dataSize;
}

int16_t EEPROM_DeleteFile(EEPROMDescriptor descriptor, const char *filename) {
  if (!filename_valid(filename))
    return FILENAMENOTVALID;

  JEEFSIter victim;
  uint32_t chain_end;
  int16_t ret = chain_walk(descriptor, filename, &victim, &chain_end);
  if (ret < 0)
    return ret;
  if (ret == 0)
    return FILENOTFOUND;

  uint32_t shift = sizeof(JEEFSFileHeaderv1) + victim.hdr.dataSize;
  uint32_t tail_start = victim.addr + shift; // first byte after the victim

  if (victim.hdr.nextFileAddress == 0) {
    // Victim is the last file: terminate the predecessor and wipe.
    if (victim.prev != 0) {
      JEEFSFileHeaderv1 prev_hdr;
      if (eeprom_read(descriptor, &prev_hdr, sizeof(prev_hdr), victim.prev) !=
          sizeof(prev_hdr))
        return EEPROMREADERROR;
      prev_hdr.nextFileAddress = 0;
      if (eeprom_write(descriptor, &prev_hdr, sizeof(prev_hdr), victim.prev) !=
          sizeof(prev_hdr))
        return EEPROMWRITEERROR;
    }
    ret = fill_bytes(descriptor, victim.addr, shift, EEPROM_EMPTYBYTE);
    return ret < 0 ? ret : 1;
  }

  // Compact: move [tail_start, chain_end) down by `shift`. The successor
  // lands exactly at the victim's address, so the predecessor's link (which
  // already names that address) stays valid without a write.
  ret = move_down(descriptor, tail_start, victim.addr, chain_end - tail_start);
  if (ret < 0)
    return ret;

  // Rewrite the moved headers' absolute links.
  uint16_t addr = victim.addr;
  while (addr != 0) {
    JEEFSFileHeaderv1 hdr;
    if (eeprom_read(descriptor, &hdr, sizeof(hdr), addr) != sizeof(hdr))
      return EEPROMREADERROR;
    if (hdr.nextFileAddress == 0 || hdr.nextFileAddress == 0xFFFF) {
      break; // terminal link (0 or erased) needs no rewrite
    }
    hdr.nextFileAddress = (uint16_t)(hdr.nextFileAddress - shift);
    if (eeprom_write(descriptor, &hdr, sizeof(hdr), addr) != sizeof(hdr))
      return EEPROMWRITEERROR;
    addr = hdr.nextFileAddress;
  }

  // Wipe the freed span at the old end of the chain.
  ret = fill_bytes(descriptor, chain_end - shift, shift, EEPROM_EMPTYBYTE);
  return ret < 0 ? ret : 1;
}

int16_t EEPROM_WriteFile(EEPROMDescriptor eeprom_descriptor,
                         const char *filename, const uint8_t *data,
                         uint16_t dataSize) {
  if (!filename_valid(filename))
    return FILENAMENOTVALID;
  if (!data || dataSize == 0 || dataSize > INT16_MAX)
    return BUFFERNOTVALID;

  JEEFSIter found;
  uint32_t chain_end;
  int16_t ret = chain_walk(eeprom_descriptor, filename, &found, &chain_end);
  if (ret < 0)
    return ret;
  if (ret == 0)
    return FILENOTFOUND;

  if (found.hdr.dataSize == dataSize) {
    // Same size: overwrite in place, then refresh the stored CRC.
    uint16_t data_addr = found.addr + sizeof(JEEFSFileHeaderv1);
    if (eeprom_write(eeprom_descriptor, (void *)data, dataSize, data_addr) !=
        dataSize)
      return EEPROMWRITEERROR;
    found.hdr.crc32 = calculateCRC32(data, dataSize);
    if (eeprom_write(eeprom_descriptor, &found.hdr, sizeof(found.hdr),
                     found.addr) != sizeof(found.hdr))
      return EEPROMWRITEERROR;
    return (int16_t)dataSize;
  }

  // Different size: ensure the delete + add cannot run out of space BEFORE
  // destroying the old content (#9: WriteFile must not lose the file).
  uint32_t old_span = sizeof(JEEFSFileHeaderv1) + found.hdr.dataSize;
  uint32_t needed = chain_end - old_span + sizeof(JEEFSFileHeaderv1) + dataSize;
  if (needed > eeprom_descriptor.eeprom_size)
    return NOTENOUGHSPACE;

  ret = EEPROM_DeleteFile(eeprom_descriptor, filename);
  if (ret < 0)
    return ret;
  return EEPROM_AddFile(eeprom_descriptor, filename, data, dataSize);
}

// Format EEPROM: write an initialized header, wipe the file area.
int EEPROM_FormatEEPROM(EEPROMDescriptor ep, int version) {
  int header_size = jeefs_header_size(version);
  if (header_size < 0)
    return EEPROMCORRUPTED;
  if ((uint32_t)header_size > ep.eeprom_size)
    return NOTENOUGHSPACE;

  union JEEPROMHeader header;
  if (jeefs_header_init((uint8_t *)&header, sizeof(header), version) != 0)
    return EEPROMCORRUPTED;

  if (eeprom_write(ep, &header, (uint16_t)header_size, 0) != header_size)
    return EEPROMWRITEERROR;
  return fill_bytes(ep, (uint32_t)header_size,
                    ep.eeprom_size - (uint32_t)header_size, EEPROM_EMPTYBYTE);
}

static uint32_t calculateCRC32(const uint8_t *data, size_t length) {
  return crc32(0L, data, length);
}

static inline bool EEPROM_ByteIsEmpty(char var) {
  return var == '\xFF' || var == '\0';
}
