

add_library(test-common tests-common.c)


target_link_libraries(test-common PUBLIC jeefs)

target_include_directories(test-common PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)
