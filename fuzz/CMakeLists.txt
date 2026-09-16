# libFuzzer harnesses (#25 made the FS API a pure buffer function — the
# ideal fuzz target). Requires Clang; JEEFS_FUZZ_DRIVER builds standalone
# corpus runners instead when libFuzzer is unavailable.

set(FUZZ_TARGETS fuzz_fs fuzz_header fuzz_devid)

# The differential target also drives the Rust port, through a C-ABI shim
# built as a static library. Without cargo it simply is not built — the other
# fuzzers do not depend on it.
find_program(CARGO_FOR_FUZZ cargo)

# The core sources are compiled INTO each fuzzer so the library code gets
# fuzzer coverage instrumentation and sanitizers — linking the ordinary
# static library would leave the interesting code uninstrumented.
set(FUZZ_CORE
        ${jeefs_SOURCE_DIR}/src/jeefs.c
        ${jeefs_SOURCE_DIR}/src/jeefs_header.c
        ${jeefs_SOURCE_DIR}/src/jeefs_devid.c
        ${jeefs_SOURCE_DIR}/src/jeefs_walk.c
        ${jeefs_SOURCE_DIR}/src/jeefs_crc32.c)

foreach(t IN LISTS FUZZ_TARGETS)
    add_executable(${t} ${t}.c ${FUZZ_CORE})
    target_include_directories(${t} PRIVATE ${jeefs_SOURCE_DIR}/include)
    if(JEEFS_FUZZ_DRIVER)
        target_compile_definitions(${t} PRIVATE JEEFS_FUZZ_DRIVER)
    else()
        target_compile_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
        target_link_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
endforeach()

# ─── Differential C vs Rust target ───────────────────────────────────────────
if(CARGO_FOR_FUZZ)
    set(FFI_DIR ${jeefs_SOURCE_DIR}/rust/jeefs-fuzz-ffi)

    # Without coverage instrumentation on the Rust side, libFuzzer would steer
    # by the C core alone and a branch only the Rust port has could never guide
    # the search. The flags go to a target-specific build so they do not reach
    # build scripts, which are compiled for the host and would fail with them.
    execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine OUTPUT_VARIABLE FFI_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(COMMAND rustc -vV OUTPUT_VARIABLE RUSTC_VV)
    string(REGEX MATCH "host: ([^
]+)" _ "${RUSTC_VV}")
    set(FFI_TRIPLE ${CMAKE_MATCH_1})
    set(FFI_LIB ${FFI_DIR}/target/${FFI_TRIPLE}/release/libjeefs_fuzz_ffi.a)
    set(FFI_RUSTFLAGS "-Cpasses=sancov-module -Cllvm-args=-sanitizer-coverage-level=4 -Cllvm-args=-sanitizer-coverage-inline-8bit-counters -Cllvm-args=-sanitizer-coverage-pc-table -Cllvm-args=-sanitizer-coverage-trace-compares")

    add_custom_command(
        OUTPUT ${FFI_LIB}
        COMMAND ${CMAKE_COMMAND} -E env RUSTFLAGS=${FFI_RUSTFLAGS}
                ${CARGO_FOR_FUZZ} build --release --target ${FFI_TRIPLE} --manifest-path ${FFI_DIR}/Cargo.toml
        DEPENDS
            ${FFI_DIR}/src/lib.rs
            ${FFI_DIR}/Cargo.toml
            ${jeefs_SOURCE_DIR}/rust/jeefs-header/src/fs.rs
            ${jeefs_SOURCE_DIR}/rust/jeefs-header/src/lib.rs
        COMMENT "Building the instrumented Rust FS shim for the differential fuzzer"
    )
    add_custom_target(jeefs_fuzz_ffi DEPENDS ${FFI_LIB})

    add_executable(fuzz_fs_diff fuzz_fs_diff.c ${FUZZ_CORE})
    add_dependencies(fuzz_fs_diff jeefs_fuzz_ffi)
    target_include_directories(fuzz_fs_diff PRIVATE ${jeefs_SOURCE_DIR}/include)
    target_link_libraries(fuzz_fs_diff ${FFI_LIB})
    if(JEEFS_FUZZ_DRIVER)
        # Corpus runner: no libFuzzer runtime needed, which is how this target
        # stays buildable on hosts whose clang does not ship one.
        target_compile_definitions(fuzz_fs_diff PRIVATE JEEFS_FUZZ_DRIVER)
    else()
        target_compile_options(fuzz_fs_diff PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
        target_link_options(fuzz_fs_diff PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
endif()
