

if (NOT DEFINED TEST_EEPROM_PATH)
    set(TEST_EEPROM_PATH "/tmp")
endif()

if (NOT DEFINED TEST_EEPROM_FILENAME)
    set(TEST_EEPROM_FILENAME "eeprom.bin")
endif()

# Default EEPROM size for the test images; tests-common.h falls back to the
# same value when the define is absent.
if (NOT DEFINED TEST_EEPROM_SIZE)
    set(TEST_EEPROM_SIZE 8192)
endif()

if (DEFINED DEBUG)
    add_definitions(-DDEBUG)
endif()

# path for temp files
add_definitions(-DTEST_DIR="/tmp")
# EEPROM define
add_definitions(-DTEST_EEPROM_PATH="${TEST_EEPROM_PATH}")
add_definitions(-DTEST_EEPROM_FILENAME="${TEST_EEPROM_FILENAME}")
add_definitions(-DTEST_EEPROM_SIZE=${TEST_EEPROM_SIZE})



add_subdirectory(common)
add_subdirectory(test_00_formateeprom)
add_subdirectory(test_01_addfiles)
add_subdirectory(test_02_readfile)

add_subdirectory(test_04_jeefspp)
add_subdirectory(test_05_fscore)
add_subdirectory(test_06_freestanding)
add_subdirectory(test_07_devid)
add_subdirectory(test_08_devid_first)
add_subdirectory(cross-language)
