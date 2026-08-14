
# Guards the "no I/O dependency" promise of the pure header API.
add_test(NAME header_freestanding
         COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/check_freestanding.sh
                 ${CMAKE_C_COMPILER} ${CMAKE_SOURCE_DIR}/include)
