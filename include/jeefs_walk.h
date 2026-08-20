// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 *
 * Pull-model file locator (#81): find one file in the JEEFS chain
 * without buffering the image. The walker owns the state machine and
 * every validation rule of the in-memory iterator (headerCrc32 before
 * any field is trusted, exact contiguity, bounds, erased-link and
 * empty-slot terminals); the environment owns every read:
 *
 *   JEEFSWalk w;
 *   uint8_t hdr[sizeof(JEEFSFileHeaderv1)];
 *   jeefs_walk_begin(&w, prefix, prefix_len, image_size, "wifi.conf");
 *   uint32_t off; uint16_t len;
 *   while (jeefs_walk_want(&w, &off, &len) == 1) {
 *       env_read(off, hdr, len);              // the environment reads
 *       if (jeefs_walk_feed(&w, hdr, len) != 0)
 *           break;                            // terminal state reached
 *   }
 *   // JEEFS_WALK_FOUND: stream the data from w.file_offset and verify
 *   // it incrementally with jeefs_crc32_update() against w.file_crc32.
 *
 * RAM: this struct plus one header window. `prefix` is the board-header
 * prefix the environment has already read (>= 12 bytes; verifying the
 * board-header CRC first — a bounded 256/512-byte read — stays the
 * caller's job, exactly as with jeefs_header_verify_crc()).
 */

#ifndef JEEFS_JEEFS_WALK_H
#define JEEFS_JEEFS_WALK_H

#include <stdint.h>

#include "jeefs_generated.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    JEEFS_WALK_FOUND = 1, // file located: file_offset/file_size/file_crc32 valid
    JEEFS_WALK_NOTFOUND = 2 // clean end of chain without a match
};

typedef struct {
    // request to the environment (valid while jeefs_walk_want() == 1)
    uint32_t want_offset;
    uint16_t want_len; // always sizeof(JEEFSFileHeaderv1)
    // result (valid in state JEEFS_WALK_FOUND)
    uint32_t file_offset; // first data byte of the located file
    uint16_t file_size;
    uint32_t file_crc32; // expected CRC32 of the file data
    // internals
    uint16_t image_size;
    int16_t state;
    char target[JEEFS_FILE_NAME_LENGTH + 1];
} JEEFSWalk;

/*
 * Initialize a walk over an image of image_size bytes. prefix is the
 * board-header prefix already read by the environment (prefix_len >= 12:
 * enough for version detection and the fs_version byte).
 * Return: 0 on success; BUFFERNOTVALID (short prefix), FILENAMENOTVALID,
 * EEPROMCORRUPTED (bad magic/version or header does not fit image_size),
 * FSVERSIONNOTSUPPORTED. An fs_version of 0 succeeds and terminates as
 * JEEFS_WALK_NOTFOUND without requesting any bytes.
 */
int16_t jeefs_walk_begin(JEEFSWalk *w, const uint8_t *prefix, uint16_t prefix_len, uint16_t image_size,
                         const char *filename);

/*
 * Returns 1 while the walker wants bytes — the environment must then
 * read want_len bytes at want_offset and pass them to jeefs_walk_feed().
 * offset/len outputs are optional conveniences. Returns 0 once a
 * terminal state is reached.
 */
int16_t jeefs_walk_want(const JEEFSWalk *w, uint32_t *offset, uint16_t *len);

/*
 * Feed exactly the requested bytes. Returns 0 to continue (the walker
 * wants the next header), JEEFS_WALK_FOUND, JEEFS_WALK_NOTFOUND, or a
 * negative EEPROMError (BUFFERNOTVALID for a wrong length,
 * EEPROMCORRUPTED for a chain that fails validation).
 */
int16_t jeefs_walk_feed(JEEFSWalk *w, const uint8_t *header, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_WALK_H
