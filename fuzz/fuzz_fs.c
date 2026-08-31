// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * libFuzzer harness for the buffer-centric FS API (#25): the input IS the
 * image. Every operation must terminate with a defined result on any
 * byte soup — ASan/UBSan catch the rest. Mutating operations run on the
 * fuzz copy; a second pass re-validates the chain after each mutation so
 * an operation that "succeeds" into a corrupt state trips the harness.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "eepromerr.h"
#include "jeefs.h"
#include "jeefs_walk.h"

#define MAX_IMG 8192

/* Enough slots for every possible file (8192 / 28, kept with margin
 * from the 24-byte era) — a shorter list
 * would let ListFiles return "full" without walking the rest of the
 * chain, blinding the invariant to late corruption. */
#define MAX_FILES 344

static void exercise(uint8_t *img, uint16_t size) {
    static char names[MAX_FILES][JEEFS_FILE_NAME_LENGTH + 1];
    uint8_t buf[MAX_IMG];

    (void) EEPROM_HeaderCheckConsistency(img, size);

    // The pull-model walker (#81) must reach the same terminal state as
    // the in-memory iterator on any byte soup.
    {
        JEEFSWalk w;
        int16_t b = jeefs_walk_begin(&w, img, size < 256 ? size : 256, size, "fuzz");
        while (jeefs_walk_want(&w, NULL, NULL) == 1) {
            if (jeefs_walk_feed(&w, img + w.want_offset, w.want_len) != 0)
                break;
        }
        int16_t r = EEPROM_ReadFile(img, size, "fuzz", buf, sizeof(buf));
        int16_t terminal = b < 0 ? b : w.state;
        switch (terminal) {
            case JEEFS_WALK_FOUND: // ReadFile still validates the tail + data CRC
                if (!(r > 0 || r == EEPROMCORRUPTED))
                    abort();
                break;
            case JEEFS_WALK_NOTFOUND:
                if (r != FILENOTFOUND)
                    abort();
                break;
            case EEPROMCORRUPTED:
            case FSVERSIONNOTSUPPORTED:
                if (r != terminal)
                    abort();
                break;
            default:
                break;
        }
    }

    int16_t n = EEPROM_ListFiles(img, size, names, MAX_FILES);
    if (n < 0) {
        // No walkable chain. A headerless image (magic mismatch) must be
        // CLAIMED by AddFile (#86): success means a fresh header and a
        // walkable one-file chain — anything else on that path aborts.
        const uint8_t payload[5] = {1, 2, 3, 4, 5};
        int16_t a = EEPROM_AddFile(img, size, "fuzz", payload, sizeof(payload));
        if (a == 5 &&
            (EEPROM_HeaderCheckConsistency(img, size) != 1 || EEPROM_ListFiles(img, size, names, MAX_FILES) != 1))
            abort();
        return;
    }

    for (int16_t i = 0; i < n; i++) {
        int16_t r = EEPROM_ReadFile(img, size, names[i], buf, sizeof(buf));
        if (r > 0) {
            // same-size overwrite must keep the chain walkable
            (void) EEPROM_WriteFile(img, size, names[i], buf, (uint16_t) r);
            if (EEPROM_ListFiles(img, size, names, MAX_FILES) < 0)
                abort();
        }
    }

    if (n > 0) {
        // deleting the first listed file must keep the chain walkable
        if (EEPROM_DeleteFile(img, size, names[0]) == 1 && EEPROM_ListFiles(img, size, names, MAX_FILES) < 0)
            abort();
    }

    const uint8_t payload[5] = {1, 2, 3, 4, 5};
    if (EEPROM_AddFile(img, size, "fuzz", payload, sizeof(payload)) == 5 &&
        EEPROM_ListFiles(img, size, names, MAX_FILES) < 0)
        abort();

    // The reserved identity name takes the insert-first path (#80): the
    // shift + relink over an arbitrary fuzzed chain must stay walkable.
    if (EEPROM_AddFile(img, size, JEEFS_DEVICE_ID_FILENAME, payload, sizeof(payload)) >= 0 &&
        EEPROM_ListFiles(img, size, names, MAX_FILES) < 0)
        abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t len) {
    if (len == 0 || len > MAX_IMG)
        return 0; // sub-12-byte inputs stay in: every op must gate lengths itself
    uint8_t img[MAX_IMG];
    memcpy(img, data, len);
    exercise(img, (uint16_t) len);
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
