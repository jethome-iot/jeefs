// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * C++17 file system API — header-only wrapper over the C jeefs.h FS
 * functions. Header parsing lives in jeefs_headerpp.hpp (HeaderView /
 * HeaderBuffer); this wrapper returns raw header bytes and does not
 * duplicate any parsing.
 */

#ifndef JEEFS_JEEFSPP_HPP
#define JEEFS_JEEFSPP_HPP

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include "eepromerr.h"
#include "jeefs.h"
}

namespace jeefs {

/// RAII wrapper around an open EEPROM and the C file system API.
/// Non-copyable, movable. Error codes are EEPROMError values from
/// eepromerr.h, retained by lastError() for optional-returning calls.
class FileSystem {
public:
    FileSystem(const std::string &pathname, uint16_t eeprom_size)
        : desc_(EEPROM_OpenEEPROM(pathname.c_str(), eeprom_size)) {}

    ~FileSystem() {
        if (valid())
            EEPROM_CloseEEPROM(desc_);
    }

    FileSystem(const FileSystem &) = delete;
    FileSystem &operator=(const FileSystem &) = delete;

    FileSystem(FileSystem &&other) noexcept : desc_(other.desc_), last_error_(other.last_error_) {
        other.desc_.eeprom_fid = -1;
    }

    FileSystem &operator=(FileSystem &&other) noexcept {
        if (this != &other) {
            if (valid())
                EEPROM_CloseEEPROM(desc_);
            desc_ = other.desc_;
            last_error_ = other.last_error_;
            other.desc_.eeprom_fid = -1;
        }
        return *this;
    }

    /// True if the underlying EEPROM was opened successfully.
    bool valid() const { return desc_.eeprom_fid != -1; }

    /// Format the EEPROM with a header of the given version. 0 on success.
    /// All methods short-circuit with EEPROMREADERROR on an invalid
    /// descriptor: a failed open leaves eeprom_size uninitialized, and the
    /// C layer sizes buffers from it.
    int format(int version) {
        if (!valid())
            return remember(EEPROMREADERROR);
        int ret = EEPROM_FormatEEPROM(desc_, version);
        if (ret != 0)
            last_error_ = ret;
        return ret;
    }

    /// File names present on the EEPROM, or nullopt on error.
    std::optional<std::vector<std::string>> listFiles(uint16_t max_files = 64) {
        if (!valid()) {
            last_error_ = EEPROMREADERROR;
            return std::nullopt;
        }
        std::vector<char> flat(static_cast<size_t>(max_files) *
                               (FILE_NAME_LENGTH + 1));
        auto *table = reinterpret_cast<char(*)[FILE_NAME_LENGTH + 1]>(flat.data());
        int16_t count = EEPROM_ListFiles(desc_, table, max_files);
        if (count < 0) {
            last_error_ = count;
            return std::nullopt;
        }
        std::vector<std::string> names;
        names.reserve(static_cast<size_t>(count));
        for (int16_t i = 0; i < count; ++i) {
            const char *p = table[i];
            names.emplace_back(p, strnlen(p, FILE_NAME_LENGTH));
        }
        return names;
    }

    /// File contents, or nullopt if missing / on error (see lastError()).
    std::optional<std::vector<uint8_t>> readFile(const std::string &filename) {
        if (!valid()) {
            last_error_ = EEPROMREADERROR;
            return std::nullopt;
        }
        // eeprom_size is not initialized by a failed open — guarded above
        std::vector<uint8_t> buf(desc_.eeprom_size);
        int16_t n = EEPROM_ReadFile(desc_, filename.c_str(), buf.data(),
                                    static_cast<uint16_t>(buf.size()));
        if (n <= 0) {
            last_error_ = n;
            return std::nullopt;
        }
        buf.resize(static_cast<size_t>(n));
        return buf;
    }

    /// Create a new file. Returns written byte count; 0 if the file already
    /// exists; negative EEPROMError (FILENAMENOTVALID, NOTENOUGHSPACE, ...)
    /// on error.
    int16_t addFile(const std::string &filename, const std::vector<uint8_t> &data) {
        if (!valid())
            return remember(EEPROMREADERROR);
        // int16_t returns cannot represent byte counts above INT16_MAX, and
        // the C layer would report a wrapped count (issue #9)
        if (data.size() > INT16_MAX)
            return remember(BUFFERNOTVALID);
        return remember(EEPROM_AddFile(desc_, filename.c_str(), data.data(),
                                       static_cast<uint16_t>(data.size())));
    }

    /// Overwrite an existing file. Returns written byte count; FILENOTFOUND
    /// if the file does not exist; NOTENOUGHSPACE leaves the old content
    /// intact; other negative codes on error.
    int16_t writeFile(const std::string &filename, const std::vector<uint8_t> &data) {
        if (!valid())
            return remember(EEPROMREADERROR);
        if (data.size() > INT16_MAX)
            return remember(BUFFERNOTVALID);
        return remember(EEPROM_WriteFile(desc_, filename.c_str(), data.data(),
                                         static_cast<uint16_t>(data.size())));
    }

    /// Delete a file. Returns 1 on success; FILENOTFOUND if the file does
    /// not exist; other negative on error.
    int16_t deleteFile(const std::string &filename) {
        if (!valid())
            return remember(EEPROMREADERROR);
        return remember(EEPROM_DeleteFile(desc_, filename.c_str()));
    }

    /// 1 if the header is consistent, 0 if not, negative on read error.
    int16_t checkConsistency() {
        if (!valid())
            return remember(EEPROMREADERROR);
        return remember(EEPROM_HeaderCheckConsistency(desc_));
    }

    /// Header bytes, exactly sized for the detected header version (parse
    /// with jeefs::HeaderView from jeefs_headerpp.hpp), or nullopt on error.
    std::optional<std::vector<uint8_t>> readHeader() {
        if (!valid()) {
            last_error_ = EEPROMREADERROR;
            return std::nullopt;
        }
        // EEPROM_GetHeader reads the full caller-supplied size, so probe the
        // known header sizes instead of over-reading past a 256-byte header:
        // it rejects a buffer smaller than the detected header. A failed
        // probe reaches last_error_ only if every probe fails.
        int ret = EEPROMREADERROR;
        for (size_t size : {size_t{256}, size_t{512}}) {
            if (size > desc_.eeprom_size)
                break;
            std::vector<uint8_t> buf(size);
            ret = EEPROM_GetHeader(desc_, buf.data(), static_cast<int>(buf.size()));
            if (ret == 0)
                return buf;
        }
        last_error_ = ret;
        return std::nullopt;
    }

    /// Write a header image (must be a valid packed header of 256/512
    /// bytes); the CRC32 is recalculated by the C layer. 0 on success,
    /// negative EEPROMError otherwise.
    int setHeader(void *header) {
        if (!valid())
            return remember(EEPROMREADERROR);
        int ret = EEPROM_SetHeader(desc_, header);
        if (ret < 0)
            last_error_ = ret;
        return ret;
    }

    /// Last non-positive code returned by the C layer.
    int lastError() const { return last_error_; }

    /// Underlying descriptor for direct C API calls.
    EEPROMDescriptor descriptor() const { return desc_; }

private:
    EEPROMDescriptor desc_;
    int last_error_ = 0;

    int16_t remember(int16_t code) {
        if (code <= 0)
            last_error_ = code;
        return code;
    }
};

} // namespace jeefs

#endif // JEEFS_JEEFSPP_HPP
