

add_executable(test_02 test_02.c)
target_link_libraries(test_02 test-common)
add_test(test_02 test_02)

set_tests_properties(test_02 PROPERTIES FIXTURES_REQUIRED eeprom_files)
