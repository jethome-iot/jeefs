// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Cross-language verification: read a .bin header, verify fields match expected.
 *
 * Usage: verify_c <bin_file> <json_file>
 *   Reads the binary header, parses the JSON for expected values,
 *   compares all fields. Exits 0 on success, 1 on mismatch.
 *
 * Minimal JSON parsing (no external library) — extracts known fields
 * by simple string search. Sufficient for our well-defined test vectors.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "jeefs_generated.h"
#include "jeefs_header.h"

#define MAX_FILE_SIZE 4096
#define MAX_JSON_SIZE 4096

static int failures = 0;

static void check_string(const char *name, const char *actual, const char *expected) {
    if (strcmp(actual, expected) != 0) {
        fprintf(stderr, "  FAIL: %s = \"%s\" (expected \"%s\")\n", name, actual, expected);
        failures++;
    } else {
        printf("  OK: %s = \"%s\"\n", name, actual);
    }
}

/* Bounded string (RFC #13): compare against the whole field — value bytes,
 * then zero padding; no NUL required at full length. */
static void check_bounded(const char *name, const uint8_t *field, size_t field_size, const char *expected) {
    size_t elen = strlen(expected);
    int ok = elen <= field_size && memcmp(field, expected, elen) == 0;
    for (size_t i = elen; ok && i < field_size; i++)
        ok = field[i] == 0;
    if (!ok) {
        fprintf(stderr, "  FAIL: %s (bounded, expected \"%s\")\n", name, expected);
        failures++;
    } else {
        printf("  OK: %s = \"%s\"\n", name, expected);
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
    uint8_t expected[6];
    if (sscanf(expected_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx", &expected[0], &expected[1], &expected[2], &expected[3],
               &expected[4], &expected[5]) != 6) {
        fprintf(stderr, "  FAIL: cannot parse expected MAC: %s\n", expected_str);
        failures++;
        return;
    }
    if (memcmp(actual, expected, 6) != 0) {
        fprintf(stderr, "  FAIL: %s = %02x:%02x:%02x:%02x:%02x:%02x (expected %s)\n", name, actual[0], actual[1],
                actual[2], actual[3], actual[4], actual[5], expected_str);
        failures++;
    } else {
        printf("  OK: %s = %02x:%02x:%02x:%02x:%02x:%02x\n", name, actual[0], actual[1], actual[2], actual[3],
               actual[4], actual[5]);
    }
}

/* Extract a JSON string value for a given key (simple, no nesting) */
static int json_get_string(const char *json, const char *key, char *out, size_t out_size) {
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);
    const char *pos = strstr(json, search);
    if (!pos)
        return -1;
    pos += strlen(search);
    /* Skip whitespace and colon */
    while (*pos && (*pos == ' ' || *pos == ':' || *pos == '\t'))
        pos++;
    if (*pos != '"')
        return -1;
    pos++;
    const char *end = strchr(pos, '"');
    if (!end)
        return -1;
    size_t len = (size_t) (end - pos);
    if (len >= out_size)
        len = out_size - 1;
    memcpy(out, pos, len);
    out[len] = '\0';
    return 0;
}

