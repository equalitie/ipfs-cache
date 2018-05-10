include_directories(
    "${Boost_INCLUDE_DIR}"
    "../src")

add_executable(test-btree "test_btree.cpp")
target_link_libraries(test-btree ${Boost_LIBRARIES})

