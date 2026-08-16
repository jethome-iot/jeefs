cmake_minimum_required(VERSION 3.19)

# --- Read version from version.json ---
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/version.json" _version_json)
string(JSON JEEFS_VERSION GET "${_version_json}" "stable")

project(jeefs VERSION ${JEEFS_VERSION} LANGUAGES C CXX)

# --- Options ---
option(JEEFS_BUILD_TESTS "Build tests" ON)
option(JEEFS_USEDYNAMIC_FILES "Build the shared jeefs library in addition to the static one" ON)
option(JEEFS_INSTALL "Generate install rules and package files" ON)

# --- Compiler options ---

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(GNUInstallDirs)

# Code generation is never part of the build: the generated files are
# committed and checked against docs/format/*.md by CI and the prek hook.
# Regeneration is a manual step — see docs/CODEGEN.md.

# zlib: only tests and cross-language tools use it directly now — the
# libraries default to the built-in CRC32 (include/jeefs_port.h, #24).
find_package(ZLIB REQUIRED)

# Python is still needed by the cross-language test matrix.
find_package(Python3 COMPONENTS Interpreter)

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

# --- Install / export -------------------------------------------------------

if (JEEFS_INSTALL)
    include(CMakePackageConfigHelpers)

    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
            PATTERN "debug.h" EXCLUDE)

    install(EXPORT jeefsTargets
            NAMESPACE jeefs::
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jeefs)

    configure_package_config_file(
            cmake/jeefsConfig.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/jeefsConfig.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jeefs)
    write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/jeefsConfigVersion.cmake
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMinorVersion)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/jeefsConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/jeefsConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/jeefs)

    configure_file(cmake/jeefs.pc.in ${CMAKE_CURRENT_BINARY_DIR}/jeefs.pc @ONLY)
    configure_file(cmake/jeefs-header.pc.in ${CMAKE_CURRENT_BINARY_DIR}/jeefs-header.pc @ONLY)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/jeefs.pc
            ${CMAKE_CURRENT_BINARY_DIR}/jeefs-header.pc
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif ()
