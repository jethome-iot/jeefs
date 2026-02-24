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

    /* Check buffer is large enough for this version's struct */
    int hdr_size = jeefs_header_size(version);
    if (hdr_size < 0 || fsize < hdr_size) {
        fprintf(stderr, "Error: file too short for v%d header (%ld < %d bytes)\n", version, fsize, hdr_size);
        free(buf);
        return 1;
    }

    /* Verify CRC */
    if (jeefs_header_verify_crc(buf, (size_t)fsize) != 0) {
        fprintf(stderr, "Warning: CRC32 mismatch\n");
    } else {
        printf("CRC32: OK\n");
    }

    /* Access fields via union — safe now that buffer size is validated */
    const union JEEPROMHeader *hdr = (const union JEEPROMHeader *)buf;
    printf("Board name: %s\n", hdr->v2.boardname);
    printf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n", hdr->v2.mac[0], hdr->v2.mac[1],
           hdr->v2.mac[2], hdr->v2.mac[3], hdr->v2.mac[4], hdr->v2.mac[5]);

    free(buf);
    return 0;
}
