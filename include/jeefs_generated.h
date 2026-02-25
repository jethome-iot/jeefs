// DO NOT EDIT — auto-generated from docs/format/*.md by tools/jeefs_codegen
// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
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

#define MAGIC "JETHOME"
#define MAGIC_LENGTH 8
#define HEADER_VERSION 3
#define SIGNATURE_FIELD_SIZE 64
#define FILE_NAME_LENGTH 15
#define MAC_LENGTH 6
#define SERIAL_LENGTH 32
#define USID_LENGTH 32
#define CPUID_LENGTH 32
#define BOARDNAME_LENGTH 31
#define BOARDVERSION_LENGTH 31
#define EEPROM_EMPTYBYTE '\x00'
#define EEPROM_PARTITION_SIZE 4096

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
    uint8_t reserved1[3];  // 3B, offset 9, Alignment / reserved
} JEEPROMHeaderversion;

// JEEPROMHeaderv1 (512 bytes)
typedef struct {
    char magic[8];  // 8B, offset 0, "JETHOME\0"
    uint8_t version;  // 1B, offset 8, Header version = 1
    uint8_t reserved1[3];  // 3B, offset 9, Reserved (zeros)
    char boardname[32];  // 32B, offset 12, Board name, null-terminated
    char boardversion[32];  // 32B, offset 44, Board version, null-term.
    uint8_t serial[32];  // 32B, offset 76, Device serial number
    uint8_t usid[32];  // 32B, offset 108, CPU eFuse USID
    uint8_t cpuid[32];  // 32B, offset 140, CPU ID
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
    uint8_t serial[32];  // 32B, offset 76, Device serial number
    uint8_t usid[32];  // 32B, offset 108, CPU eFuse USID
    uint8_t cpuid[32];  // 32B, offset 140, CPU ID
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
    uint8_t header_reserved[2];  // 2B, offset 10, Reserved (zeros)
    char boardname[32];  // 32B, offset 12, Board name, null-terminated
    char boardversion[32];  // 32B, offset 44, Board version, null-terminated
    uint8_t serial[32];  // 32B, offset 76, Device serial number
    uint8_t usid[32];  // 32B, offset 108, CPU eFuse USID
    uint8_t cpuid[32];  // 32B, offset 140, CPU ID / factory MAC
    uint8_t mac[6];  // 6B, offset 172, MAC address (6 raw bytes)
    uint8_t reserved2[2];  // 2B, offset 178, Reserved for extended MAC
    uint8_t signature[64];  // 64B, offset 180, ECDSA signature (r‖s, zero-padded)
    int64_t timestamp;  // 8B, offset 244, Unix timestamp (seconds)
    uint32_t crc32;  // 4B, offset 252, CRC32 of bytes 0-251
} JEEPROMHeaderv3;

// JEEFSFileHeaderv1 (24 bytes)
typedef struct {
    char name[16];  // 16B, offset 0, Filename, null-terminated (max 15 ch.)
    uint16_t dataSize;  // 2B, offset 16, File data size in bytes
    uint32_t crc32;  // 4B, offset 18, CRC32 of file data only (not header)
    uint16_t nextFileAddress;  // 2B, offset 22, Absolute offset of next file, 0 = end
} JEEFSFileHeaderv1;

// JEEPROMHeader
union JEEPROMHeader {
    JEEPROMHeaderversion version;  // Version detection (12B)
    JEEPROMHeaderv1 v1;  // Full v1 header (512B)
    JEEPROMHeaderv2 v2;  // Full v2 header (256B)
    JEEPROMHeaderv3 v3;  // Full v3 header (256B)
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
JEEFS_STATIC_ASSERT(sizeof(JEEFSFileHeaderv1) == 24, "sizeof(JEEFSFileHeaderv1) must be 24");
#undef JEEFS_STATIC_ASSERT

#ifdef __cplusplus
}
#endif

#endif // JEEFS_GENERATED_H
