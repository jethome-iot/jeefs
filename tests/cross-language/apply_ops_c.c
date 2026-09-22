// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Mutation-vector runner (C): execute a compiled operation program against an
 * image and report what happened, then dump the resulting bytes. The Rust
 * runner (apply_ops_rs) executes the same program and prints the same journal,
 * so the two ports are compared on both — see verify_fs_mutation.py.
 *
 * The program is produced by tests/cross-language/ops_program.py, the single
 * reader of the .ops text. While every runner parsed that text itself, the
 * hand-written parsers disagreed on malformed input — an empty token, a
 * trailing character, an offset too wide for the type — and each disagreement
 * showed up as a divergence between the *ports* that was really a divergence
 * between the *parsers*. A program leaves nothing to interpret: fixed-width
 * little-endian integers and length-prefixed blobs.
 *
 * Usage: apply_ops_c <program.jops> <out.bin>
 */

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
/* Room for a name far beyond the format's 15-byte limit: a scenario names a
 * file the format must refuse, so the bytes have to reach the API intact,
 * but a name this long can only come from a damaged program. */
/* The format's ceiling for a name blob; the buffer below is larger so the
 * limit is the format's rather than this runner's. */
#define MAX_NAME 255

/* Opcodes and image kinds, as ops_program.py emits them. */
enum {
    OP_INIT = 0,
    OP_FORMAT = 1,
    OP_ADD = 2,
    OP_WRITE = 3,
    OP_DELETE = 4,
    OP_READ = 5,
    OP_LIST = 6,
    OP_WALK = 7,
    OP_POKE = 8,
    OP_RESEAL = 9,
    OP_CONSISTENCY = 10,
};

enum {
    IMG_ZEROS = 0,
    IMG_ERASED = 1,
    IMG_GARBAGE = 2,
};

static uint8_t image[MAX_IMG];
static uint16_t image_size;

/* A cursor over the program bytes. Every read is bounds-checked against
 * `len` before it happens, so a truncated file cannot be read past. */
struct reader {
    const uint8_t *data;
    size_t len;
    size_t pos;
};

/* A program is a build artifact: the compiler emits whole operations or
 * fails. Anything unreadable here means the file is damaged, and reporting
 * that as an operation outcome would make the journals compare the harness
 * rather than the ports — so it ends the run instead. _Noreturn is what lets
 * the readers below treat a failed bounds check as the end of the road. */
/* The whole program, kept reachable for the reason main explains. */
static uint8_t *g_program;

static _Noreturn void truncated(const char *what) {
    fprintf(stderr, "truncated program: %s\n", what);
    exit(2);
}

static _Noreturn void malformed(const char *what) {
    fprintf(stderr, "malformed program: %s\n", what);
    exit(2);
}

static uint8_t take_u8(struct reader *r, const char *what) {
    if (r->len - r->pos < 1)
        truncated(what);
    return r->data[r->pos++];
}

static uint32_t take_u32(struct reader *r, const char *what) {
    if (r->len - r->pos < 4)
        truncated(what);
    uint32_t v = jeefs_get_le32(r->data + r->pos);
    r->pos += 4;
    return v;
}

/* The length is compared against the room left rather than added to the
 * cursor: a damaged program declaring a 4 GB blob would wrap the sum and
 * hand back a pointer past the end. */
static const uint8_t *take_blob(struct reader *r, uint32_t *len, const char *what) {
    uint32_t n = take_u32(r, what);
    if (n > r->len - r->pos)
        truncated(what);
    const uint8_t *p = r->data + r->pos;
    r->pos += n;
    *len = n;
    return p;
}

/* The FS API takes a NUL-terminated string, the program carries the name as
 * bytes. Copy and terminate, but do not inspect the content: a scenario
 * deliberately names a file outside the format's printable-ASCII domain to
 * watch every port refuse it (#116).
 *
 * The two format rules are enforced, not assumed. A name past MAX_NAME or
 * carrying a NUL is a malformed program: terminating here would silently
 * shorten such a name, where a reader that hands the bytes to its own
 * length check would refuse it — the readers have to agree on a damaged
 * artifact too (#119). */
static const char *take_name(struct reader *r, char *buf, size_t cap) {
    uint32_t len;
    const uint8_t *p = take_blob(r, &len, "name");
    if (len > MAX_NAME || len >= cap)
        malformed("name blob over the format's limit");
    if (memchr(p, '\0', len) != NULL)
        malformed("NUL inside a name blob");
    memcpy(buf, p, len);
    buf[len] = '\0';
    return buf;
}

