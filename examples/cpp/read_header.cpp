// SPDX-License-Identifier: (GPL-2.0+ or Apache-2.0)
/*
 * Example: Read EEPROM header, print version and MAC address.
 *
 * Usage: ./read_header <eeprom.bin>
 */

#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#include "jeefs_headerpp.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <eeprom.bin>" << std::endl;
        return 1;
    }

    // Read file
    std::ifstream file(argv[1], std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error: cannot open " << argv[1] << std::endl;
        return 1;
    }

    auto size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    file.read(reinterpret_cast<char *>(buf.data()), size);

    // Create a read-only view
    jeefs::HeaderView view(buf);

    // Detect version
    auto version = view.detect_version();
    if (!version) {
        std::cerr << "Error: invalid EEPROM header (bad magic or too short)" << std::endl;
        return 1;
    }
    std::cout << "Header version: " << *version << std::endl;

    // Check buffer is large enough for this version's struct
    int expected = jeefs_header_size(*version);
    if (expected < 0 || buf.size() < static_cast<size_t>(expected)) {
        std::cerr << "Error: file too short for v" << *version
                  << " header (" << buf.size() << " < " << expected
                  << " bytes)" << std::endl;
        return 1;
    }

    // Verify CRC
    if (view.verify_crc()) {
        std::cout << "CRC32: OK" << std::endl;
    } else {
        std::cerr << "Warning: CRC32 mismatch" << std::endl;
    }

    // Access fields
    std::cout << "Board name: " << view.boardname() << std::endl;

    const uint8_t *mac = view.mac();
    if (mac) {
        std::printf("MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3],
                     mac[4], mac[5]);
    }

    return 0;
}
