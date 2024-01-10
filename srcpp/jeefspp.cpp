// SPDX-License-Identifier: (GPL-2.0+ or MIT)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <adeep@lexina.in>
 */

#include <string>
#include <vector>

#include "jeefs.h"

class EEPROMFileSystem {
public:
    explicit EEPROMFileSystem(const std::string& pathname, uint16_t eeprom_size)
    {
        descriptor = EEPROM_OpenEEPROM(pathname.c_str(), eeprom_size);
    }

    ~EEPROMFileSystem()
    {
        EEPROM_CloseEEPROM(descriptor);
    }

    std::vector<std::string> ListFiles()
    {
        char fileList[MAX_FILES][FILE_NAME_LENGTH];
        int16_t fileCount = EEPROM_ListFiles(descriptor, fileList, MAX_FILES);

        std::vector<std::string> files;
        for (int i = 0; i < fileCount; ++i) {
            files.emplace_back(fileList[i]);
        }
        return files;
    }

    std::vector<uint8_t> ReadFile(const std::string& filename)
    {
        std::vector<uint8_t> buffer(MAX_FILE_SIZE);
        int16_t bytesRead = EEPROM_ReadFile(descriptor, filename.c_str(), buffer.data(), MAX_FILE_SIZE);

        if (bytesRead > 0) {
            buffer.resize(bytesRead);
        } else {
            buffer.clear();
        }
        return buffer;
    }

    int16_t WriteFile(const std::string& filename, const std::vector<uint8_t>& data)
    {
        return EEPROM_WriteFile(descriptor, filename.c_str(), data.data(), data.size());
    }

    int16_t AddFile(const std::string& filename, const std::vector<uint8_t>& data)
    {
        return EEPROM_AddFile(descriptor, filename.c_str(), data.data(), data.size());
    }

    int16_t DeleteFile(const std::string& filename)
    {
        return EEPROM_DeleteFile(descriptor, filename.c_str());
    }

    int16_t Defrag()
    {
        return defragEEPROM(descriptor);
    }

    int16_t CheckConsistency()
    {
        return EEPROM_HeaderCheckConsistency(descriptor);
    }

    // Additional methods for setting and getting the header, formatting the EEPROM, etc., can be added here.

private:
    EEPROMDescriptor descriptor;
    static constexpr uint16_t MAX_FILES = 100;   // Adjust as needed
    static constexpr uint16_t MAX_FILE_SIZE = 1024;  // Adjust as needed
};