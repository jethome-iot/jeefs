// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Cross-language verification using C++ API.
 * Usage: verify_cpp <bin_file> <json_file>
 */

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "jeefs_headerpp.hpp"

static int failures = 0;

static void check_sv(const char *name, std::string_view actual, const char *expected) {
    if (actual != expected) {
        fprintf(stderr, "  FAIL: %s = \"%.*s\" (expected \"%s\")\n", name, (int) actual.size(), actual.data(),
                expected);
        failures++;
    } else {
        printf("  OK: %s = \"%.*s\"\n", name, (int) actual.size(), actual.data());
    }
}

static void check_int(const char *name, int actual, int expected) {
    if (actual != expected) {
        fprintf(stderr, "  FAIL: %s = %d (expected %d)\n", name, actual, expected);
        failures++;
    } else {
        printf("  OK: %s = %d\n", name, actual);
    }
}

static void check_mac(const char *name, const uint8_t *actual, const char *expected_str) {
    if (!actual) {
        fprintf(stderr, "  FAIL: %s: mac pointer is null\n", name);
        failures++;
        return;
    }
    uint8_t expected[6];
    if (sscanf(expected_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &expected[0], &expected[1], &expected[2], &expected[3],
               &expected[4], &expected[5]) != 6) {
        fprintf(stderr, "  FAIL: cannot parse expected MAC: %s\n", expected_str);
        failures++;
        return;
    }
    if (memcmp(actual, expected, 6) != 0) {
        fprintf(stderr, "  FAIL: %s mismatch (expected %s)\n", name, expected_str);
        failures++;
    } else {
        printf("  OK: %s = %02x:%02x:%02x:%02x:%02x:%02x\n", name, actual[0], actual[1], actual[2], actual[3],
               actual[4], actual[5]);
    }
}

/* Simple JSON string extraction */
/* Signature sizes per algorithm (header-common.md): the field is 64 bytes,
 * but the algorithm decides how many of them carry the signature. */
static int expected_signature_size(int sig_ver) {
    switch (sig_ver) {
        case 0:
            return 0;
        case 1:
            return 48;
        case 2:
            return 64;
        default:
            return -1;
    }
}

/* Distinguish "key absent" from "key present but not a string": the
 * signature and timestamp checks must report malformed metadata instead of
 * treating it as an omitted optional field. */
static int json_key_present(const char *json, const char *key) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    return strstr(json, search) != NULL;
}

static int json_get_long(const char *json, const char *key, long long *out) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return -1;
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t'))
        pos++;
    /* atoll returns 0 for text, which would silently accept a malformed
     * timestamp whenever the wire value happens to be zero. */
    char *end = nullptr;
    long long value = strtoll(pos, &end, 10);
    if (end == pos)
        return -1;
    *out = value;
    return 0;
}

static int json_get_string(const char *json, const char *key, char *out, size_t out_size) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return -1;
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t'))
        pos++;
    if (*pos != '"')
        return -1;
    pos++;
    const char *end = strchr(pos, '"');
    if (!end)
        return -1;
    size_t len = static_cast<size_t>(end - pos);
    if (len >= out_size)
        len = out_size - 1;
    memcpy(out, pos, len);
    out[len] = '\0';
    return 0;
}

