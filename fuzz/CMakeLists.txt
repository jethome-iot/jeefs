# libFuzzer harnesses (#25 made the FS API a pure buffer function — the
# ideal fuzz target). Requires Clang; JEEFS_FUZZ_DRIVER builds standalone
# corpus runners instead when libFuzzer is unavailable.

set(FUZZ_TARGETS fuzz_fs fuzz_header fuzz_devid)

foreach(t IN LISTS FUZZ_TARGETS)
    add_executable(${t} ${t}.c)
    target_link_libraries(${t} jeefs::fs)
    if(JEEFS_FUZZ_DRIVER)
        target_compile_definitions(${t} PRIVATE JEEFS_FUZZ_DRIVER)
    else()
        target_compile_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
        target_link_options(${t} PRIVATE -fsanitize=fuzzer,address,undefined)
    endif()
endforeach()
