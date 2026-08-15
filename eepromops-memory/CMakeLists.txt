# In-memory EEPROM backend (POSIX file mirror of an in-RAM block list)

add_library(eepromops-memory STATIC
        eepromops-memory.c
)
add_library(jeefs::eepromops_memory ALIAS eepromops-memory)
set_target_properties(eepromops-memory PROPERTIES EXPORT_NAME eepromops_memory)
target_include_directories(eepromops-memory PUBLIC
        $<BUILD_INTERFACE:${jeefs_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(JEEFS_INSTALL)
    install(TARGETS eepromops-memory
            EXPORT jeefsTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
endif()
