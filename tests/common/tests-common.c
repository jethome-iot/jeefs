// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 */

#include "tests-common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"

int generate_files(const char *path, const char *basename, int num_files, int maxsize) {
    int i;
    char filename[100];
    char *filedata = malloc((size_t) maxsize);
    FILE *fp;
    if (filedata == NULL)
        return -1;
    // debug("Generating path: %s basename: %s num_files: %d maxsize: %d\n", path, basename, num_files, maxsize);
    for (i = 0; i < num_files; i++) {
        sprintf(filename, "%s/%s_%d", path, basename, i);
        fp = fopen(filename, "w+");
        if (fp == NULL) {
            printf("Error opening file %s", filename);
            perror("Error opening file");
            free(filedata);
            return -1;
        }
        // generate random data
        sprintf(filedata, "Hello, file %d!", i);
        size_t limit = (size_t) (maxsize - 2 - random() % (maxsize - maxsize / 3));
        for (size_t j = strlen(filedata); j < limit; j++) {
            filedata[j] = (char) ('a' + (char) (random() % 26));
            filedata[j + 1] = '\n';
            filedata[j + 2] = '\0';
        }

        // write data to file
        if (fwrite(filedata, sizeof(char), strlen(filedata), fp) != strlen(filedata)) {
            perror("Error writing to file");
            fclose(fp);
            free(filedata);
            return -1;
        }
        fclose(fp);
    }

    free(filedata);
    return 0;
}

int delete_files(const char *path, const char *basename, int num_files) {
    int i;
    char filename[100];
    for (i = 0; i < num_files; i++) {
        sprintf(filename, "%s/%s_%d", path, basename, i);
        if (remove(filename) != 0) {
            perror("Error deleting file");
            return -1;
        }
    }

    return 0;
}

int image_load(const char *path, uint8_t *buf, uint16_t size) {
    FILE *fp = fopen(path, "rb");
    if (fp == NULL)
        return -1;
    size_t got = fread(buf, 1, size, fp);
    fclose(fp);
    return got == size ? 0 : -1;
}

int image_save(const char *path, const uint8_t *buf, uint16_t size) {
    FILE *fp = fopen(path, "wb");
    if (fp == NULL)
        return -1;
    size_t put = fwrite(buf, 1, size, fp);
    fclose(fp);
    return put == size ? 0 : -1;
}
