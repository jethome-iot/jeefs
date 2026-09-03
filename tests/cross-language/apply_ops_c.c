// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Mutation-vector runner (C): apply a shared .ops scenario to an image and
 * report what happened, then dump the resulting bytes. The Rust runner
 * (apply_ops_rs) speaks the same script and the same journal, so the two
 * ports are compared on both — see verify_fs_mutation.py.
 *
 * Usage: apply_ops_c <scenario.ops> <out.bin>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eepromerr.h"
#include "jeefs.h"
#include "jeefs_port.h"

#define MAX_IMG 65535
#define MAX_FILES 512

static uint8_t image[MAX_IMG];
static uint16_t image_size;

static const char *err_class(int16_t code) {
    switch (code) {
        case FILENOTFOUND:
            return "not_found";
        case FILENAMENOTVALID:
        case FILENAMETOOLONG:
        case FILENAMETOOSHORT:
            return "name_invalid";
        case BUFFERNOTVALID:
            return "buffer_invalid";
        case NOTENOUGHSPACE:
            return "no_space";
        case EEPROMCORRUPTED:
            return "corrupted";
        case FSVERSIONNOTSUPPORTED:
            return "fs_version";
        default:
            return "other";
    }
}

/* "fill:<byte>:<count>" or a hex string; returns length or -1. */
static int parse_payload(const char *spec, uint8_t *out, size_t cap) {
    if (strncmp(spec, "fill:", 5) == 0) {
        unsigned byte = 0, count = 0;
        /* A byte above 255 would wrap in memset here while the Rust
         * runner rejects it outright — refuse it in both. */
        if (sscanf(spec + 5, "%u:%u", &byte, &count) != 2 || byte > 255 || count > cap)
            return -1;
        memset(out, (int) byte, count);
        return (int) count;
    }
    size_t len = strlen(spec);
    if (len % 2 != 0 || len / 2 > cap)
        return -1;
    for (size_t i = 0; i < len; i += 2) {
        unsigned v;
        if (sscanf(spec + i, "%2x", &v) != 1)
            return -1;
        out[i / 2] = (uint8_t) v;
    }
    return (int) (len / 2);
}

static void init_image(const char *kind, unsigned size) {
    image_size = (uint16_t) size;
    if (strcmp(kind, "erased") == 0)
        memset(image, 0xFF, size);
    else if (strcmp(kind, "garbage") == 0)
        for (unsigned i = 0; i < size; i++)
            image[i] = (uint8_t) (i * 37u + 11u); // deterministic, no magic
    else
        memset(image, 0x00, size);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <scenario.ops> <out.bin>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "%s: cannot open\n", argv[1]);
        return 2;
    }

    static uint8_t payload[MAX_IMG];
    static char names[MAX_FILES][JEEFS_FILE_NAME_LENGTH + 1];
    char line[4096];
    int idx = 0;

    init_image("zeros", 8192);

    while (fgets(line, sizeof(line), f)) {
        char *nl = strchr(line, '\n');
        if (nl)
            *nl = '\0';
        if (line[0] == '\0' || line[0] == '#')
            continue;

        char op[32] = {0}, arg1[4096] = {0}, arg2[4096] = {0};
        int fields = sscanf(line, "%31s %4095s %4095s", op, arg1, arg2);
        if (fields < 1)
            continue;

        if (strcmp(op, "init") == 0) {
            init_image(arg1, (unsigned) atoi(arg2));
            printf("%d init ok %u\n", idx, (unsigned) image_size);
        } else if (strcmp(op, "format") == 0) {
            int r = EEPROM_FormatEEPROM(image, image_size, atoi(arg1));
            if (r < 0)
                printf("%d format err %s\n", idx, err_class((int16_t) r));
            else
                printf("%d format ok 0\n", idx);
        } else if (strcmp(op, "add") == 0 || strcmp(op, "write") == 0) {
            int n = parse_payload(arg2, payload, sizeof(payload));
            if (n < 0) {
                fprintf(stderr, "bad payload: %s\n", arg2);
                return 2;
            }
            int16_t r = strcmp(op, "add") == 0 ? EEPROM_AddFile(image, image_size, arg1, payload, (uint16_t) n)
                                               : EEPROM_WriteFile(image, image_size, arg1, payload, (uint16_t) n);
            /* AddFile reports "already there" as 0 written; the Rust port
             * spells the same outcome FsError::FileExists. */
            if (r == 0 && strcmp(op, "add") == 0)
                printf("%d %s err exists\n", idx, op);
            else if (r < 0)
                printf("%d %s err %s\n", idx, op, err_class(r));
            else
                printf("%d %s ok %d\n", idx, op, (int) r);
        } else if (strcmp(op, "delete") == 0) {
            int16_t r = EEPROM_DeleteFile(image, image_size, arg1);
            if (r < 0)
                printf("%d delete err %s\n", idx, err_class(r));
            else
                printf("%d delete ok %d\n", idx, (int) r);
        } else if (strcmp(op, "read") == 0) {
            static uint8_t buf[MAX_IMG];
            unsigned cap = (unsigned) atoi(arg2);
            if (cap > sizeof(buf))
                cap = sizeof(buf);
            int16_t r = EEPROM_ReadFile(image, image_size, arg1, buf, (uint16_t) cap);
            if (r < 0)
                printf("%d read err %s\n", idx, err_class(r));
            else
                printf("%d read ok %d %08x\n", idx, (int) r, jeefs_crc32(buf, (size_t) r));
        } else if (strcmp(op, "list") == 0) {
            int16_t n = EEPROM_ListFiles(image, image_size, names, MAX_FILES);
            if (n < 0) {
                printf("%d list err %s\n", idx, err_class(n));
            } else {
                printf("%d list ok %d", idx, (int) n);
                for (int16_t i = 0; i < n; i++)
                    printf(" %s", names[i]);
                printf("\n");
            }
        } else if (strcmp(op, "poke") == 0) {
            /* poke <offset> <hexbyte>: corrupt the medium under the reader */
            /* Decimal, or 0x for hex — matching the Rust runner. Plain
             * strtoul(.., 0) would read a leading zero as octal and make
             * the two runners disagree on the vector, not on the port. */
            int base = (arg1[0] == '0' && (arg1[1] == 'x' || arg1[1] == 'X')) ? 16 : 10;
            unsigned off = (unsigned) strtoul(arg1, NULL, base);
            unsigned val = (unsigned) strtoul(arg2, NULL, 16);
            if (off < image_size)
                image[off] = (uint8_t) val;
            printf("%d poke ok %u\n", idx, off);
        } else if (strcmp(op, "consistency") == 0) {
            printf("%d consistency ok %d\n", idx, (int) EEPROM_HeaderCheckConsistency(image, image_size));
        } else {
            fprintf(stderr, "unknown op: %s\n", op);
            return 2;
        }
        idx++;
    }
    fclose(f);

    FILE *out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "%s: cannot write\n", argv[2]);
        return 2;
    }
    fwrite(image, 1, image_size, out);
    fclose(out);
    return 0;
}
