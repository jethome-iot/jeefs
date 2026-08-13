

add_executable(test_00 test_00.c)

target_link_libraries(test_00 test-common)

add_test(test_format test_00)

# test_00 leaves a v1-formatted shared image for test_01
set_tests_properties(test_format PROPERTIES FIXTURES_SETUP eeprom_image)
