// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * Little-endian field accessors for the wire format. Freestanding
 * (stdint only). Will fold into the port layer (#24) together with the
 * CRC32 selection.
 */

#ifndef JEEFS_JEEFS_ENDIAN_H
#define JEEFS_JEEFS_ENDIAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint16_t jeefs_get_le16(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t jeefs_get_le32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static inline void jeefs_put_le16(uint8_t *p, uint16_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)(v >> 8);
}

static inline void jeefs_put_le32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_ENDIAN_H
