// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 *
 * Pull-model file locator (#81). Pure: no I/O, no heap, state machine
 * over caller-fed 28-byte header windows. Validation mirrors the
 * in-memory iterator in src/jeefs.c step by step — the two must reach
 * the same terminal state on the same image (locked by test_09 and the
 * fuzz harness).
 */

#include <stddef.h>
#include <string.h>

#include "jeefs_walk.h"

#include "eepromerr.h"
#include "jeefs_endian.h"
#include "jeefs_header.h"
#include "jeefs_port.h"

// Internal state values (public terminals live in jeefs_walk.h).
#define WALK_WANTING 0

static int filename_ok(const char *filename) {
    if (!filename)
        return 0;
    size_t len = strnlen(filename, JEEFS_FILE_NAME_LENGTH + 1);
    return len > 0 && len <= JEEFS_FILE_NAME_LENGTH;
}

int16_t jeefs_walk_begin(JEEFSWalk *w, const uint8_t *prefix, uint16_t prefix_len, uint16_t image_size,
                         const char *filename) {
    if (!w)
        return BUFFERNOTVALID;
    memset(w, 0, sizeof(*w));
    if (!prefix || prefix_len < sizeof(JEEPROMHeaderversion))
        return w->state = BUFFERNOTVALID;
    if (!filename_ok(filename))
        return w->state = FILENAMENOTVALID;

    int version = jeefs_header_detect_version(prefix, prefix_len);
    if (version < 0)
        return w->state = EEPROMCORRUPTED;
    int header_size = jeefs_header_size(version);
    if (header_size < 0 || (uint32_t) header_size > image_size)
        return w->state = EEPROMCORRUPTED;

    uint8_t fs_version = prefix[JEEFS_FS_VERSION_OFFSET];
    if (fs_version != 0 && fs_version != JEEFS_FS_VERSION)
        return w->state = FSVERSIONNOTSUPPORTED;

    strncpy(w->target, filename, JEEFS_FILE_NAME_LENGTH);
    w->image_size = image_size;
    if (fs_version == 0 || (uint32_t) header_size + sizeof(JEEFSFileHeaderv1) > image_size) {
        w->state = JEEFS_WALK_NOTFOUND; // no filesystem / no room for a header
        return 0;
    }
    w->want_offset = (uint32_t) header_size;
    w->want_len = (uint16_t) sizeof(JEEFSFileHeaderv1);
    w->state = WALK_WANTING;
    return 0;
}

int16_t jeefs_walk_want(const JEEFSWalk *w, uint32_t *offset, uint16_t *len) {
    if (!w || w->state != WALK_WANTING)
        return 0;
    if (offset)
        *offset = w->want_offset;
    if (len)
        *len = w->want_len;
    return 1;
}

int16_t jeefs_walk_feed(JEEFSWalk *w, const uint8_t *header, uint16_t len) {
    if (!w)
        return BUFFERNOTVALID;
    if (w->state != WALK_WANTING)
        return w->state; // already terminal: idempotent
    if (!header || len != sizeof(JEEFSFileHeaderv1))
        return w->state = BUFFERNOTVALID;

    // Same rule set and order as iter_step() in src/jeefs.c.
    if (header[0] == JEEFS_EMPTYBYTE || header[0] == JEEFS_ERASEDBYTE)
        return w->state = JEEFS_WALK_NOTFOUND; // unwritten slot: end of chain

    uint32_t stored_hcrc = jeefs_get_le32(header + offsetof(JEEFSFileHeaderv1, headerCrc32));
    if (jeefs_crc32(header, offsetof(JEEFSFileHeaderv1, headerCrc32)) != stored_hcrc)
        return w->state = EEPROMCORRUPTED;

    if (header[JEEFS_FILE_NAME_LENGTH] != '\0')
        return w->state = EEPROMCORRUPTED;
    uint16_t data_size = jeefs_get_le16(header + offsetof(JEEFSFileHeaderv1, dataSize));
    if (data_size == 0 || data_size == 0xFFFF)
        return w->state = EEPROMCORRUPTED;

    uint32_t end = w->want_offset + sizeof(JEEFSFileHeaderv1) + data_size;
    if (end > w->image_size)
        return w->state = EEPROMCORRUPTED;

    uint16_t next = jeefs_get_le16(header + offsetof(JEEFSFileHeaderv1, nextFileAddress));
    if (next == 0xFFFF)
        next = 0; // erased link terminates like 0 (RFC #14)
    if (next != 0 && (next != end || end + sizeof(JEEFSFileHeaderv1) > w->image_size))
        return w->state = EEPROMCORRUPTED;

    if (strncmp((const char *) header, w->target, JEEFS_FILE_NAME_LENGTH + 1) == 0) {
        w->file_offset = w->want_offset + (uint32_t) sizeof(JEEFSFileHeaderv1);
        w->file_size = data_size;
        w->file_crc32 = jeefs_get_le32(header + offsetof(JEEFSFileHeaderv1, crc32));
        return w->state = JEEFS_WALK_FOUND;
    }

    if (next == 0)
        return w->state = JEEFS_WALK_NOTFOUND;
    w->want_offset = next;
    return WALK_WANTING;
}
