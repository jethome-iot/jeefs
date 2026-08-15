// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * DeviceIdentityV1 record API tests (issue #60): init/detect/CRC
 * round-trip, strict version gates, erased-buffer semantics, reserved
 * bytes ignored on read.
 */

/* Tests must assert in every build type, including -DNDEBUG ones. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "jeefs_devid.h"

static void test_init_detect_verify(void) {
    uint8_t rec[256];
    assert(jeefs_devid_init(rec, sizeof(rec)) == 0);
    assert(memcmp(rec, "JHDEVID\0", 8) == 0);
    assert(rec[8] == 1);
    assert(jeefs_devid_detect(rec, sizeof(rec)) == 1);
    assert(jeefs_devid_verify_crc(rec, sizeof(rec)) == 0);
    printf("  init/detect/verify: OK\n");
}

static void test_erased_buffers_are_no_record(void) {
    uint8_t rec[256];
    memset(rec, 0x00, sizeof(rec));
    assert(jeefs_devid_detect(rec, sizeof(rec)) == -1);
    memset(rec, 0xFF, sizeof(rec));
    assert(jeefs_devid_detect(rec, sizeof(rec)) == -1);
    printf("  erased buffers = no record: OK\n");
}

static void test_unknown_versions_rejected(void) {
    uint8_t rec[256];
    assert(jeefs_devid_init(rec, sizeof(rec)) == 0);

    rec[8] = 2; /* unknown record_version */
    assert(jeefs_devid_detect(rec, sizeof(rec)) == -1);
    rec[8] = 0;
    assert(jeefs_devid_detect(rec, sizeof(rec)) == -1);
    rec[8] = 1;

    rec[9] = 3; /* unknown signature_version under a known record_version */
    assert(jeefs_devid_detect(rec, sizeof(rec)) == -1);
    rec[9] = 2; /* SECP256R1 is valid */
    assert(jeefs_devid_detect(rec, sizeof(rec)) == 1);
    printf("  unknown versions rejected: OK\n");
}

static void test_crc_gates_content(void) {
    uint8_t rec[256];
    assert(jeefs_devid_init(rec, sizeof(rec)) == 0);
    rec[12] = 'X'; /* device_model corrupted after CRC */
    assert(jeefs_devid_verify_crc(rec, sizeof(rec)) == -1);
    assert(jeefs_devid_update_crc(rec, sizeof(rec)) == 0);
    assert(jeefs_devid_verify_crc(rec, sizeof(rec)) == 0);
    printf("  CRC gates content: OK\n");
}

static void test_nonzero_reserved_is_not_corruption(void) {
    /* Readers MUST ignore reserved content (spec): a record whose reserved
     * bytes are non-zero but whose CRC is valid parses fine. */
    uint8_t rec[256];
    assert(jeefs_devid_init(rec, sizeof(rec)) == 0);
    rec[94] = 0xAA; /* inside reserved2 */
    rec[92] = 0x01; /* a reserved flags bit */
    assert(jeefs_devid_update_crc(rec, sizeof(rec)) == 0);
    assert(jeefs_devid_detect(rec, sizeof(rec)) == 1);
    assert(jeefs_devid_verify_crc(rec, sizeof(rec)) == 0);
    printf("  non-zero reserved ignored: OK\n");
}

static void test_short_buffer(void) {
    uint8_t rec[256];
    assert(jeefs_devid_init(rec, 255) == -1);
    assert(jeefs_devid_init(rec, sizeof(rec)) == 0);
    assert(jeefs_devid_detect(rec, 255) == -1);
    assert(jeefs_devid_verify_crc(rec, 100) == -1);
    printf("  short buffer rejected: OK\n");
}

static void test_committed_vector(void) {
    /* Cross-language lock: parse the committed record vector; expectations
     * mirror test-vectors/vectors/devid_record_v1.json. */
    FILE *f = fopen(VECTORS_DIR "/devid_record_v1.bin", "rb");
    assert(f != NULL);
    uint8_t rec[256];
    assert(fread(rec, 1, sizeof(rec), f) == sizeof(rec));
    fclose(f);

    assert(jeefs_devid_detect(rec, sizeof(rec)) == 1);
    assert(jeefs_devid_verify_crc(rec, sizeof(rec)) == 0);
    assert(memcmp(rec + 12, "JetHub-D2", 10) == 0);
    assert(memcmp(rec + 44, "DSN-2026-000042", 16) == 0);
    assert(memcmp(rec + 76, "1.2a", 5) == 0);
    printf("  committed vector: OK\n");
}

int main(void) {
    printf("test_07: DeviceIdentityV1 record API\n");
    test_committed_vector();
    test_init_detect_verify();
    test_erased_buffers_are_no_record();
    test_unknown_versions_rejected();
    test_crc_gates_content();
    test_nonzero_reserved_is_not_corruption();
    test_short_buffer();
    printf("test_07: all OK\n");
    return 0;
}
