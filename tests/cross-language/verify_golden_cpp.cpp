// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Golden reference EEPROM binary verification (C++).
 *
 * Reads the 8192-byte reference image, verifies:
 * 1. Header v3 fields match expected values (via jeefs::HeaderView)
 * 2. CRC32 is valid
 * 3. File system linked list contains 3 files: "config", "wifi.conf", "serial"
 * 4. File data CRC32 matches
 *
 * Usage: verify_golden_cpp <eeprom_full.bin>
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>
#include <zlib.h>

#include "jeefs_headerpp.hpp"

static constexpr size_t EEPROM_SIZE = 8192;
static int failures = 0;

static void check_sv(const char *name, std::string_view actual, const char *expected) {
    if (actual != expected) {
        fprintf(stderr, "  FAIL: %s = \"%.*s\" (expected \"%s\")\n", name, (int)actual.size(), actual.data(), expected);
        failures++;
    } else {
        printf("  OK: %s = \"%.*s\"\n", name, (int)actual.size(), actual.data());
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

    std::ifstream bf(argv[1], std::ios::binary);
    if (!bf) {
        perror(argv[1]);
        return 2;
    }
    std::vector<uint8_t> eeprom((std::istreambuf_iterator<char>(bf)), std::istreambuf_iterator<char>());
    bf.close();

    if (eeprom.size() != EEPROM_SIZE) {
        fprintf(stderr, "FAIL: file size = %zu (expected %zu)\n", eeprom.size(), EEPROM_SIZE);
        return 1;
    }

    printf("=== Header verification (C++) ===\n");

    /* Use C++ HeaderView API */
    jeefs::HeaderView hdr(eeprom);

    /* Version detection */
    auto ver = hdr.detect_version();
    check_int("header_version", ver.value_or(-1), 3);

    /* CRC */
    if (!hdr.verify_crc()) {
        fprintf(stderr, "  FAIL: header CRC32 invalid\n");
        failures++;
    } else {
        printf("  OK: header CRC32 valid\n");
    }

    /* Header fields via C++ API */
    check_sv("boardname", hdr.boardname(), "JetHub-D1p");
    check_sv("boardversion", hdr.boardversion(), "2.0");
    check_sv("serial", hdr.serial(), "SN-GOLDEN-001");
    check_int("signature_version", hdr.as_v3().signature_version, 0);

    printf("\n=== Filesystem verification (C++) ===\n");

    /* Walk the linked list using raw data + C structs */
    const char *expected_names[] = {"config", "wifi.conf", "serial"};
    int file_count = 0;
    uint16_t offset = 256; /* after v3 header */

    while (offset != 0 && offset < EEPROM_SIZE) {
        const auto *fh = reinterpret_cast<const JEEFSFileHeaderv1 *>(eeprom.data() + offset);

        /* Check if this looks like a valid file header */
        if (fh->name[0] == '\0')
            break;

        if (file_count < 3) {
            check_sv("filename", fh->name, expected_names[file_count]);
        }

        printf("  File %d: \"%s\" size=%u next=%u\n", file_count, fh->name, fh->dataSize, fh->nextFileAddress);

        /* Verify file data CRC */
        const uint8_t *file_data = eeprom.data() + offset + sizeof(JEEFSFileHeaderv1);
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
