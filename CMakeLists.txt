cmake_minimum_required(VERSION 3.14)

project(jeefs C CXX)

# --- Options ---
option(JEEFS_BUILD_TESTS "Build tests" ON)
option(JEEFS_USEDYNAMIC_FILES "Use dynamic files" ON)

option(JEEFS_USE_EEPROMOPS_MEMORY "Use libeepromops with memory driver" ON)

# --- Compiler options ---

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)


# include sub-libraries
add_subdirectory(eepromops-memory)
# TODO: add_subdirectory(eepromops-file)


include_directories(include)
add_subdirectory(src)
add_subdirectory(srcpp)

if (JEEFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif (JEEFS_BUILD_TESTS)

