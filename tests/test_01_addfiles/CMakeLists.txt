

add_executable(test_01 test_01.c)

target_link_libraries(test_01 test-common)

add_test(test_01 test_01)
