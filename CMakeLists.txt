cmake_minimum_required(VERSION 3.19)

# --- Read version from version.json ---
file(READ "${CMAKE_SOURCE_DIR}/version.json" _version_json)
string(JSON JEEFS_VERSION GET "${_version_json}" "stable")

project(jeefs VERSION ${JEEFS_VERSION} LANGUAGES C CXX)

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


# Code generation is never part of the build: the generated files are
# committed and checked against docs/format/*.md by CI and the prek hook.
# Regeneration is a manual step — see docs/CODEGEN.md.

# Python is still needed by the cross-language test matrix.
find_package(Python3 COMPONENTS Interpreter)

include_directories(include)
add_subdirectory(src)

# Enable examples by default only when this is the top-level project
if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    option(JEEFS_BUILD_EXAMPLES "Build examples" ON)
else ()
    option(JEEFS_BUILD_EXAMPLES "Build examples" OFF)
endif ()

if (JEEFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif (JEEFS_BUILD_TESTS)

if (JEEFS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif (JEEFS_BUILD_EXAMPLES)

