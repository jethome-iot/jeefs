

add_executable(test_05 test_05.c)

# test_05 compares wire CRCs against zlib directly (the LE lock)
find_package(ZLIB REQUIRED)
target_link_libraries(test_05 jeefs::fs ZLIB::ZLIB)

add_test(test_fscore test_05)
