add_executable(test_07 test_07.c)

target_compile_definitions(test_07 PRIVATE
        VECTORS_DIR="${jeefs_SOURCE_DIR}/test-vectors/vectors")

target_link_libraries(test_07 jeefs_header)

add_test(test_devid test_07)
