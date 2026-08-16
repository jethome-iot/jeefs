// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 *
 * Port layer (#24): the one place where the environment plugs in.
 *
 * CRC32 provider — IEEE 802.3 polynomial, zlib semantics
 * (jeefs_crc32(buf, len) == zlib crc32(0, buf, len)); select one:
 *
 *   (default)           built-in table, no external dependency —
 *                       the fallback for MCU and U-Boot SPL builds
 *   JEEFS_CRC32_ZLIB    hosted builds that already link zlib
 *   JEEFS_CRC32_UBOOT   U-Boot proper: lib/crc32.c is derived from zlib
 *                       and is a verified drop-in (CONFIG_CRC32=y always;
 *                       SPL_CRC32 is conditional — use the default there)
 *   JEEFS_CRC32_KERNEL  Linux kernel: crc32_le MUST be wrapped as
 *                       crc32_le(~0, buf, len) ^ ~0 — the plain call
 *                       computes without the standard inversions and
 *                       yields different values
 */

#ifndef JEEFS_JEEFS_PORT_H
#define JEEFS_JEEFS_PORT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The providers are mutually exclusive — a misconfigured port must fail
 * loudly, not silently pick whichever branch comes first. */
#if (defined(JEEFS_CRC32_ZLIB) + defined(JEEFS_CRC32_UBOOT) + defined(JEEFS_CRC32_KERNEL)) > 1
#error "Define at most one JEEFS_CRC32_* provider"
#endif

#if defined(JEEFS_CRC32_ZLIB)

#include <zlib.h>
static inline uint32_t jeefs_crc32(const uint8_t *buf, size_t len) { return (uint32_t) crc32(0L, buf, len); }

#elif defined(JEEFS_CRC32_UBOOT)

#include <u-boot/crc.h>
static inline uint32_t jeefs_crc32(const uint8_t *buf, size_t len) { return crc32(0, buf, (uint) len); }

#elif defined(JEEFS_CRC32_KERNEL)

#include <linux/crc32.h>
static inline uint32_t jeefs_crc32(const uint8_t *buf, size_t len) { return crc32_le(~0u, buf, len) ^ ~0u; }

#else /* built-in table (src/jeefs_crc32.c) */

uint32_t jeefs_crc32(const uint8_t *buf, size_t len);

#endif

#ifdef __cplusplus
}
#endif

#endif // JEEFS_JEEFS_PORT_H
