// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
#ifndef JEEFS_EXAMPLE_READ_IDENTITY_H
#define JEEFS_EXAMPLE_READ_IDENTITY_H

#include <stdbool.h>
#include <stdint.h>
#include "jeefs_generated.h"

/* This is an example environment adapter, not a JEEFS library I/O API.
 * Return zero only after filling exactly len bytes, negative on I/O failure. */
typedef int (*jeefs_example_read_fn)(void *context, uint32_t offset, uint8_t *data, uint16_t len);

enum jeefs_example_status {
    JEEFS_EXAMPLE_OK = 0,
    JEEFS_EXAMPLE_NO_DEVICE = 1, /* No device.id in the first slot; no full-chain search. */
    JEEFS_EXAMPLE_ARGUMENT = -1,
    JEEFS_EXAMPLE_IO = -2,
    JEEFS_EXAMPLE_BOARD = -3,
    JEEFS_EXAMPLE_FS = -4,
    JEEFS_EXAMPLE_PAYLOAD = -5,
    JEEFS_EXAMPLE_RECORD = -6
};

struct jeefs_example_identity {
    union JEEPROMHeader board;
    DeviceIdentityV1 device;
    uint16_t board_size;
    bool has_device;
};

/* Read/verify the board and first-slot device.id without buffering an image.
 * At most 540 bytes read for v2-v4, 796 for v1. The caller owns all I/O.
 * On NO_DEVICE only board/board_size are valid. On failure do not consume
 * the result. has_device is true only after outer and inner CRC checks.
 * This checks integrity, not cryptographic authenticity. */
int jeefs_example_read_identity(jeefs_example_read_fn read, void *context, uint16_t image_size,
                                struct jeefs_example_identity *result);
#endif
