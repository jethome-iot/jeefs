// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Mutation-vector runner (C): apply a shared .ops scenario to an image and
 * report what happened, then dump the resulting bytes. The Rust runner
 * (apply_ops_rs) speaks the same script and the same journal, so the two
 * ports are compared on both — see verify_fs_mutation.py.
 *
 * Usage: apply_ops_c <scenario.ops> <out.bin>
 */

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eepromerr.h"
#include "jeefs.h"
#include "jeefs_endian.h"
#include "jeefs_port.h"
#include "jeefs_walk.h"

#define MAX_IMG 65535
#define MAX_FILES 512

static uint8_t image[MAX_IMG];
static uint16_t image_size;

/* An offset, in the one form both runners accept: an optional 0x or 0X
 * prefix, then one or more digits of that base, and nothing else. No
 * sign, no trailing characters, no empty token. Returns 0 and leaves
 * *out untouched when the token does not match.
 *
 * The grammar is spelled out rather than delegated to strtoul because
 * the two runners' library parsers disagree on every malformed form:
 * strtoul reads "12junk" as 12 and an empty string as 0, while Rust's
 * from_str_radix accepts a leading '+'. A vector with a malformed offset
 * would then mutate one runner's image and not the other's — and the
 * comparison would report a divergence between the ports that is really
 * a divergence between the parsers. Keeping the full width until the
 * bounds check matters for the same reason: narrowing first turned
 * 4294967296 into 0 on a 64-bit host. */
static int parse_offset(const char *s, unsigned long *out) {
    unsigned long base = 10;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s += 2;
    }
    if (*s == '\0')
        return 0;
    unsigned long v = 0;
    for (; *s != '\0'; s++) {
        unsigned long d;
        if (*s >= '0' && *s <= '9')
            d = (unsigned long) (*s - '0');
        else if (base == 16 && *s >= 'a' && *s <= 'f')
            d = (unsigned long) (*s - 'a') + 10;
        else if (base == 16 && *s >= 'A' && *s <= 'F')
            d = (unsigned long) (*s - 'A') + 10;
        else
            return 0;
        if (v > (ULONG_MAX - d) / base)
            return 0; /* overflow: out of range by definition */
        v = v * base + d;
    }
    *out = v;
    return 1;
}

/* A byte value for poke, in the one form both runners accept: an
 * optional 0x or 0X prefix, then one or two hex digits, and nothing
 * else. strtoul read "1ff" as 511 and the cast made it 0xFF, while Rust
 * overflowed u8 and wrote 0x00 — the same malformed vector produced
 * different media. */
static int parse_byte(const char *s, unsigned *out) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    size_t n = strlen(s);
    if (n == 0 || n > 2)
        return 0;
    unsigned v = 0;
    for (; *s != '\0'; s++) {
        unsigned d;
        if (*s >= '0' && *s <= '9')
            d = (unsigned) (*s - '0');
        else if (*s >= 'a' && *s <= 'f')
            d = (unsigned) (*s - 'a') + 10;
        else if (*s >= 'A' && *s <= 'F')
            d = (unsigned) (*s - 'A') + 10;
        else
            return 0;
        v = v * 16 + d;
    }
    *out = v;
    return 1;
}

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
    /* The scenario is external input: an oversized value would run past
     * the static buffer and truncate through the uint16_t cast. */
    if (size == 0 || size > MAX_IMG) {
        fprintf(stderr, "image size out of range: %u\n", size);
        exit(2);
    }
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
            unsigned long off;
            unsigned val;
            if (parse_offset(arg1, &off) && parse_byte(arg2, &val) && off < image_size) {
                image[off] = (uint8_t) val;
                printf("%d poke ok %lu\n", idx, off);
            } else {
                printf("%d poke skip\n", idx);
            }
        } else if (strcmp(op, "reseal") == 0) {
            /* reseal <offset>: recompute a file header's headerCrc32 after a
             * poke, so a scenario can present a header that was legally
             * WRITTEN with unusual content rather than merely corrupted. */
            unsigned long off;
            /* Compare against the room left rather than adding to off: a
             * scenario naming a huge offset would wrap the sum and write
             * past the image. */
            if (parse_offset(arg1, &off) && off < image_size && image_size - off >= sizeof(JEEFSFileHeaderv1)) {
                uint32_t c = jeefs_crc32(image + off, offsetof(JEEFSFileHeaderv1, headerCrc32));
                jeefs_put_le32(image + off + offsetof(JEEFSFileHeaderv1, headerCrc32), c);
                printf("%d reseal ok %lu\n", idx, off);
            } else {
                /* Out of range prints no number: the two runners parse an
                 * oversized offset into different widths, and the journal
                 * must compare the ports, not the parsers. */
                printf("%d reseal skip\n", idx);
            }
        } else if (strcmp(op, "walk") == 0) {
            /* Locate the file the way a bounded-RAM environment does: the
             * pull-model walker plus a running CRC over the payload. The
             * journal records the read count too, so a port that reaches
             * the same terminal by a different number of hops diverges. */
            JEEFSWalk w;
            uint16_t prefix_len = image_size < 256 ? image_size : 256;
            int hops = 0;
            int16_t st = jeefs_walk_begin(&w, image, prefix_len, image_size, arg1);
            if (st < 0) {
                printf("%d walk err %s %d\n", idx, err_class(st), hops);
            } else {
                uint32_t off;
                uint16_t len;
                while (jeefs_walk_want(&w, &off, &len) == 1) {
                    hops++;
                    st = jeefs_walk_feed(&w, image + off, len);
                    if (st != 0)
                        break;
                }
                if (st < 0) {
                    printf("%d walk err %s %d\n", idx, err_class(st), hops);
                } else if (w.state == JEEFS_WALK_FOUND) {
                    uint32_t crc = 0;
                    for (uint32_t o = 0; o < w.file_size; o += 64) {
                        uint32_t n = (uint32_t) w.file_size - o < 64 ? (uint32_t) w.file_size - o : 64;
                        crc = jeefs_crc32_update(crc, image + w.file_offset + o, n);
                    }
                    printf("%d walk ok found %d %u %u %08x %d\n", idx, hops, (unsigned) w.file_offset,
                           (unsigned) w.file_size, (unsigned) w.file_crc32, crc == w.file_crc32 ? 1 : 0);
                } else {
                    printf("%d walk ok notfound %d\n", idx, hops);
                }
            }
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
