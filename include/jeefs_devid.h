// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 *
 * Pure DeviceIdentityV1 record API — byte-buffer operations, no I/O.
 * The record is a self-contained 256-byte blob (docs/format/
 * device-identity-v1.md); the header entry points gate on the JETHOME
 * magic, so the record gets its own parallel module.
 */

#ifndef JEEFS_JEEFS_DEVID_H
#define JEEFS_JEEFS_DEVID_H

#include <stddef.h>
#include <stdint.h>

#include "jeefs_generated.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Detect a DeviceIdentityV1 record in the buffer.
 * Returns the record_version (>= 1) on success, or -1 when the buffer is
 * too short, the magic does not match, the record_version or the
 * signature_version byte is unknown. An all-0x00/0xFF buffer fails the
 * magic check — "no record", not "corrupt".
 */
int jeefs_devid_detect(const uint8_t *data, size_t len);

/*
 * Verify the record CRC32 (bytes 0-251, IEEE 802.3).
 * Returns 0 when valid, -1 on detect failure, short buffer or mismatch.
 */
int jeefs_devid_verify_crc(const uint8_t *data, size_t len);

/*
 * Recalculate and store the record CRC32.
 * Returns 0 on success, -1 on detect failure or short buffer.
 */
int jeefs_devid_update_crc(uint8_t *data, size_t len);

/*
 * Initialize an empty record: zero fill, magic, record_version = 1,
 * signature_version = NONE, CRC. Returns 0 on success, -1 when the
 * buffer is shorter than the record.
 */
int jeefs_devid_init(uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_DEVID_H
