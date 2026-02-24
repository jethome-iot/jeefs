find_package(ZLIB REQUIRED)

# --- C example ---
add_executable(example_c_read_header c/read_header.c)
target_link_libraries(example_c_read_header jeefs_header ZLIB::ZLIB)

# --- C++ example ---
add_executable(example_cpp_read_header cpp/read_header.cpp)
target_link_libraries(example_cpp_read_header jeefs_headerpp)
target_compile_features(example_cpp_read_header PRIVATE cxx_std_17)
