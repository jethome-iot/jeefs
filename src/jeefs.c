// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023-2026 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "jeefs.h"

#include "eepromerr.h"
#include "jeefs_endian.h"
#include "jeefs_header.h"
#include "jeefs_port.h"

/*
 * Every operation works on a caller-owned image buffer (#25, variant A):
 * the environment reads the EEPROM itself, hands the bytes over, and
 * writes the buffer back if an operation mutated it. The library performs
 * no I/O, allocates nothing and keeps the stack bounded.
 *
 * The file chain is contiguous by construction (filesystem-v1.md): the
 * first file header starts right after the EEPROM header, every next file
 * starts at previous + sizeof(JEEFSFileHeaderv1) + dataSize, and
 * nextFileAddress duplicates that address (0 terminates the chain). Every
 * walk below re-validates the invariant before trusting any field, so a
 * single corrupted byte terminates the operation with EEPROMCORRUPTED
 * instead of hanging or writing through a wild pointer. Empty bytes are
 * 0x00 or 0xFF (erased medium) — issue #14.
 */

// Internal helpers. Header parsing (version detection, sizes, header CRC)
// lives in jeefs_header.c — this file only adds chain logic around it. The
// local calculateCRC32 covers file *data* checksums; it moves to the port
// layer together with the header library's copy (#24).

static uint32_t calculateCRC32(const uint8_t *data, size_t length);
static int header_size_of(const uint8_t *image, uint16_t imageSize);
static inline bool EEPROM_ByteIsEmpty(char var);

// File headers cross the wire little-endian; the in-memory struct is
// filled through explicit LE accessors so the core works on big-endian
// hosts too (offsets via offsetof on the packed struct).
static void file_hdr_from_bytes(const uint8_t *raw, JEEFSFileHeaderv1 *hdr) {
    memcpy(hdr->name, raw, sizeof(hdr->name));
    hdr->dataSize = jeefs_get_le16(raw + offsetof(JEEFSFileHeaderv1, dataSize));
    hdr->crc32 = jeefs_get_le32(raw + offsetof(JEEFSFileHeaderv1, crc32));
    hdr->nextFileAddress = jeefs_get_le16(raw + offsetof(JEEFSFileHeaderv1, nextFileAddress));
}

static void file_hdr_to_bytes(const JEEFSFileHeaderv1 *hdr, uint8_t *raw) {
    memcpy(raw, hdr->name, sizeof(hdr->name));
    jeefs_put_le16(raw + offsetof(JEEFSFileHeaderv1, dataSize), hdr->dataSize);
    jeefs_put_le32(raw + offsetof(JEEFSFileHeaderv1, crc32), hdr->crc32);
    jeefs_put_le16(raw + offsetof(JEEFSFileHeaderv1, nextFileAddress), hdr->nextFileAddress);
}

// Validated chain iterator.
typedef struct {
    uint16_t addr; // current file header address (valid after step == 1)
    uint16_t prev; // predecessor header address, 0 = current is first
    uint16_t next_addr; // where the next step will read
    JEEFSFileHeaderv1 hdr;
} JEEFSIter;

static int16_t iter_begin(const uint8_t *image, uint16_t imageSize, JEEFSIter *it) {
    int header_size = header_size_of(image, imageSize);
    if (header_size < 0)
        return EEPROMCORRUPTED;
    it->addr = 0;
    it->prev = 0;
    it->next_addr = (uint16_t) header_size;
    memset(&it->hdr, 0, sizeof(it->hdr));
    return 0;
}

// Advance to the next file. Returns 1 when a validated file header is
// loaded into it->hdr, 0 at the end of the chain, negative EEPROMError.
static int16_t iter_step(const uint8_t *image, uint16_t imageSize, JEEFSIter *it) {
    if (it->next_addr == 0)
        return 0; // previous file was terminal

    uint32_t start = it->next_addr;
    if (start + sizeof(JEEFSFileHeaderv1) > imageSize)
        return 0; // no room for another header: clean end

    JEEFSFileHeaderv1 hdr;
    file_hdr_from_bytes(image + start, &hdr);

    if (EEPROM_ByteIsEmpty(hdr.name[0]))
        return 0; // unwritten slot (0x00 or 0xFF): end of chain

    // A written header must carry a terminated name and a sane size.
    if (hdr.name[JEEFS_FILE_NAME_LENGTH] != '\0')
        return EEPROMCORRUPTED;
    if (hdr.dataSize == 0 || hdr.dataSize == 0xFFFF)
        return EEPROMCORRUPTED;

    uint32_t end = start + sizeof(JEEFSFileHeaderv1) + hdr.dataSize;
    if (end > imageSize)
        return EEPROMCORRUPTED;

    // An erased link (0xFFFF) terminates the chain like 0 — RFC #14: the
    // file may have been written onto erased media without a link update.
    if (hdr.nextFileAddress == 0xFFFF)
        hdr.nextFileAddress = 0;

    // Contiguity: the link either terminates or names exactly the next slot,
    // and a claimed successor must have room for its own header. This also
    // makes cycles impossible — addresses strictly increase.
    if (hdr.nextFileAddress != 0 && (hdr.nextFileAddress != end || end + sizeof(JEEFSFileHeaderv1) > imageSize))
        return EEPROMCORRUPTED;

    it->prev = it->addr;
    it->addr = (uint16_t) start;
    it->next_addr = hdr.nextFileAddress;
    it->hdr = hdr;
    return 1;
}

