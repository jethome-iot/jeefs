// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#ifndef JEEFS_JEEFS_H
#define JEEFS_JEEFS_H

#include <stdint.h>
#include <string>
#include <vector>

#include "eepromops.h"
#include "jeefs.h" // Include the original C header file

class EEPROMFileSystem {
public:
    explicit EEPROMFileSystem(const std::string& pathname, uint16_t eeprom_size);
    ~EEPROMFileSystem();

    std::vector<std::string> ListFiles();
    std::vector<uint8_t> ReadFile(const std::string& filename);
    int16_t WriteFile(const std::string& filename, const std::vector<uint8_t>& data);
    int16_t AddFile(const std::string& filename, const std::vector<uint8_t>& data);
    int16_t DeleteFile(const std::string& filename);
    int16_t Defrag();
    int16_t CheckConsistency();

    // Additional methods as needed

private:
    EEPROMDescriptor descriptor;
    static constexpr uint16_t MAX_FILES = 100;   // Adjust as needed
    static constexpr uint16_t MAX_FILE_SIZE = 1024;  // Adjust as needed
};

#endif //JEEFS_JEEFS_H