/* Extract a JSON integer value for a given key */
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
    char *end = NULL;
    errno = 0;
    long long value = strtoll(pos, &end, 10);
    /* strtoll saturates at LLONG_MAX/MIN and sets ERANGE; without this an
     * out-of-range expectation would compare equal to a saturated wire
     * value. */
    if (errno == ERANGE)
        return -1;
    /* The token must end here: "1755300000.0" is a valid JSON number but not
     * an integer, and truncating it would accept malformed metadata. */
    if (end == pos || (*end != ',' && *end != '}' && *end != ' ' && *end != '\n' && *end != '\r' && *end != '\t'))
        return -1;
    *out = value;
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
    /* atoi cannot report failure: "1.0" would read as 1 and a malformed
     * algorithm or version would pass as valid metadata. */
    long long wide = 0;
    if (json_get_long(json, key, &wide) != 0 || wide < INT_MIN || wide > INT_MAX)
        return -1;
    *out = (int) wide;
    return 0;
}

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

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <bin_file> <json_file>\n", argv[0]);
        return 2;
    }

    /* Read binary file */
    FILE *bf = fopen(argv[1], "rb");
    if (!bf) {
        perror(argv[1]);
        return 2;
    }
    uint8_t bin_data[MAX_FILE_SIZE];
    size_t bin_size = fread(bin_data, 1, MAX_FILE_SIZE, bf);
    fclose(bf);

    /* Read JSON file */
    FILE *jf = fopen(argv[2], "r");
    if (!jf) {
        perror(argv[2]);
        return 2;
    }
    char json[MAX_JSON_SIZE];
    size_t json_size = fread(json, 1, MAX_JSON_SIZE - 1, jf);
    json[json_size] = '\0';
    fclose(jf);

    /* Get expected version */
    int expected_version = 0;
    json_get_int(json, "version", &expected_version);
    printf("Verifying: %s (version %d, %zu bytes)\n", argv[1], expected_version, bin_size);

    /* Detect version in binary */
    int detected_version = jeefs_header_detect_version(bin_data, bin_size);
    check_int("detected_version", detected_version, expected_version);

    /* Verify CRC */
    int crc_ok = jeefs_header_verify_crc(bin_data, bin_size);
    if (crc_ok != 0) {
        fprintf(stderr, "  FAIL: CRC verification failed\n");
        failures++;
    } else {
        printf("  OK: CRC32 valid\n");
    }

    /* Check header size */
    /* Every check below reads fixed offsets. A file shorter than the header
     * it claims to be has nothing there: refuse it before the first read
     * rather than inspecting bytes that were never loaded. */
    int claimed_size = jeefs_header_size(detected_version);
    if (claimed_size < 0) {
        /* No recognisable header: there are no fields to look at, and the
         * offsets below would read whatever the buffer happens to hold. */
        fprintf(stderr, "  FAIL: no JEEFS header to verify\n");
        printf("\nResult: %d failure(s)\n", failures + 1);
        return 1;
    }
    if (bin_size < (size_t) claimed_size) {
        fprintf(stderr, "  FAIL: file is %zu bytes, too short for a %d-byte v%d header\n", bin_size, claimed_size,
                detected_version);
        printf("\nResult: 1 failure(s)\n");
        return 1;
    }

    int expected_size = 0;
    json_get_int(json, "header_size", &expected_size);
    check_int("header_size", jeefs_header_size(detected_version), expected_size);

    /* Verify individual fields based on version */
    char expected_str[256];
    const JEEPROMHeaderversion *hdr_ver = (const JEEPROMHeaderversion *) bin_data;
    (void) hdr_ver;

    /* Common string fields across all versions (at same offsets) */
    /* boardname at offset 12, boardversion at 44, serial at 76, usid at 108, cpuid at 140, mac at 172 */
    if (json_get_string(json, "boardname", expected_str, sizeof(expected_str)) == 0) {
        check_string("boardname", (const char *) (bin_data + 12), expected_str);
    }
    if (json_get_string(json, "boardversion", expected_str, sizeof(expected_str)) == 0) {
        check_string("boardversion", (const char *) (bin_data + 44), expected_str);
    }
    /* v4 names the serial slot board_serial (same offset) */
    const char *serial_key = (detected_version == 4) ? "board_serial" : "serial";
    if (json_get_string(json, serial_key, expected_str, sizeof(expected_str)) == 0) {
        check_bounded(serial_key, bin_data + 76, 32, expected_str);
    }
    if (json_get_string(json, "usid", expected_str, sizeof(expected_str)) == 0) {
        check_bounded("usid", bin_data + 108, 32, expected_str);
    }
    if (json_get_string(json, "cpuid", expected_str, sizeof(expected_str)) == 0) {
        check_bounded("cpuid", bin_data + 140, 32, expected_str);
    }
    if (json_get_string(json, "mac", expected_str, sizeof(expected_str)) == 0) {
        check_mac("mac", bin_data + 172, expected_str);
    }

    if (detected_version == 3 || detected_version == 4) {
        int expected_sig_ver = 0;
        if (json_get_int(json, "signature_version", &expected_sig_ver) != 0 &&
            json_key_present(json, "signature_version")) {
            fprintf(stderr, "  FAIL: signature_version is present but not an integer\n");
            failures++;
        } else if (json_get_int(json, "signature_version", &expected_sig_ver) == 0) {
            check_int("signature_version", bin_data[9], expected_sig_ver);
        }

        /* The signature field is 64 bytes whatever the algorithm puts in
         * it: a shorter signature is zero-padded to the end, and "no
         * signature" means the whole field is zero. Checking only the
         * populated prefix would let a generator leave anything behind it. */
        /* One char of slack past the 128 a full field needs, so an
         * over-long value is seen as over-long instead of being silently
         * truncated into something that fits. */
        char sig_hex[130] = {0};
        unsigned char expected_sig[64] = {0};
        size_t expected_len = 0;
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
                    if (!isxdigit((unsigned char) sig_hex[i * 2]) || !isxdigit((unsigned char) sig_hex[i * 2 + 1])) {
                        fprintf(stderr, "  FAIL: signature_hex not hex\n");
                        failures++;
                        expected_len = 0;
                        break;
                    }
                    unsigned byte;
                    sscanf(sig_hex + i * 2, "%2x", &byte);
                    expected_sig[i] = (unsigned char) byte;
                }
            }
        }
        /* The declared algorithm fixes the length; a vector that supplies a
         * different one is malformed however well the bytes match. */
        int want_len = expected_signature_size(bin_data[9]);
        if (want_len < 0) {
            fprintf(stderr, "  FAIL: unknown signature_version %u\n", bin_data[9]);
            failures++;
        } else if ((size_t) want_len != expected_len) {
            fprintf(stderr, "  FAIL: signature_version %u wants %d bytes, vector supplies %zu\n", bin_data[9], want_len,
                    expected_len);
            failures++;
        } else if (memcmp(bin_data + 180, expected_sig, expected_len) != 0) {
            fprintf(stderr, "  FAIL: signature mismatch\n");
            failures++;
        } else {
            int tail_ok = 1;
            for (size_t i = expected_len; i < 64; i++)
                if (bin_data[180 + i] != 0)
                    tail_ok = 0;
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
            int64_t actual_ts = (int64_t) raw_ts;
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
