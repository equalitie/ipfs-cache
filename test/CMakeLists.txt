include_directories(
    "${Boost_INCLUDE_DIR}"
    "${JSON_DIR}"
    "../src")

add_executable(test-btree "test_btree.cpp" "../src/btree.cpp")
target_link_libraries(test-btree ${Boost_LIBRARIES})