static int json_get_int(const char *json, const char *key, int *out) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return -1;
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t'))
        pos++;
    *out = atoi(pos);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <bin_file> <json_file>\n", argv[0]);
        return 2;
    }

    /* Read binary */
    std::ifstream bf(argv[1], std::ios::binary);
    if (!bf) {
        perror(argv[1]);
        return 2;
    }
    std::vector<uint8_t> bin_data((std::istreambuf_iterator<char>(bf)), std::istreambuf_iterator<char>());
    bf.close();

    /* Read JSON */
    std::ifstream jf(argv[2]);
    if (!jf) {
        perror(argv[2]);
        return 2;
    }
    std::string json_str((std::istreambuf_iterator<char>(jf)), std::istreambuf_iterator<char>());
    jf.close();
    const char *json = json_str.c_str();

    int expected_version = 0;
    json_get_int(json, "version", &expected_version);
    printf("Verifying (C++): %s (version %d, %zu bytes)\n", argv[1], expected_version, bin_data.size());

    /* Use C++ HeaderView API */
    jeefs::HeaderView hdr(bin_data);

    /* Version detection */
    auto ver = hdr.detect_version();
    check_int("detected_version", ver.value_or(-1), expected_version);

    /* CRC */
    if (!hdr.verify_crc()) {
        fprintf(stderr, "  FAIL: CRC verification failed\n");
        failures++;
    } else {
        printf("  OK: CRC32 valid\n");
    }

    /* Header size */
    int expected_size = 0;
    json_get_int(json, "header_size", &expected_size);
    check_int("header_size", hdr.header_size(), expected_size);

    /* String fields via C++ API */
    char expected_str[256];
    if (json_get_string(json, "boardname", expected_str, sizeof(expected_str)) == 0)
        check_sv("boardname", hdr.boardname(), expected_str);
    if (json_get_string(json, "boardversion", expected_str, sizeof(expected_str)) == 0)
        check_sv("boardversion", hdr.boardversion(), expected_str);
    /* v4 names the serial slot board_serial (same offset/view) */
    const bool is_v4 = ver && *ver == 4;
    const char *serial_key = is_v4 ? "board_serial" : "serial";
    if (json_get_string(json, serial_key, expected_str, sizeof(expected_str)) == 0)
        check_sv(serial_key, is_v4 ? hdr.board_serial() : hdr.serial(), expected_str);
    if (json_get_string(json, "usid", expected_str, sizeof(expected_str)) == 0)
        check_sv("usid", hdr.usid(), expected_str);
    if (json_get_string(json, "cpuid", expected_str, sizeof(expected_str)) == 0)
        check_sv("cpuid", hdr.cpuid(), expected_str);
    if (json_get_string(json, "mac", expected_str, sizeof(expected_str)) == 0)
        check_mac("mac", hdr.mac(), expected_str);

    /* V3/V4 tail via direct struct access (identical layout) */
    if (((ver && *ver == 3) || is_v4) && bin_data.size() < 256) {
        /* A file shorter than the header it claims to be has no tail to
         * read — stop before touching bytes that were never loaded. */
        fprintf(stderr, "  FAIL: file is %zu bytes, too short for a v3/v4 tail\n", bin_data.size());
        failures++;
    } else if ((ver && *ver == 3) || is_v4) {
        int expected_sig_ver = 0;
        if (json_get_int(json, "signature_version", &expected_sig_ver) == 0)
            check_int("signature_version",
                      (ver && *ver == 3) ? hdr.as_v3().signature_version : hdr.as_v4().signature_version,
                      expected_sig_ver);

        /* The signature field is 64 bytes whatever the algorithm puts in it:
         * a shorter signature is zero-padded to the end, and "no signature"
         * means the whole field is zero. Checking only the populated prefix
         * would let a generator leave anything behind it. */
        unsigned char expected_sig[64] = {0};
        size_t expected_len = 0;
        /* One char of slack past the 128 a full field needs, so an over-long
         * value is seen as over-long instead of being silently truncated
         * into something that fits. */
        char sig_hex[130] = {0};
        if (json_get_string(json, "signature_hex", sig_hex, sizeof(sig_hex)) != 0 &&
            json_key_present(json, "signature_hex")) {
            fprintf(stderr, "  FAIL: signature_hex is present but not a string\n");
            failures++;
        } else if (json_get_string(json, "signature_hex", sig_hex, sizeof(sig_hex)) == 0) {
            size_t hex_len = strlen(sig_hex);
            if (hex_len % 2 != 0 || hex_len / 2 > sizeof(expected_sig)) {
                fprintf(stderr, "  FAIL: signature_hex length %zu\n", hex_len);
                failures++;
            } else {
                expected_len = hex_len / 2;
                for (size_t i = 0; i < expected_len; i++) {
                    /* sscanf("%2x") accepts one digit and stops, so "0g"
                     * would read as 0x0; require both characters. */
                    if (!isxdigit(static_cast<unsigned char>(sig_hex[i * 2])) ||
                        !isxdigit(static_cast<unsigned char>(sig_hex[i * 2 + 1]))) {
                        fprintf(stderr, "  FAIL: signature_hex not hex\n");
                        failures++;
                        expected_len = 0;
                        break;
                    }
                    unsigned byte = 0;
                    sscanf(sig_hex + i * 2, "%2x", &byte);
                    expected_sig[i] = static_cast<unsigned char>(byte);
                }
            }
        }
        unsigned char sig_field[64];
        std::memcpy(sig_field, bin_data.data() + 180, sizeof(sig_field));
        /* The declared algorithm fixes the length; a vector that supplies a
         * different one is malformed however well the bytes match. */
        int wire_sig_ver = bin_data[9];
        int want_len = expected_signature_size(wire_sig_ver);
        if (want_len < 0) {
            fprintf(stderr, "  FAIL: unknown signature_version %d\n", wire_sig_ver);
            failures++;
        } else if (static_cast<size_t>(want_len) != expected_len) {
            fprintf(stderr, "  FAIL: signature_version %d wants %d bytes, vector supplies %zu\n", wire_sig_ver,
                    want_len, expected_len);
            failures++;
        } else if (std::memcmp(sig_field, expected_sig, expected_len) != 0) {
            fprintf(stderr, "  FAIL: signature mismatch\n");
            failures++;
        } else {
            bool tail_ok = true;
            for (size_t i = expected_len; i < sizeof(sig_field); i++)
                if (sig_field[i] != 0)
                    tail_ok = false;
            if (!tail_ok) {
                fprintf(stderr, "  FAIL: signature tail not zero-padded past %zu bytes\n", expected_len);
                failures++;
            } else {
                printf("  OK: signature (%zu bytes, zero-padded to 64)\n", expected_len);
            }
        }

        long long expected_ts = 0;
        if (json_get_long(json, "timestamp", &expected_ts) != 0 && json_key_present(json, "timestamp")) {
            /* Present but unparseable is malformed metadata, not an omitted
             * optional field: skipping it would accept any wire value. */
            fprintf(stderr, "  FAIL: timestamp is present but not an integer\n");
            failures++;
        } else if (json_get_long(json, "timestamp", &expected_ts) == 0) {
            /* The field is little-endian on the wire; memcpy into an
             * int64_t would read it host-endian. */
            uint64_t raw_ts = 0;
            for (int i = 7; i >= 0; i--)
                raw_ts = (raw_ts << 8) | bin_data[244 + i];
            int64_t actual_ts = static_cast<int64_t>(raw_ts);
            if (actual_ts != expected_ts) {
                fprintf(stderr, "  FAIL: timestamp = %lld (expected %lld)\n", (long long) actual_ts, expected_ts);
                failures++;
            } else {
                printf("  OK: timestamp = %lld\n", (long long) actual_ts);
            }
        }
    }

    printf("\nResult: %d failure(s)\n", failures);
    return failures > 0 ? 1 : 0;
}
