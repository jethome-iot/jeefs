
# find zlib as required
find_package(ZLIB REQUIRED)

add_library(jeefs
    jeefs.c
        ../include/err.h
        ../include/debug.h
)

if(JEEFS_USE_EEPROMOPS_MEMORY)
    # Link against eepromops-memory
    target_link_libraries(jeefs eepromops-memory)
else()
    # Link against eepromops-file
    # TODO: add target_link_libraries(JeefsApp eepromops-file)
endif()


target_link_libraries(jeefs ZLIB::ZLIB)
