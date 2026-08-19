// GENERATED FILE — DO NOT EDIT BY HAND.
// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
//
// Source of truth: docs/format/*.md (see docs/CODEGEN.md for the policy).
// Manual edits are rejected: the codegen-check CI job and the local prek
// hook diff this file against the specs on every change.
//
// Regenerate with:
//   python -m jeefs_codegen --specs docs/format/*.md --c-output include/jeefs_generated.h

#ifndef JEEFS_GENERATED_H
#define JEEFS_GENERATED_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- Named constants ---

#define JEEFS_BOARDNAME_LENGTH 31
#define JEEFS_BOARDVERSION_LENGTH 31
#define JEEFS_CPUID_LENGTH 32
#define JEEFS_DEVICE_ID_FILENAME "device.id"
#define JEEFS_DEVID_MAGIC "JHDEVID"
#define JEEFS_EMPTYBYTE 0x00
#define JEEFS_ERASEDBYTE 0xFF
#define JEEFS_FILE_NAME_LENGTH 15
#define JEEFS_FS_VERSION 1
#define JEEFS_FS_VERSION_OFFSET 10
#define JEEFS_HEADER_VERSION 4
#define JEEFS_MAC_LENGTH 6
#define JEEFS_MAGIC "JETHOME"
#define JEEFS_MAGIC_LENGTH 8
#define JEEFS_PARTITION_SIZE 4096
#define JEEFS_SERIAL_LENGTH 32
#define JEEFS_SIGNATURE_FIELD_SIZE 64
#define JEEFS_USID_LENGTH 32

// JEEFSSignatureAlgorithm
enum JEEFSSignatureAlgorithm {
    JEEFS_SIG_NONE = 0,  // No signature
    JEEFS_SIG_SECP192R1 = 1,  // ECDSA secp192r1/NIST P-192, r‖s
    JEEFS_SIG_SECP256R1 = 2,  // ECDSA secp256r1/NIST P-256, r‖s
};

#pragma pack(push, 1)

// JEEPROMHeaderversion (12 bytes)
typedef struct {
    char magic[8];  // 8B, offset 0, Magic string "JETHOME\0"
    uint8_t version;  // 1B, offset 8, Header version number
    uint8_t reserved1[3];  // 3B, offset 9, Not used by detection
} JEEPROMHeaderversion;

// JEEPROMHeaderv1 (512 bytes)
typedef struct {
    char magic[8];  // 8B, offset 0, "JETHOME\0"
    uint8_t version;  // 1B, offset 8, Header version = 1
    uint8_t reserved1[3];  // 3B, offset 9, Reserved (zeros)
    char boardname[32];  // 32B, offset 12, Board name, null-terminated
    char boardversion[32];  // 32B, offset 44, Board version, null-term.
    uint8_t serial[32];  // 32B, offset 76, Board serial (bounded str.)
    uint8_t usid[32];  // 32B, offset 108, CPU eFuse USID (bounded)
    uint8_t cpuid[32];  // 32B, offset 140, CPU ID (bounded string)
    uint8_t mac[6];  // 6B, offset 172, MAC address (6 raw bytes)
    uint8_t reserved2[2];  // 2B, offset 178, Reserved for extended MAC
    uint16_t modules[16];  // 32B, offset 180, 16 module IDs
    uint8_t reserved3[296];  // 296B, offset 212, Reserved for future use
    uint32_t crc32;  // 4B, offset 508, CRC32 of bytes 0-507
} JEEPROMHeaderv1;

// JEEPROMHeaderv2 (256 bytes)
typedef struct {
    char magic[8];  // 8B, offset 0, "JETHOME\0"
    uint8_t version;  // 1B, offset 8, Header version = 2
    uint8_t reserved1[3];  // 3B, offset 9, Reserved (zeros)
    char boardname[32];  // 32B, offset 12, Board name, null-terminated
    char boardversion[32];  // 32B, offset 44, Board version, null-term.
    uint8_t serial[32];  // 32B, offset 76, Board serial (bounded str.)
    uint8_t usid[32];  // 32B, offset 108, CPU eFuse USID (bounded)
    uint8_t cpuid[32];  // 32B, offset 140, CPU ID (bounded string)
    uint8_t mac[6];  // 6B, offset 172, MAC address (6 raw bytes)
    uint8_t reserved2[2];  // 2B, offset 178, Reserved for extended MAC
    uint8_t reserved3[72];  // 72B, offset 180, Reserved for future use
    uint32_t crc32;  // 4B, offset 252, CRC32 of bytes 0-251
} JEEPROMHeaderv2;

// JEEPROMHeaderv3 (256 bytes)
typedef struct {
    char magic[8];  // 8B, offset 0, "JETHOME\0" (null-terminated string)
    uint8_t version;  // 1B, offset 8, Header version = 3
    uint8_t signature_version;  // 1B, offset 9, Signature algorithm (see enums)
    uint8_t fs_version;  // 1B, offset 10, Filesystem version: 0 = none, 1 = current
    uint8_t header_reserved;  // 1B, offset 11, Reserved (zeros)
    char boardname[32];  // 32B, offset 12, Board name, null-terminated
    char boardversion[32];  // 32B, offset 44, Board version, null-terminated
    uint8_t serial[32];  // 32B, offset 76, Board serial number (bounded string)
    uint8_t usid[32];  // 32B, offset 108, CPU eFuse USID (bounded string)
    uint8_t cpuid[32];  // 32B, offset 140, CPU ID / factory MAC (bounded string)
    uint8_t mac[6];  // 6B, offset 172, MAC address (6 raw bytes)
    uint8_t reserved2[2];  // 2B, offset 178, Reserved for extended MAC
    uint8_t signature[64];  // 64B, offset 180, ECDSA signature (r‖s, zero-padded)
    int64_t timestamp;  // 8B, offset 244, Unix timestamp (seconds)
    uint32_t crc32;  // 4B, offset 252, CRC32 of bytes 0-251
} JEEPROMHeaderv3;

