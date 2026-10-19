// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
#include "read_identity.h"

#include <string.h>
#include "jeefs_devid.h"
#include "jeefs_header.h"
#include "jeefs_port.h"
#include "jeefs_walk.h"

int jeefs_example_read_identity(jeefs_example_read_fn read, void *context, uint16_t image_size,
                                struct jeefs_example_identity *result) {
    if (!result)
        return JEEFS_EXAMPLE_ARGUMENT;
    memset(result, 0, sizeof(*result));
    if (!read || image_size < sizeof(JEEPROMHeaderversion))
        return JEEFS_EXAMPLE_ARGUMENT;

    uint8_t *board = (uint8_t *) &result->board;
    const uint16_t probe_size = sizeof(JEEPROMHeaderversion);
    if (read(context, 0, board, probe_size) != 0)
        return JEEFS_EXAMPLE_IO;
    int version = jeefs_header_detect_version(board, probe_size);
    int board_size = jeefs_header_size(version);
    if (board_size < 0 || board_size > image_size)
        return JEEFS_EXAMPLE_BOARD;
    if (read(context, probe_size, board + probe_size, (uint16_t) (board_size - probe_size)) != 0)
        return JEEFS_EXAMPLE_IO;
    if (jeefs_header_verify_crc(board, (size_t) board_size) != 0)
        return JEEFS_EXAMPLE_BOARD;
    result->board_size = (uint16_t) board_size;

    JEEFSWalk walk;
    if (jeefs_walk_begin(&walk, board, result->board_size, image_size, JEEFS_DEVICE_ID_FILENAME) != 0)
        return JEEFS_EXAMPLE_FS;
    uint32_t offset;
    uint16_t len;
    if (jeefs_walk_want(&walk, &offset, &len) != 1)
        return JEEFS_EXAMPLE_NO_DEVICE;
    uint8_t file_header[sizeof(JEEFSFileHeaderv1)];
    if (read(context, offset, file_header, len) != 0)
        return JEEFS_EXAMPLE_IO;
    int status = jeefs_walk_feed(&walk, file_header, len);
    if (status < 0)
        return JEEFS_EXAMPLE_FS;
    if (status != JEEFS_WALK_FOUND)
        return JEEFS_EXAMPLE_NO_DEVICE; /* Deliberately never follow a second header. */
    if (walk.file_size != sizeof(result->device))
        return JEEFS_EXAMPLE_RECORD;

    uint8_t *record = (uint8_t *) &result->device;
    uint32_t crc = 0;
    for (uint16_t done = 0; done < walk.file_size;) {
        uint16_t chunk = (uint16_t) (walk.file_size - done);
        if (chunk > 64)
            chunk = 64;
        if (read(context, walk.file_offset + done, record + done, chunk) != 0)
            return JEEFS_EXAMPLE_IO;
        crc = jeefs_crc32_update(crc, record + done, chunk);
        done = (uint16_t) (done + chunk);
    }
    if (crc != walk.file_crc32)
        return JEEFS_EXAMPLE_PAYLOAD;
    if (jeefs_devid_verify_crc(record, sizeof(result->device)) != 0)
        return JEEFS_EXAMPLE_RECORD;
    result->has_device = true;
    return JEEFS_EXAMPLE_OK;
}
