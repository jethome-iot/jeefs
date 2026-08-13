

add_executable(test_04 test_04.cpp)

target_link_libraries(test_04 jeefspp jeefs_headerpp)
target_compile_features(test_04 PRIVATE cxx_std_17)

add_test(test_jeefspp test_04)