// Walk the whole chain. Returns 1 if filename was found (iterator left on
// it), 0 if not found, negative EEPROMError. chain_end receives the first
// byte past the last file's data (== first free byte); with no files it is
// the header size. found (optional) receives the match while the walk
// continues to the end, so chain_end is always complete.
static int16_t chain_walk(const uint8_t *image, uint16_t imageSize, const char *filename, JEEFSIter *found,
                          uint32_t *chain_end) {
    JEEFSIter it;
    int16_t ret = iter_begin(image, imageSize, &it);
    if (ret < 0)
        return ret;

    uint32_t end = it.next_addr;
    int16_t hit = 0;
    uint16_t guard = 0;
    const uint16_t max_files = (uint16_t) (imageSize / sizeof(JEEFSFileHeaderv1)) + 1;

    while ((ret = iter_step(image, imageSize, &it)) == 1) {
        if (++guard > max_files)
            return EEPROMCORRUPTED; // defense in depth, unreachable by invariant
        end = (uint32_t) it.addr + sizeof(JEEFSFileHeaderv1) + it.hdr.dataSize;
        if (filename && strncmp(it.hdr.name, filename, JEEFS_FILE_NAME_LENGTH + 1) == 0) {
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
    size_t len = strnlen(filename, JEEFS_FILE_NAME_LENGTH + 1);
    return len > 0 && len <= JEEFS_FILE_NAME_LENGTH;
}

/*
 * Public API
 */

static int header_size_of(const uint8_t *image, uint16_t imageSize) {
    if (!image || imageSize < sizeof(JEEPROMHeaderversion))
        return -1;
    int version = jeefs_header_detect_version(image, imageSize);
    if (version < 0)
        return -1;
    int size = jeefs_header_size(version);
    if (size < 0 || (uint32_t) size > imageSize)
        return -1;
    return size;
}

int EEPROM_GetHeader(const uint8_t *image, uint16_t imageSize, void *header, int size) {
    if (!header || size < (int) sizeof(JEEPROMHeaderversion))
        return BUFFERNOTVALID;

    int header_size = header_size_of(image, imageSize);
    if (header_size < 0)
        return EEPROMCORRUPTED;
    if (size < header_size)
        return BUFFERNOTVALID;

    memcpy(header, image, (size_t) header_size);
    return 0;
}

int EEPROM_SetHeader(uint8_t *image, uint16_t imageSize, void *header) {
    if (!header)
        return BUFFERNOTVALID;
    if (!image)
        return EEPROMCORRUPTED;

    int version = jeefs_header_detect_version((const uint8_t *) header, sizeof(JEEPROMHeaderversion));
    if (version < 0)
        return EEPROMCORRUPTED;
    int size = jeefs_header_size(version);
    if ((uint32_t) size > imageSize)
        return EEPROMCORRUPTED;

    if (jeefs_header_update_crc((uint8_t *) header, (size_t) size) != 0)
        return EEPROMCORRUPTED;

    memcpy(image, header, (size_t) size);
    return 0;
}

int16_t EEPROM_HeaderCheckConsistency(const uint8_t *image, uint16_t imageSize) {
    int header_size = header_size_of(image, imageSize);
    if (header_size < 0)
        return 0; // bad magic/version or short image: inconsistent

    return jeefs_header_verify_crc(image, (size_t) header_size) == 0 ? 1 : 0;
}

int16_t EEPROM_ListFiles(const uint8_t *image, uint16_t imageSize, char fileList[][JEEFS_FILE_NAME_LENGTH + 1],
                         uint16_t maxFiles) {
    if (!fileList)
        return BUFFERNOTVALID;

    JEEFSIter it;
    int16_t ret = iter_begin(image, imageSize, &it);
    if (ret < 0)
        return ret;

    int16_t count = 0;
    while ((ret = iter_step(image, imageSize, &it)) == 1) {
        if ((uint16_t) count >= maxFiles)
            return count; // list full; the rest is still a valid chain
        memcpy(fileList[count], it.hdr.name, JEEFS_FILE_NAME_LENGTH);
        fileList[count][JEEFS_FILE_NAME_LENGTH] = '\0';
        count++;
    }
    return ret < 0 ? ret : count;
}

int16_t EEPROM_ReadFile(const uint8_t *image, uint16_t imageSize, const char *filename, uint8_t *buffer,
                        uint16_t bufferSize) {
    if (!filename_valid(filename))
        return FILENAMENOTVALID;
    if (!buffer || bufferSize == 0)
        return BUFFERNOTVALID;

    JEEFSIter found;
    int16_t ret = chain_walk(image, imageSize, filename, &found, NULL);
    if (ret < 0)
        return ret;
    if (ret == 0)
        return FILENOTFOUND;

    if (found.hdr.dataSize > INT16_MAX)
        return EEPROMCORRUPTED; // count not representable in the return type
    if (found.hdr.dataSize > bufferSize)
        return BUFFERNOTVALID;

    const uint8_t *data = image + found.addr + sizeof(JEEFSFileHeaderv1);
    if (calculateCRC32(data, found.hdr.dataSize) != found.hdr.crc32)
        return EEPROMCORRUPTED;

    memcpy(buffer, data, found.hdr.dataSize);
    return (int16_t) found.hdr.dataSize;
}

int16_t EEPROM_AddFile(uint8_t *image, uint16_t imageSize, const char *filename, const uint8_t *data,
                       uint16_t dataSize) {
    if (!filename_valid(filename))
        return FILENAMENOTVALID;
    if (!data || dataSize == 0 || dataSize > INT16_MAX)
        return BUFFERNOTVALID;

    JEEFSIter last;
    uint32_t chain_end;
    int16_t ret = chain_walk(image, imageSize, filename, &last, &chain_end);
    if (ret < 0)
        return ret;
    if (ret == 1)
        return 0; // file already exists

    if (chain_end + sizeof(JEEFSFileHeaderv1) + dataSize > imageSize)
        return NOTENOUGHSPACE;

    JEEFSFileHeaderv1 hdr;
    memset(&hdr, 0, sizeof(hdr));
    strncpy(hdr.name, filename, JEEFS_FILE_NAME_LENGTH);
    hdr.dataSize = dataSize;
    hdr.crc32 = calculateCRC32(data, dataSize);
    hdr.nextFileAddress = 0;

    uint16_t new_addr = (uint16_t) chain_end;
    file_hdr_to_bytes(&hdr, image + new_addr);
    memcpy(image + new_addr + sizeof(hdr), data, dataSize);

    // Link the predecessor (the last existing file), if any. The first file
    // needs no link: its position is implied by the header size.
    int header_size = header_size_of(image, imageSize);
    if (new_addr != (uint16_t) header_size) {
        JEEFSIter it;
        ret = iter_begin(image, imageSize, &it);
        if (ret < 0)
            return ret;
        JEEFSIter tail = it;
        while ((ret = iter_step(image, imageSize, &it)) == 1)
            if (it.addr != new_addr)
                tail = it;
        if (ret < 0)
            return ret;
        tail.hdr.nextFileAddress = new_addr;
        file_hdr_to_bytes(&tail.hdr, image + tail.addr);
    }

    return (int16_t) dataSize;
}

int16_t EEPROM_DeleteFile(uint8_t *image, uint16_t imageSize, const char *filename) {
    if (!filename_valid(filename))
        return FILENAMENOTVALID;

    JEEFSIter victim;
    uint32_t chain_end;
    int16_t ret = chain_walk(image, imageSize, filename, &victim, &chain_end);
    if (ret < 0)
        return ret;
    if (ret == 0)
        return FILENOTFOUND;

    uint32_t shift = sizeof(JEEFSFileHeaderv1) + victim.hdr.dataSize;
    uint32_t tail_start = victim.addr + shift; // first byte after the victim
    uint32_t tail_len = chain_end - tail_start; // bytes of real files after it

    // The victim is terminal when nothing real follows it — including the
    // legal case of a non-zero link into a valid-but-empty slot (the
    // iterator ends the chain on the empty name). Fuzz finding, PR #75.
    if (victim.hdr.nextFileAddress == 0 || tail_len == 0) {
        // Victim is the last file: terminate the predecessor and wipe.
        if (victim.prev != 0) {
            JEEFSFileHeaderv1 prev_hdr;
            file_hdr_from_bytes(image + victim.prev, &prev_hdr);
            prev_hdr.nextFileAddress = 0;
            file_hdr_to_bytes(&prev_hdr, image + victim.prev);
        }
        memset(image + victim.addr, JEEFS_EMPTYBYTE, shift);
        return 1;
    }

    // Compact: move [tail_start, chain_end) down by `shift`. The successor
    // lands exactly at the victim's address, so the predecessor's link (which
    // already names that address) stays valid without a write.
    memmove(image + victim.addr, image + tail_start, chain_end - tail_start);

    // Rewrite the moved headers' absolute links. Only headers inside the
    // moved region are trusted — they were validated before the move; the
    // last moved file may legally link one past the region, where the
    // freed span is wiped to empty below (fuzz finding, PR #75: following
    // raw links past the region walked stale bytes and wrote wild).
    uint32_t moved_end = victim.addr + tail_len;
    uint16_t addr = victim.addr;
    while (addr != 0 && (uint32_t) addr + sizeof(JEEFSFileHeaderv1) <= moved_end) {
        JEEFSFileHeaderv1 hdr;
        file_hdr_from_bytes(image + addr, &hdr);
        if (hdr.nextFileAddress == 0 || hdr.nextFileAddress == 0xFFFF) {
            break; // terminal link (0 or erased) needs no rewrite
        }
        uint16_t next = (uint16_t) (hdr.nextFileAddress - shift);
        if (next <= addr)
            break; // defense in depth: validated links strictly increase
        hdr.nextFileAddress = next;
        file_hdr_to_bytes(&hdr, image + addr);
        addr = next;
    }

    // Wipe the freed span at the old end of the chain.
    memset(image + (chain_end - shift), JEEFS_EMPTYBYTE, shift);
    return 1;
}

int16_t EEPROM_WriteFile(uint8_t *image, uint16_t imageSize, const char *filename, const uint8_t *data,
                         uint16_t dataSize) {
    if (!filename_valid(filename))
        return FILENAMENOTVALID;
    if (!data || dataSize == 0 || dataSize > INT16_MAX)
        return BUFFERNOTVALID;

    JEEFSIter found;
    uint32_t chain_end;
    int16_t ret = chain_walk(image, imageSize, filename, &found, &chain_end);
    if (ret < 0)
        return ret;
    if (ret == 0)
        return FILENOTFOUND;

    if (found.hdr.dataSize == dataSize) {
        // Same size: overwrite in place, then refresh the stored CRC.
        memcpy(image + found.addr + sizeof(JEEFSFileHeaderv1), data, dataSize);
        found.hdr.crc32 = calculateCRC32(data, dataSize);
        file_hdr_to_bytes(&found.hdr, image + found.addr);
        return (int16_t) dataSize;
    }

    // Different size: ensure the delete + add cannot run out of space BEFORE
    // destroying the old content (#9: WriteFile must not lose the file).
    uint32_t old_span = sizeof(JEEFSFileHeaderv1) + found.hdr.dataSize;
    uint32_t needed = chain_end - old_span + sizeof(JEEFSFileHeaderv1) + dataSize;
    if (needed > imageSize)
        return NOTENOUGHSPACE;

    ret = EEPROM_DeleteFile(image, imageSize, filename);
    if (ret < 0)
        return ret;
    return EEPROM_AddFile(image, imageSize, filename, data, dataSize);
}

// Format the image: write an initialized header, wipe the file area.
int EEPROM_FormatEEPROM(uint8_t *image, uint16_t imageSize, int version) {
    if (!image)
        return BUFFERNOTVALID;
    int header_size = jeefs_header_size(version);
    if (header_size < 0)
        return EEPROMCORRUPTED;
    if ((uint32_t) header_size > imageSize)
        return NOTENOUGHSPACE;

    if (jeefs_header_init(image, imageSize, version) != 0)
        return EEPROMCORRUPTED;

    memset(image + header_size, JEEFS_EMPTYBYTE, imageSize - (uint32_t) header_size);
    return 0;
}

static uint32_t calculateCRC32(const uint8_t *data, size_t length) { return jeefs_crc32(data, length); }

static inline bool EEPROM_ByteIsEmpty(char var) { return var == '\xFF' || var == '\0'; }
