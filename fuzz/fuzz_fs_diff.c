// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Differential harness: the same operations, against the C core and the Rust
 * port, on byte-identical images — and the two must agree on every result and
 * every byte afterwards.
 *
 * The shared mutation vectors already compare the two ports, but they explore
 * blindly: a fixed list of scenarios plus randomised ones, with no coverage
 * feedback and no corpus that survives the run. This is the coverage-guided
 * counterpart — libFuzzer steers the operation stream toward paths neither
 * implementation has taken yet, which is where a divergence is likely to hide.
 *
 * The fuzzer input is read as a program: a few bytes of prologue choose the
 * image, then each record is one operation. Everything is bounded, so a
 * malformed input is a short program rather than a rejected one.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eepromerr.h"
#include "jeefs.h"

#define MAX_IMG 8192
#define MAX_PAYLOAD 512
#define MAX_OPS 64

/* Implemented by rust/jeefs-fuzz-ffi, linked into this target. */
int16_t jeefs_rs_format(uint8_t *image, uint16_t size, uint8_t version);
int16_t jeefs_rs_add(uint8_t *image, uint16_t size, const uint8_t *name, const uint8_t *data, uint16_t data_len);
int16_t jeefs_rs_write(uint8_t *image, uint16_t size, const uint8_t *name, const uint8_t *data, uint16_t data_len);
int16_t jeefs_rs_delete(uint8_t *image, uint16_t size, const uint8_t *name);
int16_t jeefs_rs_read(const uint8_t *image, uint16_t size, const uint8_t *name, uint8_t *out, uint16_t out_len);
int16_t jeefs_rs_list(const uint8_t *image, uint16_t size, uint8_t *out, uint16_t max_files);

/* A small pool of names: the interesting collisions are repeats and the
 * reserved identity name, not the space of all 15-character strings. */
static const char *const NAMES[] = {"a", "b", "cfg", JEEFS_DEVICE_ID_FILENAME, "0123456789abcde", "x.bin", "", "dup"};
#define NAME_COUNT (sizeof(NAMES) / sizeof(NAMES[0]))

struct cursor {
    const uint8_t *data;
    size_t len;
    size_t pos;
};

static uint8_t take_u8(struct cursor *c) {
    if (c->pos >= c->len)
        return 0;
    return c->data[c->pos++];
}

static uint16_t take_u16(struct cursor *c) {
    uint16_t hi = take_u8(c);
    return (uint16_t) ((hi << 8) | take_u8(c));
}

static void divergence(const char *what) {
    fprintf(stderr, "DIVERGENCE: %s\n", what);
    abort();
}

static void compare_images(const uint8_t *c_img, const uint8_t *rs_img, uint16_t size, const char *op) {
    if (memcmp(c_img, rs_img, size) == 0)
        return;
    size_t at = 0;
    while (at < size && c_img[at] == rs_img[at])
        at++;
    fprintf(stderr, "after %s: images differ at byte %zu (C 0x%02x, Rust 0x%02x)\n", op, at, c_img[at], rs_img[at]);
    divergence("image bytes");
}

