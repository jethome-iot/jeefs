// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * Tests for the header-only C++ FS wrapper (jeefs::FileSystem).
 *
 * Delete/compaction scenarios are intentionally absent until the FS core
 * rewrite lands (issue #9); setHeader() return value is not asserted while
 * EEPROM_SetHeader keeps its inverted return (issue #6).
 */

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "jeefspp.hpp"
// Guard-collision regression: the old wrapper reused JEEFS_JEEFS_H, wiping
// whichever header came second. All three must coexist in one TU.
#include "jeefs.h"
#include "jeefs_headerpp.hpp"

static const char *kPath = TEST_DIR "/test_04_eeprom.bin";
static const uint16_t kSize = 8192;

static void make_blank_image() {
    FILE *f = fopen(kPath, "wb");
    assert(f != nullptr);
    std::vector<uint8_t> zeros(kSize, 0);
    size_t written = fwrite(zeros.data(), 1, zeros.size(), f);
    assert(written == zeros.size());
    fclose(f);
}

int main() {
    make_blank_image();

    // C symbols visible in the same TU (fails to compile on guard collision).
    static_assert(FILE_NAME_LENGTH == 15, "C macro must be visible");

    jeefs::FileSystem fs(kPath, kSize);
    assert(fs.valid());

    assert(fs.format(3) == 0);

    // Header round-trip through the pure header wrapper: composition of the
    // two C++ layers, no parsing duplicated inside FileSystem.
    auto hdr = fs.readHeader();
    assert(hdr.has_value());
    // exactly the header, not the header region: v3 header is 256 bytes
    assert(hdr->size() == 256);
    jeefs::HeaderView view(*hdr);
    assert(view.detect_version().value_or(-1) == 3);
    assert(view.verify_crc());

    // add + read back
    const std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
    assert(fs.addFile("config", data) > 0);

    auto rd = fs.readFile("config");
    assert(rd.has_value());
    assert(*rd == data);

    // list contains exactly the one file
    auto names = fs.listFiles();
    assert(names.has_value());
    assert(names->size() == 1);
    assert(names->at(0) == "config");

    // duplicate add is refused
    assert(fs.addFile("config", data) <= 0);

    // name longer than FILE_NAME_LENGTH is refused
    assert(fs.addFile("abcdefghijklmnop", data) < 0);

    // same-size overwrite
    const std::vector<uint8_t> data2 = {9, 9, 9, 9, 9, 9, 9, 9};
    assert(fs.writeFile("config", data2) > 0);
    rd = fs.readFile("config");
    assert(rd.has_value());
    assert(*rd == data2);

    // missing file: no value, error code retained
    auto missing = fs.readFile("nope");
    assert(!missing.has_value());
    assert(fs.lastError() <= 0);

    // payload larger than the C API's uint16_t size must be rejected, not
    // silently truncated by the size cast (65544 would wrap to 8 bytes)
    const std::vector<uint8_t> huge(65544, 0xAA);
    assert(fs.addFile("huge", huge) < 0);
    assert(fs.writeFile("config", huge) < 0);
    rd = fs.readFile("config");
    assert(rd.has_value());
    assert(*rd == data2);

    // consistency check on a healthy image: the C layer currently returns
    // 0 for "consistent" although jeefs.h documents 1 — the wrapper passes
    // the value through; contract unification is part of issue #9.
    assert(fs.checkConsistency() == 0);

    // move semantics: source becomes invalid, target keeps working
    jeefs::FileSystem fs2 = std::move(fs);
    assert(fs2.valid());
    assert(!fs.valid());
    auto rd2 = fs2.readFile("config");
    assert(rd2.has_value());
    assert(*rd2 == data2);

    // v1 header (512 B): readHeader must return the full v1 size and a
    // successful probe sequence must not leave a stale lastError from the
    // failed 256-byte attempt
    make_blank_image();
    jeefs::FileSystem fs3(kPath, kSize);
    assert(fs3.valid());
    assert(fs3.format(1) == 0);
    auto hdr1 = fs3.readHeader();
    assert(hdr1.has_value());
    assert(hdr1->size() == 512);
    assert(fs3.lastError() == 0);

    printf("test_04: OK\n");
    return 0;
}
