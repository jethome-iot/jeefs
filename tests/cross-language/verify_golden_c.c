// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Golden reference EEPROM binary verification (C).
 *
 * Reads the 8192-byte reference image, verifies:
 * 1. Header v3 fields match expected values
 * 2. CRC32 is valid
 * 3. File system linked list contains 3 files: "config", "wifi.conf", "serial"
 * 4. File data CRC32 matches
 *
 * Usage: verify_golden_c <eeprom_full.bin>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "jeefs_generated.h"
#include "jeefs_header.h"

#define EEPROM_SIZE 8192

static int failures = 0;

static void check_str(const char *name, const char *actual, const char *expected) {
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "  FAIL: %s = \"%s\" (expected \"%s\")\n", name, actual, expected);
        failures++;
    } else {
        printf("  OK: %s = \"%s\"\n", name, actual);
    }
}

static void check_int(const char *name, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "  FAIL: %s = %d (expected %d)\n", name, actual, expected);
        failures++;
    } else {
        printf("  OK: %s = %d\n", name, actual);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <eeprom_full.bin>\n", argv[0]);
        return 2;
    }

    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror(argv[1]);
        return 2;
    }
    uint8_t eeprom[EEPROM_SIZE];
    size_t read_size = fread(eeprom, 1, EEPROM_SIZE, f);
    fclose(f);

    if (read_size != EEPROM_SIZE) {
        fprintf(stderr, "FAIL: file size = %zu (expected %d)\n", read_size, EEPROM_SIZE);
        return 1;
    }

    printf("=== Header verification ===\n");

    /* Detect and verify header */
    int ver = jeefs_header_detect_version(eeprom, EEPROM_SIZE);
    check_int("header_version", ver, 3);

    int crc_ok = jeefs_header_verify_crc(eeprom, EEPROM_SIZE);
    if (crc_ok != 0) {
        fprintf(stderr, "  FAIL: header CRC32 invalid\n");
        failures++;
    } else {
        printf("  OK: header CRC32 valid\n");
    }

    check_str("boardname", (const char *)(eeprom + 12), "JetHub-D1p");
    check_str("boardversion", (const char *)(eeprom + 44), "2.0");
    check_str("serial", (const char *)(eeprom + 76), "SN-GOLDEN-001");
    check_int("signature_version", eeprom[9], 0);

    printf("\n=== Filesystem verification ===\n");

    /* Walk the linked list */
    const char *expected_names[] = {"config", "wifi.conf", "serial"};
    int file_count = 0;
    uint16_t offset = 256; /* after v3 header */

    while (offset != 0 && offset < EEPROM_SIZE) {
        const JEEFSFileHeaderv1 *fh = (const JEEFSFileHeaderv1 *)(eeprom + offset);

        /* Check if this looks like a valid file header */
        if (fh->name[0] == '\0')
            break;

        if (file_count < 3) {
            check_str("filename", fh->name, expected_names[file_count]);
        }

        printf("  File %d: \"%s\" size=%u next=%u\n", file_count, fh->name, fh->dataSize, fh->nextFileAddress);

        /* Verify file data CRC */
        const uint8_t *file_data = eeprom + offset + sizeof(JEEFSFileHeaderv1);
        uint32_t calc_crc = crc32(0L, file_data, fh->dataSize);
        if (calc_crc != fh->crc32) {
            fprintf(stderr, "  FAIL: file '%s' CRC mismatch: stored=0x%08x calculated=0x%08x\n", fh->name, fh->crc32,
                    calc_crc);
            failures++;
        } else {
            printf("  OK: file '%s' CRC32 = 0x%08x\n", fh->name, fh->crc32);
        }

        file_count++;
        offset = fh->nextFileAddress;
    }

    check_int("file_count", file_count, 3);

    printf("\nResult: %d failure(s)\n", failures);
    return failures > 0 ? 1 : 0;
}
