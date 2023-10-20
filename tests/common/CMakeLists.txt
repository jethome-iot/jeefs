
if (NOT DEFINED TEST_EEPROM_PATH)
    set(TEST_EEPROM_PATH "./eeprom.bin")
endif()

if (NOT DEFINED TEST_EEPROM_SIZE)
    set(TEST_EEPROM_SIZE 8192)
endif()


add_definitions(-DTEST_EEPROM_PATH="${TEST_EEPROM_PATH}")
add_definitions(-DTEST_DIR="./")
add_definitions(-DTEST_FILENAME="test_eefs")
add_definitions(-DJEEFS_EEPROMSIZE=${TEST_EEPROM_SIZE})





add_executable(test_eefs test_eefs.c)
target_link_libraries(test_eefs jeefs)
add_test(test_eefs test_eefs)
