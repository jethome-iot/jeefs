// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Cross-language header generator: create a .bin header from .json spec.
 *
 * Usage: generate_c <json_file> <output_bin>
 *   Reads the JSON spec, creates a binary header, writes to output file.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jeefs_generated.h"
#include "jeefs_header.h"

#define MAX_JSON_SIZE 4096

/* --- Minimal JSON helpers (same approach as verify_c.c) --- */

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
    size_t len = (size_t)(end - pos);
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

static void pack_string(uint8_t *dest, size_t field_size, const char *value) {
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
    return (int)byte_len;
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

    /* Get version */
    int version = 0;
    json_get_int(json, "version", &version);

    int hdr_size = jeefs_header_size(version);
    if (hdr_size <= 0) {
        fprintf(stderr, "Unsupported version: %d\n", version);
        return 1;
    }

    /* Initialize header buffer */
    uint8_t buf[512];
    memset(buf, 0, sizeof(buf));
    if (jeefs_header_init(buf, sizeof(buf), version) != 0) {
        fprintf(stderr, "jeefs_header_init failed\n");
        return 1;
    }

    /* Fill common fields */
    char str_val[256];

    if (json_get_string(json, "boardname", str_val, sizeof(str_val)) == 0)
        pack_string(buf + 12, 32, str_val);

    if (json_get_string(json, "boardversion", str_val, sizeof(str_val)) == 0)
        pack_string(buf + 44, 32, str_val);

    if (json_get_string(json, "serial", str_val, sizeof(str_val)) == 0)
        pack_string(buf + 76, 32, str_val);

    if (json_get_string(json, "usid", str_val, sizeof(str_val)) == 0)
        pack_string(buf + 108, 32, str_val);

    if (json_get_string(json, "cpuid", str_val, sizeof(str_val)) == 0)
        pack_string(buf + 140, 32, str_val);

    if (json_get_string(json, "mac", str_val, sizeof(str_val)) == 0) {
        uint8_t mac[6];
        if (parse_mac(str_val, mac) == 0)
            memcpy(buf + 172, mac, 6);
    }

    /* V3-specific fields */
    if (version == 3) {
        int sig_ver = 0;
        if (json_get_int(json, "signature_version", &sig_ver) == 0)
            buf[9] = (uint8_t)sig_ver;

        long long ts = 0;
        if (json_get_long(json, "timestamp", &ts) == 0) {
            int64_t timestamp = (int64_t)ts;
            memcpy(buf + 244, &timestamp, 8);
        }

        if (json_get_string(json, "signature_hex", str_val, sizeof(str_val)) == 0) {
            hex_to_bytes(str_val, buf + 180, 64);
        }
    }

    /* Finalize CRC */
    jeefs_header_update_crc(buf, (size_t)hdr_size);

    /* Write output */
    FILE *of = fopen(argv[2], "wb");
    if (!of) {
        perror(argv[2]);
        return 2;
    }
    fwrite(buf, 1, (size_t)hdr_size, of);
    fclose(of);

    printf("Generated (C): %s (%d bytes, version %d)\n", argv[2], hdr_size, version);
    return 0;
}
