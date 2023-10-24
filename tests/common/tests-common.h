// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_TESTS_COMMON_H
#define JEEFS_TESTS_COMMON_H

#ifndef TEST_DIR
#define TEST_DIR "/tmp"
#endif

#ifndef TEST_EEPROM_PATH
#define TEST_EEPROM_PATH "/tmp"
#endif

#ifndef TEST_EEPROM_FILENAME
#define TEST_EEPROM_FILENAME "eeprom.bin"
#endif

#ifndef TEST_EEPROM_SIZE
#define TEST_EEPROM_SIZE 8192
#endif

#ifndef TEST_DIR
#define TEST_DIR "/tmp"
#endif

#ifndef TEST_FILENAME
#define TEST_FILENAME TEST_DIR "/" "tstf"
#endif

/**
 * @brief Generate test files
 * @param path
 * @param basename
 * @param num_files
 * @param maxsize
 * @return
 */
int generate_files(const char *path, const char *basename, int num_files, int maxsize);

/**
 * @brief Delete test files
 * @param path
 * @param basename
 * @param num_files
 * @return
 */
 int delete_files(const char *path, const char *basename, int num_files);


#endif //JEEFS_TESTS_COMMON_H
