

add_executable(test_00 test_00.c)

target_link_libraries(test_00 test-common)

add_test(test_format test_00)
