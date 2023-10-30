
# find zlib as required
find_package(ZLIB REQUIRED)

add_library(jeefsstatic STATIC
    jeefs.c
        ../include/eepromerr.h
        ../include/debug.h
)

add_library(jeefs SHARED
        jeefs.c
        ../include/eepromerr.h
        ../include/debug.h
)

set_target_properties(jeefsstatic PROPERTIES OUTPUT_NAME jeefs)

if(JEEFS_USE_EEPROMOPS_MEMORY)
    # Link against eepromops-memory
    target_link_libraries(jeefs eepromops-memory)
else()
    # Link against eepromops-file
    # TODO: add target_link_libraries(JeefsApp eepromops-file)
endif()


target_link_libraries(jeefs ZLIB::ZLIB)
