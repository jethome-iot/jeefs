find_package(ZLIB REQUIRED)

# ─── Build C verifier & generator ─────────────────────────────────────────────
add_executable(verify_c verify_c.c)
target_include_directories(verify_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(verify_c jeefs_header ZLIB::ZLIB)

add_executable(generate_c generate_c.c)
target_include_directories(generate_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(generate_c jeefs_header ZLIB::ZLIB)

# Mutation-vector runner: exercises the FS core (not just the header)
add_executable(apply_ops_c apply_ops_c.c)
target_include_directories(apply_ops_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(apply_ops_c jeefs ZLIB::ZLIB)

# ─── Build C++ verifier & generator ───────────────────────────────────────────
add_executable(verify_cpp verify_cpp.cpp)
target_link_libraries(verify_cpp jeefs_headerpp ZLIB::ZLIB)

add_executable(generate_cpp generate_cpp.cpp)
target_link_libraries(generate_cpp jeefs_headerpp ZLIB::ZLIB)

# ─── Rust verifiers & generators (built via cargo) ───────────────────────────
find_program(CARGO cargo)
if(CARGO)
    set(RUST_CRATE_DIR ${CMAKE_SOURCE_DIR}/rust/jeefs-header)
    set(RUST_TARGET_DIR ${RUST_CRATE_DIR}/target/release)
    set(VERIFY_RS ${RUST_TARGET_DIR}/verify_rs)
    set(VERIFY_GOLDEN_RS ${RUST_TARGET_DIR}/verify_golden_rs)
    set(GENERATE_RS ${RUST_TARGET_DIR}/generate_rs)
    set(APPLY_OPS_RS ${RUST_TARGET_DIR}/apply_ops_rs)

    add_custom_command(
        OUTPUT ${VERIFY_RS} ${VERIFY_GOLDEN_RS} ${GENERATE_RS} ${APPLY_OPS_RS}
        COMMAND ${CARGO} build --release --features bins --manifest-path ${RUST_CRATE_DIR}/Cargo.toml
        DEPENDS
            ${RUST_CRATE_DIR}/src/lib.rs
            ${RUST_CRATE_DIR}/src/generated.rs
            ${RUST_CRATE_DIR}/src/header.rs
            ${RUST_CRATE_DIR}/src/bin/verify_rs.rs
            ${RUST_CRATE_DIR}/src/bin/verify_golden_rs.rs
            ${RUST_CRATE_DIR}/src/bin/generate_rs.rs
            ${RUST_CRATE_DIR}/src/bin/apply_ops_rs.rs
            ${RUST_CRATE_DIR}/src/fs.rs
            ${RUST_CRATE_DIR}/Cargo.toml
        COMMENT "Building Rust verifiers and generators"
    )
    add_custom_target(rust_verifiers ALL DEPENDS ${VERIFY_RS} ${VERIFY_GOLDEN_RS} ${GENERATE_RS} ${APPLY_OPS_RS})
endif()

# ─── NxN cross-language test matrix ──────────────────────────────────────────
# Each generator creates a .bin from .json, each verifier checks it.
# Test naming: cross_{gen}_gen_{ver}_verify_{vector_stem}
#
# Generators / Verifiers:
#   c   — C executable
#   cpp — C++ executable
#   py  — Python script
#   rs  — Rust executable

set(VECTORS_DIR ${CMAKE_SOURCE_DIR}/test-vectors/vectors)
set(RUN_SCRIPT  ${CMAKE_CURRENT_SOURCE_DIR}/run_gen_verify.cmake)
set(TMPDIR      ${CMAKE_CURRENT_BINARY_DIR}/tmp)

# Generator commands (as semicolon-separated strings for CMake -P script)
set(GEN_c       $<TARGET_FILE:generate_c>)
set(GEN_cpp     $<TARGET_FILE:generate_cpp>)

# Verifier commands
set(VER_c       $<TARGET_FILE:verify_c>)
set(VER_cpp     $<TARGET_FILE:verify_cpp>)

# Assemble language lists
set(LANGUAGES c cpp)

if(Python3_FOUND)
    list(APPEND LANGUAGES py)
    set(GEN_py  "${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/generate_py.py")
    set(VER_py  "${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/verify_py.py")
endif()

if(CARGO)
    list(APPEND LANGUAGES rs)
    set(GEN_rs  ${GENERATE_RS})
    set(VER_rs  ${VERIFY_RS})
endif()

file(GLOB VECTOR_JSONS "${VECTORS_DIR}/*.json")
# The devid_* record vectors have their own layout and per-language tests —
# the header generator/verifier matrix skips them.
list(FILTER VECTOR_JSONS EXCLUDE REGEX "devid_")
foreach(json_file ${VECTOR_JSONS})
    get_filename_component(stem ${json_file} NAME_WE)

    foreach(gen_lang ${LANGUAGES})
        foreach(ver_lang ${LANGUAGES})
            set(test_name cross_${gen_lang}_gen_${ver_lang}_verify_${stem})
            set(tmpbin ${TMPDIR}/${gen_lang}_${stem}.bin)

            add_test(
                NAME ${test_name}
                COMMAND ${CMAKE_COMMAND}
                    -DGENERATOR=${GEN_${gen_lang}}
                    -DVERIFIER=${VER_${ver_lang}}
                    -DJSON=${json_file}
                    -DTMPBIN=${tmpbin}
                    -P ${RUN_SCRIPT}
            )
        endforeach()
    endforeach()
endforeach()

# ─── Golden reference EEPROM image tests ─────────────────────────────────────
add_executable(verify_golden_c verify_golden_c.c)
target_include_directories(verify_golden_c PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(verify_golden_c jeefs_header ZLIB::ZLIB)

add_executable(verify_golden_cpp verify_golden_cpp.cpp)
target_link_libraries(verify_golden_cpp jeefs_headerpp ZLIB::ZLIB)

set(GOLDEN_BIN ${CMAKE_SOURCE_DIR}/test-vectors/reference/eeprom_full.bin)
set(GOLDEN_JSON ${CMAKE_SOURCE_DIR}/test-vectors/reference/eeprom_full.json)
set(GOLDEN_BIN_V4 ${CMAKE_SOURCE_DIR}/test-vectors/reference/eeprom_full_v4.bin)
set(GOLDEN_JSON_V4 ${CMAKE_SOURCE_DIR}/test-vectors/reference/eeprom_full_v4.json)

add_test(
    NAME golden_c_verify
    COMMAND verify_golden_c ${GOLDEN_BIN}
)

add_test(
    NAME golden_c_verify_v4
    COMMAND verify_golden_c ${GOLDEN_BIN_V4} 4
)

add_test(
    NAME golden_cpp_verify
    COMMAND verify_golden_cpp ${GOLDEN_BIN}
)

add_test(
    NAME golden_cpp_verify_v4
    COMMAND verify_golden_cpp ${GOLDEN_BIN_V4} 4
)

if(Python3_FOUND)
    add_test(
        NAME golden_py_verify
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/verify_golden_py.py
            ${GOLDEN_BIN} ${GOLDEN_JSON}
    )

    add_test(
        NAME golden_py_verify_v4
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/verify_golden_py.py
            ${GOLDEN_BIN_V4} ${GOLDEN_JSON_V4}
    )

    # The public image API must reproduce the committed goldens
    # byte-for-byte and parse them back (one format implementation).
    add_test(
        NAME image_build_py_matches_golden
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/verify_image_build_py.py
            ${GOLDEN_BIN} ${GOLDEN_JSON}
    )

    add_test(
        NAME image_build_py_matches_golden_v4
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/verify_image_build_py.py
            ${GOLDEN_BIN_V4} ${GOLDEN_JSON_V4}
    )

    # The committed test-vectors/vectors/*.bin must match their .json
    # descriptions — they exist for cross-commit stability (#19).
    add_test(
        NAME committed_vectors_check
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_SOURCE_DIR}/test-vectors/generate_vectors.py --check
    )
endif()

if(CARGO)
    add_test(
        NAME fs_mutation_c_matches_rs
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/verify_fs_mutation.py
            $<TARGET_FILE:apply_ops_c> ${APPLY_OPS_RS}
            ${CMAKE_CURRENT_SOURCE_DIR}/fs_vectors
            ${CMAKE_CURRENT_BINARY_DIR}/fs_mutation
            --random 50
    )

    add_test(
        NAME image_build_rs_matches_golden
        COMMAND ${CARGO} run --quiet --manifest-path ${CMAKE_SOURCE_DIR}/rust/jeefs-header/Cargo.toml
            --features bins --bin verify_image_build_rs -- ${GOLDEN_BIN} ${GOLDEN_JSON}
    )

    add_test(
        NAME image_build_rs_matches_golden_v4
        COMMAND ${CARGO} run --quiet --manifest-path ${CMAKE_SOURCE_DIR}/rust/jeefs-header/Cargo.toml
            --features bins --bin verify_image_build_rs -- ${GOLDEN_BIN_V4} ${GOLDEN_JSON_V4}
    )

    add_test(
        NAME golden_rs_verify
        COMMAND ${VERIFY_GOLDEN_RS} ${GOLDEN_BIN}
    )

    add_test(
        NAME golden_rs_verify_v4
        COMMAND ${VERIFY_GOLDEN_RS} ${GOLDEN_BIN_V4} 4
    )
endif()
