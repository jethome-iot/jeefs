
# find zlib as required
find_package(ZLIB REQUIRED)

add_library(jeefs_header STATIC
        jeefs_header.c
        ../include/jeefs.h
        ../include/jeefs_header.h
)
target_link_libraries(jeefs_header ZLIB::ZLIB)

add_library(jeefsstatic STATIC
    jeefs.c
        ../include/eepromerr.h
        ../include/debug.h
        ../include/jeefs.h
)

add_library(jeefs SHARED
        jeefs.c
        ../include/eepromerr.h
        ../include/debug.h
        ../include/jeefs.h
)

set_target_properties(jeefsstatic PROPERTIES OUTPUT_NAME jeefs)

if(JEEFS_USE_EEPROMOPS_MEMORY)
    # Link against eepromops-memory
    # link to static library  eepromops-memory
    target_link_libraries(jeefsstatic  eepromops-memory)
    target_link_libraries(jeefs  eepromops-memory)
else()
    # Link against eepromops-file
    # TODO: add target_link_libraries(JeefsApp eepromops-file)
endif()

# The FS layer builds its header operations on the pure header library.
target_link_libraries(jeefs jeefs_header)
target_link_libraries(jeefsstatic jeefs_header)


target_link_libraries(jeefs ZLIB::ZLIB)
target_link_libraries(jeefsstatic ZLIB::ZLIB)

# --- Header-only C++ wrappers ---

# Pure header parsing (HeaderView/HeaderBuffer over jeefs_header).
# jeefs_SOURCE_DIR (not CMAKE_SOURCE_DIR) keeps the include path correct
# when this repository is consumed via add_subdirectory().
add_library(jeefs_headerpp INTERFACE)
target_include_directories(jeefs_headerpp INTERFACE ${jeefs_SOURCE_DIR}/include)
target_link_libraries(jeefs_headerpp INTERFACE jeefs_header ZLIB::ZLIB)

# File system operations (jeefs::FileSystem over the C FS API)
add_library(jeefspp INTERFACE)
target_include_directories(jeefspp INTERFACE ${jeefs_SOURCE_DIR}/include)
target_link_libraries(jeefspp INTERFACE jeefs)
