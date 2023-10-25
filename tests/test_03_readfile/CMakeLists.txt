

add_executable(test_03 test_03.c)

target_link_libraries(test_03 test-common)

add_test(test_03 test_03)
