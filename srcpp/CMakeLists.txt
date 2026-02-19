
# find zlib as required
find_package(ZLIB REQUIRED)

add_library(jeefspp SHARED
        jeefspp.cpp
        ../include/jeefspp.hpp
        ../include/eepromerr.h
        ../include/debug.h
        ../include/jeefs.h

)

#add_library(jeefsppstatic STATIC
#        jeefspp.cpp
#)

#set_target_properties(jeefsppstatic PROPERTIES OUTPUT_NAME jeefspp)

set_target_properties(jeefsstatic PROPERTIES OUTPUT_NAME jeefs)

if(JEEFS_USE_EEPROMOPS_MEMORY)
    # Link against eepromops-memory
    # link to static library  eepromops-memory
    target_link_libraries(jeefspp eepromops-memory)
else()
    # Link against eepromops-file
    # TODO: add target_link_libraries(JeefsApp eepromops-file)
endif()

target_link_libraries(jeefspp jeefs)

target_link_libraries(jeefspp ZLIB::ZLIB)

# Header-only C++ wrapper for pure header API (no FS dependency)
add_library(jeefs_headerpp INTERFACE)
target_include_directories(jeefs_headerpp INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(jeefs_headerpp INTERFACE jeefs_header ZLIB::ZLIB)
