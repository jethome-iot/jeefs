

if (NOT DEFINED TEST_EEPROM_PATH)
    set(TEST_EEPROM_PATH "/tmp")
endif()

if (NOT DEFINED TEST_EEPROM_FILENAME)
    set(TEST_EEPROM_FILENAME "eeprom.bin")
endif()

if (NOT DEFINED TEST_EEPROM_SIZE)
    set(TEST_EEPROM_SIZE 0)
endif()

if (DEFINED DEBUG)
    add_definitions(-DDEBUG)
endif()

# path for temp files
add_definitions(-DTEST_DIR="/tmp")
# EEPROM define
add_definitions(-DTEST_EEPROM_PATH="${TEST_EEPROM_PATH}")
add_definitions(-DTEST_EEPROM_FILENAME="${TEST_EEPROM_FILENAME}")
add_definitions(-DTEST_EEPROMSIZE=${TEST_EEPROM_SIZE})



add_subdirectory(common)
add_subdirectory(test_00_formateeprom)
add_subdirectory(test_01_addfiles)
add_subdirectory(test_02_readfile)
add_subdirectory(test_03_readfile)
