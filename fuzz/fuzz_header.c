// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * libFuzzer harness for the pure header API: detect/verify/update/init
 * over arbitrary bytes; update+verify on a detected header must agree.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "jeefs_header.h"

#define MAX_IMG 1024

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    if (len == 0 || len > MAX_IMG)
        return 0;
    uint8_t buf[MAX_IMG];
    memcpy(buf, data, len);

    int ver = jeefs_header_detect_version(buf, len);
    (void) jeefs_header_verify_crc(buf, len);
    if (ver > 0 && jeefs_header_update_crc(buf, len) == 0 && jeefs_header_verify_crc(buf, len) != 0)
        abort(); // update must yield a verifiable header

    for (int v = 0; v <= 5; v++)
        if (jeefs_header_init(buf, len, v) == 0 && jeefs_header_verify_crc(buf, len) != 0)
            abort(); // init must yield a verifiable header

    return 0;
}

#ifdef JEEFS_FUZZ_DRIVER
#include <stdio.h>
int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        FILE *f = fopen(argv[i], "rb");
        if (!f)
            continue;
        uint8_t data[MAX_IMG];
        size_t n = fread(data, 1, sizeof(data), f);
        fclose(f);
        LLVMFuzzerTestOneInput(data, n);
        printf("ok: %s (%zu bytes)\n", argv[i], n);
    }
    return 0;
}
#endif
