

add_executable(test_eefs test_eefs.c
        tests-common.c
        tests-common.h)
target_link_libraries(test_eefs jeefs)
add_test(test_eefs test_eefs)
