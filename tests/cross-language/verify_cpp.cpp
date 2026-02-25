// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Cross-language verification using C++ API.
 * Usage: verify_cpp <bin_file> <json_file>
 */

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
        fprintf(stderr, "  FAIL: %s = \"%.*s\" (expected \"%s\")\n", name, (int)actual.size(), actual.data(), expected);
        failures++;
    } else {
        printf("  OK: %s = \"%.*s\"\n", name, (int)actual.size(), actual.data());
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
    if (json_get_string(json, "serial", expected_str, sizeof(expected_str)) == 0)
        check_sv("serial", hdr.serial(), expected_str);
    if (json_get_string(json, "usid", expected_str, sizeof(expected_str)) == 0)
        check_sv("usid", hdr.usid(), expected_str);
    if (json_get_string(json, "cpuid", expected_str, sizeof(expected_str)) == 0)
        check_sv("cpuid", hdr.cpuid(), expected_str);
    if (json_get_string(json, "mac", expected_str, sizeof(expected_str)) == 0)
        check_mac("mac", hdr.mac(), expected_str);

    /* V3-specific via direct struct access */
    if (ver && *ver == 3) {
        int expected_sig_ver = 0;
        if (json_get_int(json, "signature_version", &expected_sig_ver) == 0)
            check_int("signature_version", hdr.as_v3().signature_version, expected_sig_ver);
    }

    printf("\nResult: %d failure(s)\n", failures);
    return failures > 0 ? 1 : 0;
}
