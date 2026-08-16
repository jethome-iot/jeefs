// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2026 JetHome. All rights reserved.
 *
 * C++17 file system API — header-only wrapper over the C jeefs.h FS
 * functions. Header parsing lives in jeefs_headerpp.hpp (HeaderView /
 * HeaderBuffer); this wrapper returns raw header bytes and does not
 * duplicate any parsing.
 *
 * The C core works on caller-owned image buffers (#25, variant A) and
 * performs no I/O. This hosted convenience class owns the image, loads
 * it from a file, and writes the file back after every mutating call —
 * preserving the write-through behavior of the pre-buffer API.
 */

#ifndef JEEFS_JEEFSPP_HPP
#define JEEFS_JEEFSPP_HPP

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <string>
#include <vector>

extern "C" {
#include "eepromerr.h"
#include "jeefs.h"
}

namespace jeefs {

    /// Owns an EEPROM image loaded from a file and wraps the C file system
    /// API over it. Non-copyable, movable. Error codes are EEPROMError
    /// values from eepromerr.h, retained by lastError() for
    /// optional-returning calls.
    class FileSystem {
    public:
        /// Load the image from `pathname`; the file must be exactly
        /// `eeprom_size` bytes (0 = use the file size as-is).
        FileSystem(const std::string &pathname, uint16_t eeprom_size) : path_(pathname) {
            FILE *f = std::fopen(pathname.c_str(), "rb");
            if (!f)
                return;
            std::fseek(f, 0, SEEK_END);
            long fsize = std::ftell(f);
            std::fseek(f, 0, SEEK_SET);
            if (fsize <= 0 || fsize > UINT16_MAX || (eeprom_size != 0 && fsize != eeprom_size)) {
                std::fclose(f);
                return;
            }
            image_.resize(static_cast<size_t>(fsize));
            size_t got = std::fread(image_.data(), 1, image_.size(), f);
            std::fclose(f);
            if (got != image_.size())
                image_.clear();
        }

        FileSystem(const FileSystem &) = delete;
        FileSystem &operator=(const FileSystem &) = delete;
        FileSystem(FileSystem &&) noexcept = default;
        FileSystem &operator=(FileSystem &&) noexcept = default;

        /// True if the image was loaded successfully.
        bool valid() const { return !image_.empty(); }

        /// Format the image with a header of the given version and persist.
        /// 0 on success. All methods short-circuit with BUFFERNOTVALID on an
        /// invalid (unloaded) image.
        int format(int version) {
            if (!valid())
                return remember(BUFFERNOTVALID);
            int ret = EEPROM_FormatEEPROM(image_.data(), size16(), version);
            if (ret != 0)
                last_error_ = ret;
            else
                ret = flush();
            return ret;
        }

        /// File names present on the image, or nullopt on error.
        std::optional<std::vector<std::string>> listFiles(uint16_t max_files = 64) {
            if (!valid()) {
                last_error_ = BUFFERNOTVALID;
                return std::nullopt;
            }
            std::vector<char> flat(static_cast<size_t>(max_files) * (JEEFS_FILE_NAME_LENGTH + 1));
            auto *table = reinterpret_cast<char (*)[JEEFS_FILE_NAME_LENGTH + 1]>(flat.data());
            int16_t count = EEPROM_ListFiles(image_.data(), size16(), table, max_files);
            if (count < 0) {
                last_error_ = count;
                return std::nullopt;
            }
            std::vector<std::string> names;
            names.reserve(static_cast<size_t>(count));
            for (int16_t i = 0; i < count; ++i) {
                const char *p = table[i];
                names.emplace_back(p, strnlen(p, JEEFS_FILE_NAME_LENGTH));
            }
            return names;
        }

        /// File contents, or nullopt if missing / on error (see lastError()).
        std::optional<std::vector<uint8_t>> readFile(const std::string &filename) {
            if (!valid()) {
                last_error_ = BUFFERNOTVALID;
                return std::nullopt;
            }
            std::vector<uint8_t> buf(image_.size());
            int16_t n = EEPROM_ReadFile(image_.data(), size16(), filename.c_str(), buf.data(),
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
                return remember(BUFFERNOTVALID);
            // int16_t returns cannot represent byte counts above INT16_MAX, and
            // the C layer would report a wrapped count (issue #9)
            if (data.size() > INT16_MAX)
                return remember(BUFFERNOTVALID);
            return mutate(EEPROM_AddFile(image_.data(), size16(), filename.c_str(), data.data(),
                                         static_cast<uint16_t>(data.size())));
        }

        /// Overwrite an existing file. Returns written byte count; FILENOTFOUND
        /// if the file does not exist; NOTENOUGHSPACE leaves the old content
        /// intact; other negative codes on error.
        int16_t writeFile(const std::string &filename, const std::vector<uint8_t> &data) {
            if (!valid())
                return remember(BUFFERNOTVALID);
            if (data.size() > INT16_MAX)
                return remember(BUFFERNOTVALID);
            return mutate(EEPROM_WriteFile(image_.data(), size16(), filename.c_str(), data.data(),
                                           static_cast<uint16_t>(data.size())));
        }

        /// Delete a file. Returns 1 on success; FILENOTFOUND if the file does
        /// not exist; other negative on error.
        int16_t deleteFile(const std::string &filename) {
            if (!valid())
                return remember(BUFFERNOTVALID);
            return mutate(EEPROM_DeleteFile(image_.data(), size16(), filename.c_str()));
        }

        /// 1 if the header is consistent, 0 if not, negative on error.
        int16_t checkConsistency() {
            if (!valid())
                return remember(BUFFERNOTVALID);
            return remember(EEPROM_HeaderCheckConsistency(image_.data(), size16()));
        }

        /// Header bytes, exactly sized for the detected header version (parse
        /// with jeefs::HeaderView from jeefs_headerpp.hpp), or nullopt on error.
        std::optional<std::vector<uint8_t>> readHeader() {
            if (!valid()) {
                last_error_ = BUFFERNOTVALID;
                return std::nullopt;
            }
            // EEPROM_GetHeader fills exactly the detected header size and
            // rejects a smaller buffer; probing the known sizes yields an
            // exactly-sized result without parsing here. A failed probe
            // reaches last_error_ only if every probe fails.
            int ret = EEPROMCORRUPTED;
            for (size_t size: {size_t{256}, size_t{512}}) {
                if (size > image_.size())
                    break;
                std::vector<uint8_t> buf(size);
                ret = EEPROM_GetHeader(image_.data(), size16(), buf.data(), static_cast<int>(buf.size()));
                if (ret == 0)
                    return buf;
            }
            last_error_ = ret;
            return std::nullopt;
        }

        /// Write a header image (must be a valid packed header of 256/512
        /// bytes); the CRC32 is recalculated by the C layer and the file is
        /// persisted. 0 on success, negative EEPROMError otherwise.
        int setHeader(void *header) {
            if (!valid())
                return remember(BUFFERNOTVALID);
            int ret = EEPROM_SetHeader(image_.data(), size16(), header);
            if (ret < 0)
                last_error_ = ret;
            else
                ret = flush();
            return ret;
        }

        /// Last non-positive code returned by the C layer.
        int lastError() const { return last_error_; }

        /// The owned image, for direct C API calls.
        uint8_t *data() { return image_.data(); }
        const uint8_t *data() const { return image_.data(); }
        uint16_t size() const { return size16(); }

    private:
        std::string path_;
        std::vector<uint8_t> image_;
        int last_error_ = 0;

        uint16_t size16() const { return static_cast<uint16_t>(image_.size()); }

        int16_t remember(int16_t code) {
            if (code <= 0)
                last_error_ = code;
            return code;
        }

        /// Persist the image after a successful mutation (write-through).
        int flush() {
            FILE *f = std::fopen(path_.c_str(), "wb");
            if (!f) {
                last_error_ = EEPROMWRITEERROR;
                return EEPROMWRITEERROR;
            }
            size_t put = std::fwrite(image_.data(), 1, image_.size(), f);
            std::fclose(f);
            if (put != image_.size()) {
                last_error_ = EEPROMWRITEERROR;
                return EEPROMWRITEERROR;
            }
            return 0;
        }

        int16_t mutate(int16_t code) {
            remember(code);
            if (code >= 0 && flush() != 0)
                return static_cast<int16_t>(EEPROMWRITEERROR);
            return code;
        }
    };

} // namespace jeefs

#endif // JEEFS_JEEFSPP_HPP
