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


# --- Code generation from format specs ---
find_package(Python3 COMPONENTS Interpreter)

if(Python3_FOUND)
    set(FORMAT_SPECS
        ${CMAKE_SOURCE_DIR}/docs/format/header-common.md
        ${CMAKE_SOURCE_DIR}/docs/format/header-v1.md
        ${CMAKE_SOURCE_DIR}/docs/format/header-v2.md
        ${CMAKE_SOURCE_DIR}/docs/format/header-v3.md
        ${CMAKE_SOURCE_DIR}/docs/format/filesystem-v1.md
    )

    set(CODEGEN_SOURCES
        ${CMAKE_SOURCE_DIR}/tools/jeefs_codegen/__main__.py
        ${CMAKE_SOURCE_DIR}/tools/jeefs_codegen/parser.py
        ${CMAKE_SOURCE_DIR}/tools/jeefs_codegen/validator.py
        ${CMAKE_SOURCE_DIR}/tools/jeefs_codegen/models.py
        ${CMAKE_SOURCE_DIR}/tools/jeefs_codegen/generators/c_generator.py
        ${CMAKE_SOURCE_DIR}/tools/jeefs_codegen/generators/python_generator.py
    )

    add_custom_command(
        OUTPUT ${CMAKE_SOURCE_DIR}/include/jeefs_generated.h
        COMMAND ${Python3_EXECUTABLE} -m jeefs_codegen
                --specs ${FORMAT_SPECS}
                --c-output ${CMAKE_SOURCE_DIR}/include/jeefs_generated.h
        DEPENDS ${FORMAT_SPECS} ${CODEGEN_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tools
        COMMENT "Generating jeefs_generated.h from format specs"
    )

    add_custom_target(codegen DEPENDS
        ${CMAKE_SOURCE_DIR}/include/jeefs_generated.h
    )
endif()

include_directories(include)
add_subdirectory(src)
add_subdirectory(srcpp)

option(JEEFS_BUILD_EXAMPLES "Build examples" ON)

if (JEEFS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif (JEEFS_BUILD_TESTS)

if (JEEFS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif (JEEFS_BUILD_EXAMPLES)