static void compare_result(int16_t c_rc, int16_t rs_rc, const char *op) {
    if (c_rc == rs_rc)
        return;
    fprintf(stderr, "after %s: C returned %d, Rust returned %d\n", op, (int) c_rc, (int) rs_rc);
    divergence("operation result");
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    if (len < 4)
        return 0;

    struct cursor cur = {data, len, 0};

    /* Prologue: image size and initial fill. Both ports see the same bytes. */
    uint16_t size = (uint16_t) (256 + (take_u16(&cur) % (MAX_IMG - 256 + 1)));
    uint8_t fill_kind = take_u8(&cur) % 3;

    static uint8_t c_img[MAX_IMG];
    static uint8_t rs_img[MAX_IMG];
    switch (fill_kind) {
        case 0:
            memset(c_img, 0x00, size);
            break;
        case 1:
            memset(c_img, 0xFF, size);
            break;
        default:
            for (uint16_t i = 0; i < size; i++)
                c_img[i] = (uint8_t) (i * 37u + 11u);
            break;
    }
    memcpy(rs_img, c_img, size);

    static uint8_t payload[MAX_PAYLOAD];
    static uint8_t c_out[MAX_IMG];
    static uint8_t rs_out[MAX_IMG];
    static char name_buf[JEEFS_FILE_NAME_LENGTH + 1];

    for (int op_index = 0; op_index < MAX_OPS && cur.pos < cur.len; op_index++) {
        uint8_t op = take_u8(&cur) % 6;
        const char *name = NAMES[take_u8(&cur) % NAME_COUNT];
        memset(name_buf, 0, sizeof(name_buf));
        strncpy(name_buf, name, JEEFS_FILE_NAME_LENGTH);

        switch (op) {
            case 0: { /* format */
                uint8_t version = take_u8(&cur) % 6; /* includes unknown versions */
                int16_t c_rc = (int16_t) EEPROM_FormatEEPROM(c_img, size, version);
                int16_t rs_rc = jeefs_rs_format(rs_img, size, version);
                compare_result(c_rc, rs_rc, "format");
                compare_images(c_img, rs_img, size, "format");
                break;
            }
            case 1: { /* add */
                uint16_t n = take_u16(&cur) % (MAX_PAYLOAD + 1);
                uint8_t byte = take_u8(&cur);
                memset(payload, byte, n);
                int16_t c_rc = EEPROM_AddFile(c_img, size, name_buf, payload, n);
                int16_t rs_rc = jeefs_rs_add(rs_img, size, (const uint8_t *) name_buf, payload, n);
                compare_result(c_rc, rs_rc, "add");
                compare_images(c_img, rs_img, size, "add");
                break;
            }
            case 2: { /* write */
                uint16_t n = take_u16(&cur) % (MAX_PAYLOAD + 1);
                uint8_t byte = take_u8(&cur);
                memset(payload, byte, n);
                int16_t c_rc = EEPROM_WriteFile(c_img, size, name_buf, payload, n);
                int16_t rs_rc = jeefs_rs_write(rs_img, size, (const uint8_t *) name_buf, payload, n);
                compare_result(c_rc, rs_rc, "write");
                compare_images(c_img, rs_img, size, "write");
                break;
            }
            case 3: { /* delete */
                int16_t c_rc = EEPROM_DeleteFile(c_img, size, name_buf);
                int16_t rs_rc = jeefs_rs_delete(rs_img, size, (const uint8_t *) name_buf);
                compare_result(c_rc, rs_rc, "delete");
                compare_images(c_img, rs_img, size, "delete");
                break;
            }
            case 4: { /* read */
                uint16_t cap = take_u16(&cur) % (MAX_IMG + 1);
                if (cap == 0)
                    cap = 1;
                memset(c_out, 0, cap);
                memset(rs_out, 0, cap);
                int16_t c_rc = EEPROM_ReadFile(c_img, size, name_buf, c_out, cap);
                int16_t rs_rc = jeefs_rs_read(rs_img, size, (const uint8_t *) name_buf, rs_out, cap);
                compare_result(c_rc, rs_rc, "read");
                if (c_rc > 0 && memcmp(c_out, rs_out, (size_t) c_rc) != 0)
                    divergence("read payload");
                /* Reading must not write. Comparing here as well means an
                 * accidental mutation is caught at the operation that made
                 * it, instead of being erased by a later format. */
                compare_images(c_img, rs_img, size, "read");
                break;
            }
            default: { /* list */
                enum { LIST_CAP = MAX_IMG / 28 + 1 };
                static char c_names[LIST_CAP][JEEFS_FILE_NAME_LENGTH + 1];
                static uint8_t rs_names[LIST_CAP * 16];
                memset(c_names, 0, sizeof(c_names));
                int16_t c_rc = EEPROM_ListFiles(c_img, size, c_names, (uint16_t) LIST_CAP);
                int16_t rs_rc = jeefs_rs_list(rs_img, size, rs_names, (uint16_t) LIST_CAP);
                compare_result(c_rc, rs_rc, "list");
                compare_images(c_img, rs_img, size, "list");
                /* Counting alone would miss a wrong name or a different
                 * order, which is exactly the kind of drift a port rewrite
                 * introduces. */
                for (int16_t i = 0; i < c_rc; i++) {
                    if (strncmp(c_names[i], (const char *) &rs_names[i * 16], JEEFS_FILE_NAME_LENGTH) != 0) {
                        fprintf(stderr, "list entry %d: C \"%s\", Rust \"%.15s\"\n", (int) i, c_names[i],
                                (const char *) &rs_names[i * 16]);
                        divergence("listed names");
                    }
                }
                break;
            }
        }
    }

    /* Reading is not supposed to change anything, and neither port may drift
     * from the other while idle. */
    compare_images(c_img, rs_img, size, "the whole program");
    return 0;
}

#ifdef JEEFS_FUZZ_DRIVER
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f)
            continue;
        static uint8_t buf[MAX_IMG];
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        LLVMFuzzerTestOneInput(buf, n);
        printf("ok: %s (%zu bytes)\n", argv[i], n);
    }
    return 0;
}
#endif
