# find zlib as required
find_package(ZLIB REQUIRED)

# Public include dir: build tree for in-tree consumers (add_subdirectory),
# the installed include dir for exported targets.
set(JEEFS_PUBLIC_INCLUDES
        $<BUILD_INTERFACE:${jeefs_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

# --- Pure header + record parsing (no I/O) ---
add_library(jeefs_header STATIC
        jeefs_header.c
        jeefs_devid.c
        ../include/jeefs.h
        ../include/jeefs_header.h
        ../include/jeefs_devid.h
)
add_library(jeefs::header ALIAS jeefs_header)
set_target_properties(jeefs_header PROPERTIES EXPORT_NAME header)
target_include_directories(jeefs_header PUBLIC ${JEEFS_PUBLIC_INCLUDES})
target_link_libraries(jeefs_header PUBLIC ZLIB::ZLIB)

# --- FS layer ---
add_library(jeefsstatic STATIC
    jeefs.c
        ../include/eepromerr.h
        ../include/debug.h
        ../include/jeefs.h
)
add_library(jeefs::fs ALIAS jeefsstatic)
set_target_properties(jeefsstatic PROPERTIES EXPORT_NAME fs)
target_include_directories(jeefsstatic PUBLIC ${JEEFS_PUBLIC_INCLUDES})
set_target_properties(jeefsstatic PROPERTIES OUTPUT_NAME jeefs)

if(JEEFS_USEDYNAMIC_FILES)
    add_library(jeefs SHARED
            jeefs.c
            ../include/eepromerr.h
            ../include/debug.h
            ../include/jeefs.h
    )
    add_library(jeefs::fs_shared ALIAS jeefs)
    set_target_properties(jeefs PROPERTIES EXPORT_NAME fs_shared)
    target_include_directories(jeefs PUBLIC ${JEEFS_PUBLIC_INCLUDES})
    set_target_properties(jeefs PROPERTIES
            VERSION ${PROJECT_VERSION}
            SOVERSION ${PROJECT_VERSION_MAJOR})
endif()

# jeefsstatic is always present; the shared build is optional
set(JEEFS_FS_TARGETS jeefsstatic)
if(JEEFS_USEDYNAMIC_FILES)
    list(APPEND JEEFS_FS_TARGETS jeefs)
endif()

foreach(tgt IN LISTS JEEFS_FS_TARGETS)
    if(JEEFS_USE_EEPROMOPS_MEMORY)
        target_link_libraries(${tgt} PUBLIC eepromops-memory)
    else()
        # TODO: eepromops-file backend
    endif()
    # The FS layer builds its header operations on the pure header library.
    target_link_libraries(${tgt} PUBLIC jeefs_header)
    target_link_libraries(${tgt} PRIVATE ZLIB::ZLIB)
endforeach()

# --- Header-only C++ wrappers ---

# Pure header parsing (HeaderView/HeaderBuffer over jeefs_header).
add_library(jeefs_headerpp INTERFACE)
add_library(jeefs::headerpp ALIAS jeefs_headerpp)
set_target_properties(jeefs_headerpp PROPERTIES EXPORT_NAME headerpp)
target_include_directories(jeefs_headerpp INTERFACE ${JEEFS_PUBLIC_INCLUDES})
target_link_libraries(jeefs_headerpp INTERFACE jeefs_header)

# File system operations (jeefs::FileSystem over the C FS API)
add_library(jeefspp INTERFACE)
add_library(jeefs::fspp ALIAS jeefspp)
set_target_properties(jeefspp PROPERTIES EXPORT_NAME fspp)
target_include_directories(jeefspp INTERFACE ${JEEFS_PUBLIC_INCLUDES})
target_link_libraries(jeefspp INTERFACE jeefsstatic)

# --- Export ---
if(JEEFS_INSTALL)
    install(TARGETS jeefs_header jeefs_headerpp jeefspp ${JEEFS_FS_TARGETS}
            EXPORT jeefsTargets
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