// JEEPROMHeaderv4 (256 bytes)
typedef struct {
    char magic[8];  // 8B, offset 0, "JETHOME\0" (null-terminated string)
    uint8_t version;  // 1B, offset 8, Header version = 4
    uint8_t signature_version;  // 1B, offset 9, Signature algorithm (see enums)
    uint8_t fs_version;  // 1B, offset 10, Filesystem version: 0 = none, 1 = current
    uint8_t header_reserved;  // 1B, offset 11, Reserved (zeros)
    char boardname[32];  // 32B, offset 12, Board name, null-terminated
    char boardversion[32];  // 32B, offset 44, Board version, null-terminated
    uint8_t board_serial[32];  // 32B, offset 76, Board serial number (bounded string)
    uint8_t usid[32];  // 32B, offset 108, Board CPU/eFuse USID, if available
    uint8_t cpuid[32];  // 32B, offset 140, Board CPU ID, if available
    uint8_t mac[6];  // 6B, offset 172, MAC address (6 raw bytes)
    uint8_t reserved2[2];  // 2B, offset 178, Reserved (alignment)
    uint8_t signature[64];  // 64B, offset 180, ECDSA signature (r‖s, zero-padded)
    int64_t timestamp;  // 8B, offset 244, Header creation/signing time (Unix s)
    uint32_t crc32;  // 4B, offset 252, CRC32 of bytes 0-251
} JEEPROMHeaderv4;

// DeviceIdentityV1 (256 bytes)
typedef struct {
    char magic[8];  // 8B, offset 0, "JHDEVID\0" (null-terminated string)
    uint8_t record_version;  // 1B, offset 8, Record version = 1
    uint8_t signature_version;  // 1B, offset 9, Signature algorithm (same enum as header)
    uint8_t reserved1[2];  // 2B, offset 10, Reserved (zeros)
    char device_model[32];  // 32B, offset 12, Device model name (bounded string)
    char device_serial[32];  // 32B, offset 44, Device serial number (bounded string)
    char hw_revision[16];  // 16B, offset 76, Device hardware revision (bounded string)
    uint16_t flags;  // 2B, offset 92, Flags: all bits reserved (see below)
    uint8_t reserved2[86];  // 86B, offset 94, Reserved for future use (zeros)
    uint8_t signature[64];  // 64B, offset 180, ECDSA signature (r‖s, zero-padded)
    int64_t timestamp;  // 8B, offset 244, Record creation/signing time (Unix s)
    uint32_t crc32;  // 4B, offset 252, CRC32 of bytes 0-251
} DeviceIdentityV1;

// JEEFSFileHeaderv1 (28 bytes)
typedef struct {
    char name[16];  // 16B, offset 0, Filename, null-terminated (max 15 ch.)
    uint16_t dataSize;  // 2B, offset 16, File data size in bytes
    uint32_t crc32;  // 4B, offset 18, CRC32 of file data only (not header)
    uint16_t nextFileAddress;  // 2B, offset 22, Absolute offset of next file, 0 = end
    uint32_t headerCrc32;  // 4B, offset 24, CRC32 of header bytes 0-23
} JEEFSFileHeaderv1;

// JEEPROMHeader
union JEEPROMHeader {
    JEEPROMHeaderversion version;  // Version detection (12B)
    JEEPROMHeaderv1 v1;  // Full v1 header (512B)
    JEEPROMHeaderv2 v2;  // Full v2 header (256B)
    JEEPROMHeaderv3 v3;  // Full v3 header (256B)
    JEEPROMHeaderv4 v4;  // Full v4 header (256B)
};

#pragma pack(pop)

// --- Size assertions ---
#ifdef __cplusplus
#define JEEFS_STATIC_ASSERT static_assert
#else
#define JEEFS_STATIC_ASSERT _Static_assert
#endif
JEEFS_STATIC_ASSERT(sizeof(JEEPROMHeaderversion) == 12, "sizeof(JEEPROMHeaderversion) must be 12");
JEEFS_STATIC_ASSERT(sizeof(JEEPROMHeaderv1) == 512, "sizeof(JEEPROMHeaderv1) must be 512");
JEEFS_STATIC_ASSERT(sizeof(JEEPROMHeaderv2) == 256, "sizeof(JEEPROMHeaderv2) must be 256");
JEEFS_STATIC_ASSERT(sizeof(JEEPROMHeaderv3) == 256, "sizeof(JEEPROMHeaderv3) must be 256");
JEEFS_STATIC_ASSERT(sizeof(JEEPROMHeaderv4) == 256, "sizeof(JEEPROMHeaderv4) must be 256");
JEEFS_STATIC_ASSERT(sizeof(DeviceIdentityV1) == 256, "sizeof(DeviceIdentityV1) must be 256");
JEEFS_STATIC_ASSERT(sizeof(JEEFSFileHeaderv1) == 28, "sizeof(JEEFSFileHeaderv1) must be 28");
#undef JEEFS_STATIC_ASSERT

#ifdef __cplusplus
}
#endif

#endif // JEEFS_GENERATED_H
