// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 *
 * Pure header parsing API — operates on byte buffers, no I/O dependency.
 * This header does NOT depend on eepromops.h.
 */

#ifndef JEEFS_JEEFS_HEADER_H
#define JEEFS_JEEFS_HEADER_H

#include <stddef.h>
#include <stdint.h>

#include "jeefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Detect header version from raw bytes.
 * Needs at least 12 bytes (sizeof(JEEPROMHeaderversion)).
 *
 * @param data  Raw EEPROM data.
 * @param len   Length of data buffer.
 * @return Header version (1, 2, or 3), or -1 on error (bad magic, too short).
 */
int jeefs_header_detect_version(const uint8_t *data, size_t len);

/**
 * Get expected header size for a given version number.
 *
 * @param version  Header version (1, 2, or 3).
 * @return Header size in bytes, or -1 for unknown version.
 */
int jeefs_header_size(int version);

/**
 * Verify CRC32 of a raw header buffer.
 * Automatically detects version and CRC offset.
 *
 * @param data  Raw EEPROM header data.
 * @param len   Length of data buffer (must be >= header size for detected version).
 * @return 0 if CRC is valid, -1 on error (bad magic, bad CRC, too short).
 */
int jeefs_header_verify_crc(const uint8_t *data, size_t len);

/**
 * Calculate and write CRC32 into a raw header buffer.
 * Detects version, computes CRC over appropriate range, writes it in place.
 *
 * @param data  Raw EEPROM header data (modified in place).
 * @param len   Length of data buffer.
 * @return 0 on success, -1 on error.
 */
int jeefs_header_update_crc(uint8_t *data, size_t len);

/**
 * Initialize a raw header buffer with default values for a given version.
 * Sets magic, version, zeros all fields, computes CRC.
 *
 * @param data     Output buffer (must be at least jeefs_header_size(version) bytes).
 * @param len      Length of output buffer.
 * @param version  Header version (1, 2, or 3).
 * @return 0 on success, -1 on error (unknown version, buffer too small).
 */
int jeefs_header_init(uint8_t *data, size_t len, int version);

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_HEADER_H
