# libFuzzer harnesses (#25 made the FS API a pure buffer function — the
# ideal fuzz target). Requires Clang; JEEFS_FUZZ_DRIVER builds standalone
# corpus runners instead when libFuzzer is unavailable.

set(FUZZ_TARGETS fuzz_fs fuzz_header fuzz_devid)

# The core sources are compiled INTO each fuzzer so the library code gets
# fuzzer coverage instrumentation and sanitizers — linking the ordinary
# static library would leave the interesting code uninstrumented.
set(FUZZ_CORE
        ${jeefs_SOURCE_DIR}/src/jeefs.c
        ${jeefs_SOURCE_DIR}/src/jeefs_header.c
        ${jeefs_SOURCE_DIR}/src/jeefs_devid.c
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
