// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 *
 * Pure DeviceIdentityV1 record operations — no I/O dependency.
 */

#include "jeefs_devid.h"

#include <string.h>
#include <zlib.h>

#include "jeefs_endian.h"

#define DEVID_SIZE sizeof(DeviceIdentityV1)
#define DEVID_CRC_OFFSET (DEVID_SIZE - sizeof(uint32_t))
#define DEVID_RECORD_VERSION 1

int jeefs_devid_detect(const uint8_t *data, size_t len) {
    if (!data || len < DEVID_SIZE)
        return -1;

    if (memcmp(data, JEEFS_DEVID_MAGIC, JEEFS_MAGIC_LENGTH) != 0)
        return -1;

    // Unknown record_version or signature_version is a parse error —
    // no forward-compatibility guessing (spec).
    uint8_t record_version = data[8];
    if (record_version != DEVID_RECORD_VERSION)
        return -1;
    uint8_t signature_version = data[9];
    if (signature_version > JEEFS_SIG_SECP256R1)
        return -1;

    return record_version;
}

int jeefs_devid_verify_crc(const uint8_t *data, size_t len) {
    if (jeefs_devid_detect(data, len) < 0)
        return -1;

    uint32_t stored = jeefs_get_le32(data + DEVID_CRC_OFFSET);
    uint32_t calc = (uint32_t) crc32(0L, data, DEVID_CRC_OFFSET);
    return stored == calc ? 0 : -1;
}

int jeefs_devid_update_crc(uint8_t *data, size_t len) {
    if (jeefs_devid_detect(data, len) < 0)
        return -1;

    uint32_t calc = (uint32_t) crc32(0L, data, DEVID_CRC_OFFSET);
    jeefs_put_le32(data + DEVID_CRC_OFFSET, calc);
    return 0;
}

int jeefs_devid_init(uint8_t *data, size_t len) {
    if (!data || len < DEVID_SIZE)
        return -1;

    memset(data, 0, DEVID_SIZE);
    memcpy(data, JEEFS_DEVID_MAGIC, JEEFS_MAGIC_LENGTH);
    data[8] = DEVID_RECORD_VERSION;
    // signature_version = NONE, reserved/flags = zeros (writers MUST zero)
    return jeefs_devid_update_crc(data, len);
}
