
# find zlib as required
find_package(ZLIB REQUIRED)


add_library(jeefs
    jeefs.c
        eepromops.c
        ../include/err.h
        ../include/debug.h
)

target_link_libraries(jeefs ZLIB::ZLIB)
