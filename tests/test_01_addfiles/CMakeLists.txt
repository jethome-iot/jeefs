

add_executable(test_01 test_01.c)

target_link_libraries(test_01 test-common)

add_test(test_01 test_01)

# consumes the image from test_00, leaves it populated for test_02
set_tests_properties(test_01 PROPERTIES
    FIXTURES_REQUIRED eeprom_image
    FIXTURES_SETUP eeprom_files)
