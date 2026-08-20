

add_executable(test_09 test_09.c)

target_link_libraries(test_09 jeefs::fs jeefs::header)

add_test(test_walk test_09)
