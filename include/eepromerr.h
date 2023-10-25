// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_EEPROMERR_H
#define JEEFS_EEPROMERR_H


typedef enum {
    NOTHINGDO = 0,
    FILEEXISTS = -1,
    FILENAMETOOLONG = -2,
    FILENAMETOOSHORT = -3,
    FILENAMENOTVALID = -4,
    // File not found
    FILENOTFOUND = -5,
    // Not enough buffer space
    NOTENOUGHSPACE = -6,
    // File already exists
    FILEALREADYEXISTS = -7,
    // buffer not valid
    BUFFERNOTVALID = -8,
    EEPROMCORRUPTED = -10,
    EEPROMREADERROR = -11
} EEPROMError;
#endif //JEEFS_EEPROMERR_H
