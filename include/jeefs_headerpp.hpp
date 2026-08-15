// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Copyright (c) 2023 JetHome. All rights reserved.
 * Author: Viacheslav Bocharov <v@baodeep.com>
 *
 * C++17 header parsing API — wraps C jeefs_header.h functions.
 * Non-owning HeaderView + owning HeaderBuffer.
 */

#ifndef JEEFS_HEADERPP_HPP
#define JEEFS_HEADERPP_HPP

#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

extern "C" {
#include "jeefs_devid.h"
#include "jeefs_generated.h"
#include "jeefs_header.h"
}

namespace jeefs {

    /// Non-owning, read-only view of a raw EEPROM header buffer.
    class HeaderView {
    public:
        explicit HeaderView(const uint8_t *data, size_t size) : data_(data), size_(size) {}
        explicit HeaderView(const std::vector<uint8_t> &v) : data_(v.data()), size_(v.size()) {}

        /// Detect header version (1, 2, or 3). Returns nullopt on bad magic / too short.
        std::optional<int> detect_version() const {
            int v = jeefs_header_detect_version(data_, size_);
            return v >= 0 ? std::optional(v) : std::nullopt;
        }

        /// Get expected header size for the detected version, or -1.
        int header_size() const {
            auto v = detect_version();
            return v ? jeefs_header_size(*v) : -1;
        }

        /// Verify stored CRC32 against calculated.
        bool verify_crc() const { return jeefs_header_verify_crc(data_, size_) == 0; }

        /// Board name (null-terminated string at offset 12, common to all versions).
        std::string_view boardname() const { return string_at(12, 32); }

        /// Board version (null-terminated string at offset 44).
        std::string_view boardversion() const { return string_at(44, 32); }

        /// Board serial (bounded string at offset 76: all 32 bytes usable, RFC #13).
        std::string_view serial() const { return string_at(76, 32); }

        /// The v4 name of the same slot (header v4 renames serial to board_serial).
        std::string_view board_serial() const { return string_at(76, 32); }

        /// CPU eFuse USID (bounded string at offset 108, RFC #13).
        std::string_view usid() const { return string_at(108, 32); }

        /// CPU ID (bounded string at offset 140, RFC #13).
        std::string_view cpuid() const { return string_at(140, 32); }

        /// MAC address pointer (6 raw bytes at offset 172). Returns nullptr if buffer too short.
        const uint8_t *mac() const {
            if (172 + 6 > size_)
                return nullptr;
            return data_ + 172;
        }

        /// Direct access: version-detect struct.
        const JEEPROMHeaderversion &as_version_header() const {
            return *reinterpret_cast<const JEEPROMHeaderversion *>(data_);
        }

        /// Direct access: v1 header (caller must check version first).
        const JEEPROMHeaderv1 &as_v1() const { return *reinterpret_cast<const JEEPROMHeaderv1 *>(data_); }

        /// Direct access: v2 header.
        const JEEPROMHeaderv2 &as_v2() const { return *reinterpret_cast<const JEEPROMHeaderv2 *>(data_); }

        /// Direct access: v3 header.
        const JEEPROMHeaderv3 &as_v3() const { return *reinterpret_cast<const JEEPROMHeaderv3 *>(data_); }

        /// Direct access: v4 header (board-scoped; layout identical to v3).
        const JEEPROMHeaderv4 &as_v4() const { return *reinterpret_cast<const JEEPROMHeaderv4 *>(data_); }

        /// Raw data pointer.
        const uint8_t *data() const { return data_; }

        /// Buffer size.
        size_t size() const { return size_; }

    private:
        const uint8_t *data_;
        size_t size_;

        std::string_view string_at(size_t offset, size_t max_len) const {
            if (offset + max_len > size_)
                return {};
            const char *p = reinterpret_cast<const char *>(data_ + offset);
            size_t len = strnlen(p, max_len);
            return {p, len};
        }
    };

    /// Owning header buffer with mutable operations.
    class HeaderBuffer {
    public:
        /// Create and initialize a header for the given version.
        explicit HeaderBuffer(int version) {
            int sz = jeefs_header_size(version);
            if (sz > 0) {
                buf_.resize(static_cast<size_t>(sz));
                jeefs_header_init(buf_.data(), buf_.size(), version);
            }
        }

        /// Create from existing data (copies).
        explicit HeaderBuffer(const uint8_t *data, size_t size) : buf_(data, data + size) {}
        explicit HeaderBuffer(const std::vector<uint8_t> &v) : buf_(v) {}

