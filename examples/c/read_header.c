// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Example: Read EEPROM header, print version and MAC address.
 *
 * Usage: ./read_header <eeprom.bin>
 */

#include <stdio.h>
#include <stdlib.h>

#include "jeefs_generated.h"
#include "jeefs_header.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <eeprom.bin>\n", argv[0]);
        return 1;
    }

    /* Read file */
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buf = malloc((size_t)fsize);
    if (!buf) {
        fprintf(stderr, "malloc failed\n");
        fclose(f);
        return 1;
    }

    if (fread(buf, 1, (size_t)fsize, f) != (size_t)fsize) {
        fprintf(stderr, "fread failed\n");
        free(buf);
        fclose(f);
        return 1;
    }
    fclose(f);

    /* Detect header version */
    int version = jeefs_header_detect_version(buf, (size_t)fsize);
    if (version < 0) {
        fprintf(stderr, "Error: invalid EEPROM header (bad magic or too short)\n");
        free(buf);
        return 1;
    }
    printf("Header version: %d\n", version);

    /* Verify CRC */
    if (jeefs_header_verify_crc(buf, (size_t)fsize) != 0) {
        fprintf(stderr, "Warning: CRC32 mismatch\n");
    } else {
        printf("CRC32: OK\n");
    }

    /* Access fields via union — MAC is at the same offset for all versions */
    const JEEPROMHeaderv2 *hdr = (const JEEPROMHeaderv2 *)buf;
    printf("Board name: %.*s\n", 32, hdr->boardname);
    printf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n", hdr->mac[0], hdr->mac[1], hdr->mac[2],
           hdr->mac[3], hdr->mac[4], hdr->mac[5]);

    free(buf);
    return 0;
}
