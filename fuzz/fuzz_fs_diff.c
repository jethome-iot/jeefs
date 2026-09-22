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
#include "jeefs_port.h"
#include "jeefs_walk.h"

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
int16_t jeefs_rs_walk(const uint8_t *image, uint16_t size, const uint8_t *name, uint32_t *hops, uint32_t *offset,
                      uint16_t *file_size, uint32_t *crc, uint8_t *verified);

/* A small pool of names: the interesting collisions are repeats and the
 * reserved identity name, not the space of all 15-character strings. The
 * last two are outside the printable-ASCII domain the format defines — one
 * that UTF-8 can carry and one it cannot — so both ports have to reject
 * them the same way. Left out, each port inherits a different name domain
 * from its own string type and nothing notices (#116). */
static const char *const NAMES[] = {"a",
                                    "b",
                                    "cfg",
                                    JEEFS_DEVICE_ID_FILENAME,
                                    "0123456789abcde",
                                    "x.bin",
                                    "",
                                    "dup",
                                    "my file",
                                    "\xd0\xbf\xd1\x80",
                                    "A\xff"};
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
    /* Exactly the width the shim reads: it takes the whole 16-byte name
     * field, so anything shorter here would be undefined behaviour. */
    static char name_buf[JEEFS_FILE_NAME_LENGTH + 1];
    _Static_assert(sizeof(name_buf) == 16, "the Rust shim reads a 16-byte name field");

    for (int op_index = 0; op_index < MAX_OPS && cur.pos < cur.len; op_index++) {
        uint8_t op = take_u8(&cur) % 7;
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
            case 5: { /* walk: the bounded-RAM locator over the same image */
                uint32_t c_hops = 0, c_off = 0, c_crc = 0;
                uint16_t c_len = 0;
                uint8_t c_ok = 0;
                uint16_t prefix_len = size < 256 ? size : 256;
                JEEFSWalk w;
                int16_t c_rc = jeefs_walk_begin(&w, c_img, prefix_len, size, name_buf);
                if (c_rc >= 0) {
                    uint32_t at;
                    uint16_t want;
                    while (jeefs_walk_want(&w, &at, &want) == 1) {
                        c_hops++;
                        c_rc = jeefs_walk_feed(&w, c_img + at, want);
                        if (c_rc != 0)
                            break;
                    }
                    if (c_rc >= 0) {
                        if (w.state == JEEFS_WALK_FOUND) {
                            uint32_t running = 0;
                            for (uint32_t o = 0; o < w.file_size; o += 64) {
                                uint32_t n = (uint32_t) w.file_size - o < 64 ? (uint32_t) w.file_size - o : 64;
                                running = jeefs_crc32_update(running, c_img + w.file_offset + o, n);
                            }
                            c_off = w.file_offset;
                            c_len = w.file_size;
                            c_crc = w.file_crc32;
                            c_ok = running == w.file_crc32;
                            c_rc = JEEFS_WALK_FOUND;
                        } else {
                            c_rc = JEEFS_WALK_NOTFOUND;
                        }
                    }
                }

                uint32_t rs_hops = 0, rs_off = 0, rs_crc = 0;
                uint16_t rs_len = 0;
                uint8_t rs_ok = 0;
                int16_t rs_rc = jeefs_rs_walk(rs_img, size, (const uint8_t *) name_buf, &rs_hops, &rs_off, &rs_len,
                                              &rs_crc, &rs_ok);

                compare_result(c_rc, rs_rc, "walk");
                /* The read count is part of the contract: reaching the same
                 * terminal after a different number of header reads means
                 * the two state machines disagree about the chain. */
                if (c_hops != rs_hops) {
                    fprintf(stderr, "after walk: C read %u headers, Rust read %u\n", (unsigned) c_hops,
                            (unsigned) rs_hops);
                    divergence("walk hop count");
                }
                if (c_rc == JEEFS_WALK_FOUND &&
                    (c_off != rs_off || c_len != rs_len || c_crc != rs_crc || c_ok != rs_ok)) {
                    fprintf(stderr, "after walk: C {%u,%u,%08x,%u} Rust {%u,%u,%08x,%u}\n", (unsigned) c_off,
                            (unsigned) c_len, (unsigned) c_crc, c_ok, (unsigned) rs_off, (unsigned) rs_len,
                            (unsigned) rs_crc, rs_ok);
                    divergence("located file");
                }
                compare_images(c_img, rs_img, size, "walk");
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
