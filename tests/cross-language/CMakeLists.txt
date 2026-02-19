find_package(ZLIB REQUIRED)

# --- C verifiers ---
add_executable(verify_c verify_c.c)
target_include_directories(verify_c PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(verify_c jeefs_header ZLIB::ZLIB)

# --- C++ verifiers ---
add_executable(verify_cpp verify_cpp.cpp)
target_link_libraries(verify_cpp jeefs_headerpp ZLIB::ZLIB)

# --- Rust verifiers (built via cargo) ---
find_program(CARGO cargo)
if(CARGO)
    set(RUST_CRATE_DIR ${CMAKE_SOURCE_DIR}/rust/jeefs-header)
    set(RUST_TARGET_DIR ${RUST_CRATE_DIR}/target/release)
    set(VERIFY_RS ${RUST_TARGET_DIR}/verify_rs)
    set(VERIFY_GOLDEN_RS ${RUST_TARGET_DIR}/verify_golden_rs)

    add_custom_command(
        OUTPUT ${VERIFY_RS} ${VERIFY_GOLDEN_RS}
        COMMAND ${CARGO} build --release --manifest-path ${RUST_CRATE_DIR}/Cargo.toml
        DEPENDS
            ${RUST_CRATE_DIR}/src/lib.rs
            ${RUST_CRATE_DIR}/src/generated.rs
            ${RUST_CRATE_DIR}/src/header.rs
            ${RUST_CRATE_DIR}/src/bin/verify_rs.rs
            ${RUST_CRATE_DIR}/src/bin/verify_golden_rs.rs
            ${RUST_CRATE_DIR}/Cargo.toml
        COMMENT "Building Rust verifiers"
    )
    add_custom_target(rust_verifiers ALL DEPENDS ${VERIFY_RS} ${VERIFY_GOLDEN_RS})
endif()

# Register cross-language tests: Python-generated .bin verified by C / C++ / Python / Rust
set(VECTORS_DIR ${CMAKE_SOURCE_DIR}/test-vectors/vectors)

file(GLOB VECTOR_JSONS "${VECTORS_DIR}/*.json")
foreach(json_file ${VECTOR_JSONS})
    get_filename_component(stem ${json_file} NAME_WE)
    set(bin_file "${VECTORS_DIR}/${stem}.bin")

    # C verifies Python-generated binary
    add_test(
        NAME cross_c_verify_${stem}
        COMMAND verify_c ${bin_file} ${json_file}
    )

    # C++ verifies Python-generated binary
    add_test(
        NAME cross_cpp_verify_${stem}
        COMMAND verify_cpp ${bin_file} ${json_file}
    )

    # Python verifies Python-generated binary
    if(Python3_FOUND)
        add_test(
            NAME cross_py_verify_${stem}
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/verify_py.py
                ${bin_file} ${json_file}
        )
    endif()

    # Rust verifies Python-generated binary
    if(CARGO)
        add_test(
            NAME cross_rs_verify_${stem}
            COMMAND ${VERIFY_RS} ${bin_file} ${json_file}
        )
    endif()
endforeach()

# Golden reference EEPROM image tests
add_executable(verify_golden_c verify_golden_c.c)
target_include_directories(verify_golden_c PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(verify_golden_c jeefs_header ZLIB::ZLIB)

add_executable(verify_golden_cpp verify_golden_cpp.cpp)
target_link_libraries(verify_golden_cpp jeefs_headerpp ZLIB::ZLIB)

set(GOLDEN_BIN ${CMAKE_SOURCE_DIR}/test-vectors/reference/eeprom_full.bin)
set(GOLDEN_JSON ${CMAKE_SOURCE_DIR}/test-vectors/reference/eeprom_full.json)

add_test(
    NAME golden_c_verify
    COMMAND verify_golden_c ${GOLDEN_BIN}
)

add_test(
    NAME golden_cpp_verify
    COMMAND verify_golden_cpp ${GOLDEN_BIN}
)

if(Python3_FOUND)
    add_test(
        NAME golden_py_verify
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/verify_golden_py.py
            ${GOLDEN_BIN} ${GOLDEN_JSON}
    )
endif()

if(CARGO)
    add_test(
        NAME golden_rs_verify
        COMMAND ${VERIFY_GOLDEN_RS} ${GOLDEN_BIN}
    )
endif()
