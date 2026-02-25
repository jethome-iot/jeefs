// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Cross-language header generator (C++): create a .bin header from .json spec.
 *
 * Usage: generate_cpp <json_file> <output_bin>
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "jeefs_headerpp.hpp"

#define MAX_JSON_SIZE 4096

/* --- Minimal JSON helpers --- */

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

static int json_get_long(const char *json, const char *key, long long *out) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return -1;
    pos += strlen(search);
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t'))
        pos++;
    *out = atoll(pos);
    return 0;
}

static void pack_string(char *dest, size_t field_size, const char *value) {
    memset(dest, 0, field_size);
    size_t len = strlen(value);
    if (len >= field_size)
        len = field_size - 1;
    memcpy(dest, value, len);
}

static void pack_string_u8(uint8_t *dest, size_t field_size, const char *value) {
    memset(dest, 0, field_size);
    size_t len = strlen(value);
    if (len >= field_size)
        len = field_size - 1;
    memcpy(dest, value, len);
}

static int parse_mac(const char *mac_str, uint8_t mac[6]) {
    return sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6
               ? 0
               : -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, size_t max_len) {
    size_t hex_len = strlen(hex);
    size_t byte_len = hex_len / 2;
    if (byte_len > max_len)
        byte_len = max_len;
    for (size_t i = 0; i < byte_len; i++) {
        sscanf(hex + i * 2, "%2hhx", &out[i]);
    }
    return static_cast<int>(byte_len);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <json_file> <output_bin>\n", argv[0]);
        return 2;
    }

    /* Read JSON */
    FILE *jf = fopen(argv[1], "r");
    if (!jf) {
        perror(argv[1]);
        return 2;
    }
    char json[MAX_JSON_SIZE];
    size_t json_size = fread(json, 1, MAX_JSON_SIZE - 1, jf);
    json[json_size] = '\0';
    fclose(jf);

    int version = 0;
    json_get_int(json, "version", &version);

    /* Create header via C++ API */
    jeefs::HeaderBuffer buf(version);
    if (!buf.valid()) {
        fprintf(stderr, "Failed to create header for version %d\n", version);
        return 1;
    }

    char str_val[256];

    if (version == 1) {
        auto &hdr = buf.as_v1();
        if (json_get_string(json, "boardname", str_val, sizeof(str_val)) == 0)
            pack_string(hdr.boardname, sizeof(hdr.boardname), str_val);
        if (json_get_string(json, "boardversion", str_val, sizeof(str_val)) == 0)
            pack_string(hdr.boardversion, sizeof(hdr.boardversion), str_val);
        if (json_get_string(json, "serial", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.serial, sizeof(hdr.serial), str_val);
        if (json_get_string(json, "usid", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.usid, sizeof(hdr.usid), str_val);
        if (json_get_string(json, "cpuid", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.cpuid, sizeof(hdr.cpuid), str_val);
        if (json_get_string(json, "mac", str_val, sizeof(str_val)) == 0)
            parse_mac(str_val, hdr.mac);
    } else if (version == 2) {
        auto &hdr = buf.as_v2();
        if (json_get_string(json, "boardname", str_val, sizeof(str_val)) == 0)
            pack_string(hdr.boardname, sizeof(hdr.boardname), str_val);
        if (json_get_string(json, "boardversion", str_val, sizeof(str_val)) == 0)
            pack_string(hdr.boardversion, sizeof(hdr.boardversion), str_val);
        if (json_get_string(json, "serial", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.serial, sizeof(hdr.serial), str_val);
        if (json_get_string(json, "usid", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.usid, sizeof(hdr.usid), str_val);
        if (json_get_string(json, "cpuid", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.cpuid, sizeof(hdr.cpuid), str_val);
        if (json_get_string(json, "mac", str_val, sizeof(str_val)) == 0)
            parse_mac(str_val, hdr.mac);
    } else if (version == 3) {
        auto &hdr = buf.as_v3();
        if (json_get_string(json, "boardname", str_val, sizeof(str_val)) == 0)
            pack_string(hdr.boardname, sizeof(hdr.boardname), str_val);
        if (json_get_string(json, "boardversion", str_val, sizeof(str_val)) == 0)
            pack_string(hdr.boardversion, sizeof(hdr.boardversion), str_val);
        if (json_get_string(json, "serial", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.serial, sizeof(hdr.serial), str_val);
        if (json_get_string(json, "usid", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.usid, sizeof(hdr.usid), str_val);
        if (json_get_string(json, "cpuid", str_val, sizeof(str_val)) == 0)
            pack_string_u8(hdr.cpuid, sizeof(hdr.cpuid), str_val);
        if (json_get_string(json, "mac", str_val, sizeof(str_val)) == 0)
            parse_mac(str_val, hdr.mac);

        int sig_ver = 0;
        if (json_get_int(json, "signature_version", &sig_ver) == 0)
            hdr.signature_version = static_cast<uint8_t>(sig_ver);

        long long ts = 0;
        if (json_get_long(json, "timestamp", &ts) == 0)
            hdr.timestamp = static_cast<int64_t>(ts);

        if (json_get_string(json, "signature_hex", str_val, sizeof(str_val)) == 0)
            hex_to_bytes(str_val, hdr.signature, sizeof(hdr.signature));
    }

    buf.update_crc();

    /* Write output */
    std::ofstream of(argv[2], std::ios::binary);
    if (!of) {
        perror(argv[2]);
        return 2;
    }
    of.write(reinterpret_cast<const char *>(buf.data()), static_cast<std::streamsize>(buf.size()));

    printf("Generated (C++): %s (%zu bytes, version %d)\n", argv[2], buf.size(), version);
    return 0;
}
