// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
#undef NDEBUG
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "args.h"
#include "jeefs.h"
#include "jeefs_devid.h"
#include "jeefs_endian.h"
#include "jeefs_header.h"
#include "jeefs_port.h"
#include "read_identity.h"

#define CAPACITY 8192
static uint8_t image[CAPACITY];
struct reader_state {
    unsigned bytes, calls, end, fail_call;
};
static int read_memory(void *ctx, uint32_t off, uint8_t *dst, uint16_t len) {
    struct reader_state *s = ctx;
    assert(off <= sizeof(image) && len <= sizeof(image) - off);
    s->calls++;
    if (s->fail_call && s->calls == s->fail_call)
        return -1;
    memcpy(dst, image + off, len);
    s->bytes += len;
    if (off + len > s->end)
        s->end = off + len;
    return 0;
}
static void prepare(int version, int device) {
    memset(image, 0xff, sizeof(image));
    assert(EEPROM_FormatEEPROM(image, sizeof(image), version) == 0);
    if (device) {
        DeviceIdentityV1 record;
        assert(jeefs_devid_init((uint8_t *) &record, sizeof(record)) == 0);
        memcpy(record.device_model, "J100 fixture", 12);
        assert(jeefs_devid_update_crc((uint8_t *) &record, sizeof(record)) == 0);
        assert(EEPROM_AddFile(image, sizeof(image), JEEFS_DEVICE_ID_FILENAME, (uint8_t *) &record, sizeof(record)) ==
               sizeof(record));
    }
    const uint8_t extra[] = {1, 2, 3};
    assert(EEPROM_AddFile(image, sizeof(image), "later", extra, sizeof(extra)) == sizeof(extra));
}
static int read_case(struct reader_state *s, struct jeefs_example_identity *out) {
    return jeefs_example_read_identity(read_memory, s, sizeof(image), out);
}
static void reseal_file(unsigned h) {
    unsigned n = jeefs_get_le16(image + h + 16);
    jeefs_put_le32(image + h + 18, jeefs_crc32(image + h + 28, n));
    jeefs_put_le32(image + h + 24, jeefs_crc32(image + h, 24));
}
int main(void) {
    unsigned value = 17;
    assert(jeefs_example_parse_arg("0x50", 16, 0x7f, &value) == 0 && value == 0x50);
    assert(jeefs_example_parse_arg("65535", 10, 65535, &value) == 0 && value == 65535);
    const char *bad[] = {"", "-1", "+1", "65536", "18446744073709551616", "1\n", "1x", "0x"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        value = 17;
        assert(jeefs_example_parse_arg(bad[i], 10, 65535, &value) != 0 && value == 17);
    }
    assert(jeefs_example_parse_arg("80", 16, 0x7f, &value) != 0);

    struct jeefs_example_identity out;
    for (int v = 1; v <= 4; v++) {
        prepare(v, 1);
        unsigned header = (unsigned) jeefs_header_size(v);
        struct reader_state s = {0};
        assert(read_case(&s, &out) == JEEFS_EXAMPLE_OK);
        assert(out.has_device && out.board_size == header);
        assert(!memcmp(out.device.device_model, "J100 fixture", 12));
        assert(s.bytes == header + 28 + 256 && s.end == s.bytes);
        assert(jeefs_header_verify_crc((uint8_t *) &out.board, out.board_size) == 0);
        assert(jeefs_devid_verify_crc((uint8_t *) &out.device, sizeof(out.device)) == 0);
        for (unsigned fail = 1; fail <= s.calls; fail++) {
            struct reader_state broken = {.fail_call = fail};
            assert(read_case(&broken, &out) == JEEFS_EXAMPLE_IO);
            assert(!out.has_device);
        }
    }
    for (int fill = 0; fill <= 255; fill += 255) {
        memset(image, fill, sizeof(image));
        struct reader_state s = {0};
        assert(read_case(&s, &out) == JEEFS_EXAMPLE_BOARD);
        assert(s.bytes == 12);
    }
    prepare(4, 0);
    struct reader_state s = {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_NO_DEVICE);
    assert(!out.has_device && s.end == 284); // Never request a second file header.
    memset(image, 0, sizeof(image));
    assert(jeefs_header_init(image, sizeof(image), 4) == 0);
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_NO_DEVICE && s.bytes == 256);
    prepare(4, 1);
    image[540 + 24] ^= 1; /* A damaged later file is outside the prefix contract. */
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_OK && s.end == 540);
    prepare(4, 1);
    image[20] ^= 1;
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_BOARD && s.bytes == 256);
    prepare(4, 1);
    image[10] = 2;
    assert(jeefs_header_update_crc(image, 256) == 0);
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_FS && s.bytes == 256);
    prepare(4, 1);
    image[256 + 24] ^= 1;
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_FS && s.bytes == 284);
    prepare(4, 1);
    image[284 + 20] ^= 1;
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_PAYLOAD);
    reseal_file(256);
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_RECORD);
    prepare(4, 1);
    image[284 + 8] = 2;
    reseal_file(256);
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_RECORD);
    prepare(4, 1);
    image[284 + 9] = 255;
    reseal_file(256);
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_RECORD);
    prepare(4, 1);
    jeefs_put_le16(image + 256 + 16, 255);
    jeefs_put_le16(image + 256 + 22, 0);
    reseal_file(256);
    s = (struct reader_state) {0};
    assert(read_case(&s, &out) == JEEFS_EXAMPLE_RECORD && s.bytes == 284);
    prepare(4, 1);
    s = (struct reader_state) {0};
    assert(jeefs_example_read_identity(read_memory, &s, 12, &out) == JEEFS_EXAMPLE_BOARD);
    assert(jeefs_example_read_identity(NULL, &s, sizeof(image), &out) == JEEFS_EXAMPLE_ARGUMENT);
    assert(jeefs_example_read_identity(read_memory, &s, sizeof(image), NULL) == JEEFS_EXAMPLE_ARGUMENT);
    assert(jeefs_example_read_identity(read_memory, &s, 0, &out) == JEEFS_EXAMPLE_ARGUMENT);
    puts("U-Boot identity example: all checks passed");
    return 0;
}