        /// Recalculate and write CRC32 into the buffer.
        bool update_crc() { return jeefs_header_update_crc(buf_.data(), buf_.size()) == 0; }

        /// Get a read-only view.
        HeaderView view() const { return HeaderView(buf_); }

        /// Mutable access to v3 struct (caller must ensure version == 3).
        JEEPROMHeaderv3 &as_v3() { return *reinterpret_cast<JEEPROMHeaderv3 *>(buf_.data()); }

        /// Mutable access to v4 struct (caller must ensure version == 4).
        JEEPROMHeaderv4 &as_v4() { return *reinterpret_cast<JEEPROMHeaderv4 *>(buf_.data()); }

        /// Mutable access to v2 struct.
        JEEPROMHeaderv2 &as_v2() { return *reinterpret_cast<JEEPROMHeaderv2 *>(buf_.data()); }

        /// Mutable access to v1 struct.
        JEEPROMHeaderv1 &as_v1() { return *reinterpret_cast<JEEPROMHeaderv1 *>(buf_.data()); }

        /// Raw mutable data.
        uint8_t *data() { return buf_.data(); }

        /// Raw const data.
        const uint8_t *data() const { return buf_.data(); }

        /// Size of the buffer.
        size_t size() const { return buf_.size(); }

        /// Check if buffer is valid (non-empty).
        bool valid() const { return !buf_.empty(); }

    private:
        std::vector<uint8_t> buf_;
    };

    /// Non-owning, read-only view of a DeviceIdentityV1 record buffer.
    class DeviceIdView {
    public:
        explicit DeviceIdView(const uint8_t *data, size_t size) : data_(data), size_(size) {}
        explicit DeviceIdView(const std::vector<uint8_t> &v) : data_(v.data()), size_(v.size()) {}

        /// Record version (currently 1), or nullopt when this is no record.
        std::optional<int> detect() const {
            int v = jeefs_devid_detect(data_, size_);
            return v >= 0 ? std::optional(v) : std::nullopt;
        }

        /// Verify the stored CRC32.
        bool verify_crc() const { return jeefs_devid_verify_crc(data_, size_) == 0; }

        /// Device model name (bounded string at offset 12).
        std::string_view device_model() const { return string_at(12, 32); }

        /// Device serial number (bounded string at offset 44).
        std::string_view device_serial() const { return string_at(44, 32); }

        /// Hardware revision, e.g. "1.2a" (bounded string at offset 76).
        std::string_view hw_revision() const { return string_at(76, 16); }

        /// Direct access (caller must check detect() first).
        const DeviceIdentityV1 &record() const { return *reinterpret_cast<const DeviceIdentityV1 *>(data_); }

        const uint8_t *data() const { return data_; }
        size_t size() const { return size_; }

    private:
        const uint8_t *data_;
        size_t size_;

        std::string_view string_at(size_t offset, size_t max_len) const {
            if (offset + max_len > size_)
                return {};
            const char *p = reinterpret_cast<const char *>(data_ + offset);
            size_t len = strnlen(p, max_len);
            return {p, len};
        }
    };

    /// Owning DeviceIdentityV1 buffer with mutable operations.
    class DeviceIdBuffer {
    public:
        /// Create an initialized empty record.
        DeviceIdBuffer() : buf_(sizeof(DeviceIdentityV1)) { jeefs_devid_init(buf_.data(), buf_.size()); }

        /// Create from existing data (copies).
        explicit DeviceIdBuffer(const uint8_t *data, size_t size) : buf_(data, data + size) {}
        explicit DeviceIdBuffer(const std::vector<uint8_t> &v) : buf_(v) {}

        /// Recalculate and store the CRC32.
        bool update_crc() { return jeefs_devid_update_crc(buf_.data(), buf_.size()) == 0; }

        /// Read-only view.
        DeviceIdView view() const { return DeviceIdView(buf_); }

        /// Mutable record access.
        DeviceIdentityV1 &record() { return *reinterpret_cast<DeviceIdentityV1 *>(buf_.data()); }

        uint8_t *data() { return buf_.data(); }
        const uint8_t *data() const { return buf_.data(); }
        size_t size() const { return buf_.size(); }
        bool valid() const { return !buf_.empty(); }

    private:
        std::vector<uint8_t> buf_;
    };

} // namespace jeefs

#endif // JEEFS_HEADERPP_HPP
