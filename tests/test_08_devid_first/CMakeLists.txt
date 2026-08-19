

add_executable(test_08 test_08.c)

target_link_libraries(test_08 jeefs::fs)

add_test(test_devid_first test_08)
