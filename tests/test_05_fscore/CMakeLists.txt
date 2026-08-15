

add_executable(test_05 test_05.c)

target_link_libraries(test_05 jeefs::fs)

add_test(test_fscore test_05)
