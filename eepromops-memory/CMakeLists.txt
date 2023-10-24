# add CMakefilelist for eepromops-memory add_library

project(eepromops-memory)

# List your source files here
set(SOURCES
        eepromops-memory.c
)

# Create a static library
add_library(eepromops-memory STATIC ${SOURCES})

# If you have any include directories, you can specify them like this
target_include_directories(eepromops-memory PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

