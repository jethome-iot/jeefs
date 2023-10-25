// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "tests-common.h"
#include "debug.h"

int generate_files(const char *path, const char *basename, int num_files, int maxsize) {
    int i;
    char filename[100];
    char filedata[maxsize];
    FILE *fp;
    //debug("Generating path: %s basename: %s num_files: %d maxsize: %d\n", path, basename, num_files, maxsize);
    for (i = 0; i < num_files; i++) {
        sprintf(filename, "%s/%s_%d", path, basename, i);
        fp = fopen(filename, "w+");
        if (fp == NULL) {
            printf("Error opening file %s", filename);
            perror("Error opening file");
            return -1;
        }
        // generate random data
        sprintf(filedata, "Hello, file %d!", i);
        for (size_t j = strlen(filedata); j < maxsize - 2 - random() % (maxsize-maxsize/3); j++) {
            filedata[j] = (char)('a' + (char)(random() % 26));
            filedata[j + 1] = '\n';
            filedata[j + 2] = '\0';
        }

        // write data to file
        if (fwrite(filedata, sizeof(char), strlen(filedata), fp) != strlen(filedata)) {
            perror("Error writing to file");
            return -1;
        }
        fclose(fp);
    }

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
