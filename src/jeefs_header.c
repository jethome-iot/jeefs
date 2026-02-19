// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 *
 * Pure header parsing — no eepromops dependency.
 */

#include "jeefs_header.h"

#include <string.h>
#include <zlib.h>

static uint32_t header_crc32(const uint8_t *data, size_t length) {
    return crc32(0L, data, length);
}

int jeefs_header_size(int version) {
    switch (version) {
    case 1:
        return (int)sizeof(JEEPROMHeaderv1);
    case 2:
        return (int)sizeof(JEEPROMHeaderv2);
    case 3:
        return (int)sizeof(JEEPROMHeaderv3);
    default:
        return -1;
    }
}

int jeefs_header_detect_version(const uint8_t *data, size_t len) {
    if (!data || len < sizeof(JEEPROMHeaderversion))
        return -1;

    const JEEPROMHeaderversion *hdr = (const JEEPROMHeaderversion *)data;
    if (strncmp(hdr->magic, MAGIC, MAGIC_LENGTH) != 0)
        return -1;

    int ver = hdr->version;
    if (jeefs_header_size(ver) < 0)
        return -1;

    return ver;
}

int jeefs_header_verify_crc(const uint8_t *data, size_t len) {
    int ver = jeefs_header_detect_version(data, len);
    if (ver < 0)
        return -1;

    int hdr_size = jeefs_header_size(ver);
    if ((int)len < hdr_size)
        return -1;

    /* CRC32 is always the last 4 bytes of the header */
    size_t crc_offset = (size_t)hdr_size - sizeof(uint32_t);
    uint32_t stored_crc;
    memcpy(&stored_crc, data + crc_offset, sizeof(uint32_t));

    uint32_t calc_crc = header_crc32(data, crc_offset);
    if (stored_crc == 0 || calc_crc != stored_crc)
        return -1;

    return 0;
}

int jeefs_header_update_crc(uint8_t *data, size_t len) {
    int ver = jeefs_header_detect_version(data, len);
    if (ver < 0)
        return -1;

    int hdr_size = jeefs_header_size(ver);
    if ((int)len < hdr_size)
        return -1;

    size_t crc_offset = (size_t)hdr_size - sizeof(uint32_t);
    uint32_t calc_crc = header_crc32(data, crc_offset);
    memcpy(data + crc_offset, &calc_crc, sizeof(uint32_t));

    return 0;
}

int jeefs_header_init(uint8_t *data, size_t len, int version) {
    int hdr_size = jeefs_header_size(version);
    if (hdr_size < 0 || (int)len < hdr_size)
        return -1;

    memset(data, 0, (size_t)hdr_size);

    /* Set magic and version */
    memcpy(data, MAGIC, MAGIC_LENGTH);
    data[8] = (uint8_t)version;

    /* v3-specific defaults */
    if (version == 3) {
        /* signature_version = NONE, timestamp = 0, signature = zeros — already zeroed */
    }

    /* Compute and write CRC */
    return jeefs_header_update_crc(data, len);
}
