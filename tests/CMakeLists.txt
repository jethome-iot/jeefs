

if (NOT DEFINED TEST_EEPROM_PATH)
    set(TEST_EEPROM_PATH ".")
endif()

if (NOT DEFINED TEST_EEPROM_FILENAME)
    set(TEST_EEPROM_FILENAME "eeprom.bin")
endif()

if (NOT DEFINED TEST_EEPROM_SIZE)
    set(TEST_EEPROM_SIZE 0)
endif()


# path for temp files
add_definitions(-DTEST_DIR=".")
# EEPROM define
add_definitions(-DTEST_EEPROM_PATH="${TEST_EEPROM_PATH}")
add_definitions(-DTEST_EEPROM_FILENAME="${TEST_EEPROM_FILENAME}")
add_definitions(-DTEST_EEPROMSIZE=${TEST_EEPROM_SIZE})



add_subdirectory(common)
