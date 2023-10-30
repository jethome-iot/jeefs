
# find zlib as required
find_package(ZLIB REQUIRED)

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


target_link_libraries(jeefs ZLIB::ZLIB)
target_link_libraries(jeefsstatic ZLIB::ZLIB)