static uint8_t *read_program(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "%s: cannot open\n", path);
        exit(2);
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "%s: cannot seek\n", path);
        exit(2);
    }
    long size = ftell(f);
    if (size < 0) {
        fprintf(stderr, "%s: cannot size\n", path);
        exit(2);
    }
    rewind(f);
    /* +1 so an empty program still gets a pointer worth freeing. */
    uint8_t *buf = malloc((size_t) size + 1);
    if (!buf) {
        fprintf(stderr, "%s: out of memory\n", path);
        exit(2);
    }
    if (fread(buf, 1, (size_t) size, f) != (size_t) size) {
        fprintf(stderr, "%s: short read\n", path);
        exit(2);
    }
    fclose(f);
    *out_len = (size_t) size;
    return buf;
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

static void init_image(uint8_t kind, uint32_t size) {
    /* The program is external input: an oversized value would run past the
     * static buffer and truncate through the uint16_t cast. */
    if (size == 0 || size > MAX_IMG) {
        fprintf(stderr, "image size out of range: %u\n", (unsigned) size);
        exit(2);
    }
    image_size = (uint16_t) size;
    switch (kind) {
        case IMG_ERASED:
            memset(image, 0xFF, size);
            break;
        case IMG_GARBAGE:
            for (uint32_t i = 0; i < size; i++)
                image[i] = (uint8_t) (i * 37u + 11u); // deterministic, no magic
            break;
        case IMG_ZEROS:
            memset(image, 0x00, size);
            break;
        default:
            malformed("unknown image kind");
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <program.jops> <out.bin>\n", argv[0]);
        return 2;
    }
    size_t len = 0;
    /* Held in a file-scope pointer, not a local: the readers end a damaged
     * program through _Noreturn exits and the operation loop can return
     * early, so a buffer owned by main alone would be unreachable at exit
     * on those paths. A leak checker then reports it and replaces the exit
     * code — which made this runner answer 1 where the Rust one answered 2
     * on an unknown opcode, and the damaged-program comparison caught it.
     * Reachable from here, it is not a leak on any path. */
    g_program = read_program(argv[1], &len);
    struct reader r = {g_program, len, 0};

    if (r.len < 4 || memcmp(r.data, "JOPS", 4) != 0)
        malformed("not a program");
    r.pos = 4;
    if (take_u8(&r, "version") != 1)
        malformed("unsupported program version");

    static char names[MAX_FILES][JEEFS_FILE_NAME_LENGTH + 1];
    static char name[MAX_NAME + 1]; /* the terminator this runner adds */
    int idx = 0;

    init_image(IMG_ZEROS, 8192);

    while (r.pos < r.len) {
        uint8_t op = take_u8(&r, "opcode");

        switch (op) {
            case OP_INIT: {
                uint8_t kind = take_u8(&r, "init kind");
                uint32_t size = take_u32(&r, "init size");
                init_image(kind, size);
                printf("%d init ok %u\n", idx, (unsigned) image_size);
                break;
            }
            case OP_FORMAT: {
                int version = (int) take_u32(&r, "format version");
                int rc = EEPROM_FormatEEPROM(image, image_size, version);
                if (rc < 0)
                    printf("%d format err %s\n", idx, err_class((int16_t) rc));
                else
                    printf("%d format ok 0\n", idx);
                break;
            }
            case OP_ADD:
            case OP_WRITE: {
                const char *file = take_name(&r, name, sizeof(name));
                uint32_t n;
                const uint8_t *data = take_blob(&r, &n, "data");
                /* The API carries the length in a uint16_t; a longer blob
                 * would silently become a different payload. */
                if (n > MAX_IMG) {
                    fprintf(stderr, "payload too large: %u\n", (unsigned) n);
                    return 2;
                }
                const char *what = op == OP_ADD ? "add" : "write";
                int16_t rc = op == OP_ADD ? EEPROM_AddFile(image, image_size, file, data, (uint16_t) n)
                                          : EEPROM_WriteFile(image, image_size, file, data, (uint16_t) n);
                /* AddFile reports "already there" as 0 written; the Rust port
                 * spells the same outcome FsError::FileExists. */
                if (rc == 0 && op == OP_ADD)
                    printf("%d %s err exists\n", idx, what);
                else if (rc < 0)
                    printf("%d %s err %s\n", idx, what, err_class(rc));
                else
                    printf("%d %s ok %d\n", idx, what, (int) rc);
                break;
            }
            case OP_DELETE: {
                const char *file = take_name(&r, name, sizeof(name));
                int16_t rc = EEPROM_DeleteFile(image, image_size, file);
                if (rc < 0)
                    printf("%d delete err %s\n", idx, err_class(rc));
                else
                    printf("%d delete ok %d\n", idx, (int) rc);
                break;
            }
            case OP_READ: {
                static uint8_t buf[MAX_IMG];
                const char *file = take_name(&r, name, sizeof(name));
                uint32_t cap = take_u32(&r, "read capacity");
                if (cap > sizeof(buf))
                    cap = sizeof(buf);
                int16_t rc = EEPROM_ReadFile(image, image_size, file, buf, (uint16_t) cap);
                if (rc < 0)
                    printf("%d read err %s\n", idx, err_class(rc));
                else
                    printf("%d read ok %d %08x\n", idx, (int) rc, jeefs_crc32(buf, (size_t) rc));
                break;
            }
            case OP_LIST: {
                int16_t n = EEPROM_ListFiles(image, image_size, names, MAX_FILES);
                if (n < 0) {
                    printf("%d list err %s\n", idx, err_class(n));
                } else {
                    printf("%d list ok %d", idx, (int) n);
                    for (int16_t i = 0; i < n; i++)
                        printf(" %s", names[i]);
                    printf("\n");
                }
                break;
            }
            case OP_POKE: {
                /* poke <offset> <byte>: corrupt the medium under the reader */
                unsigned long off = take_u32(&r, "poke offset");
                uint32_t val = take_u32(&r, "poke value");
                if (val > 0xFF)
                    malformed("poke value is not a byte");
                if (off < image_size) {
                    image[off] = (uint8_t) val;
                    printf("%d poke ok %lu\n", idx, off);
                } else {
                    printf("%d poke skip\n", idx);
                }
                break;
            }
            case OP_RESEAL: {
                /* reseal <offset>: recompute a file header's headerCrc32 after a
                 * poke, so a scenario can present a header that was legally
                 * WRITTEN with unusual content rather than merely corrupted. */
                unsigned long off = take_u32(&r, "reseal offset");
                /* Compare against the room left rather than adding to off: a
                 * scenario naming a huge offset would wrap the sum and write
                 * past the image. */
                if (off < image_size && image_size - off >= sizeof(JEEFSFileHeaderv1)) {
                    uint32_t c = jeefs_crc32(image + off, offsetof(JEEFSFileHeaderv1, headerCrc32));
                    jeefs_put_le32(image + off + offsetof(JEEFSFileHeaderv1, headerCrc32), c);
                    printf("%d reseal ok %lu\n", idx, off);
                } else {
                    /* Out of range prints no number: an offset past the image
                     * says nothing about the port, and the journal exists to
                     * compare the ports. */
                    printf("%d reseal skip\n", idx);
                }
                break;
            }
            case OP_WALK: {
                /* Locate the file the way a bounded-RAM environment does: the
                 * pull-model walker plus a running CRC over the payload. The
                 * journal records the read count too, so a port that reaches
                 * the same terminal by a different number of hops diverges. */
                const char *file = take_name(&r, name, sizeof(name));
                JEEFSWalk w;
                uint16_t prefix_len = image_size < 256 ? image_size : 256;
                int hops = 0;
                int16_t st = jeefs_walk_begin(&w, image, prefix_len, image_size, file);
                if (st < 0) {
                    printf("%d walk err %s %d\n", idx, err_class(st), hops);
                } else {
                    uint32_t off;
                    uint16_t wlen;
                    while (jeefs_walk_want(&w, &off, &wlen) == 1) {
                        hops++;
                        st = jeefs_walk_feed(&w, image + off, wlen);
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
                break;
            }
            case OP_CONSISTENCY:
                printf("%d consistency ok %d\n", idx, (int) EEPROM_HeaderCheckConsistency(image, image_size));
                break;
            default:
                fprintf(stderr, "unknown opcode: %u\n", (unsigned) op);
                return 2;
        }
        idx++;
    }
    free(g_program);
    g_program = NULL;

    FILE *out = fopen(argv[2], "wb");
    if (!out) {
        fprintf(stderr, "%s: cannot write\n", argv[2]);
        return 2;
    }
    fwrite(image, 1, image_size, out);
    fclose(out);
    return 0;
}
