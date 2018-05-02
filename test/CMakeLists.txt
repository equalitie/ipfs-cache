include_directories(
    "${Boost_INCLUDE_DIR}"
    "../src")

add_executable(test-db-tree "test_db_tree.cpp" "../src/db_tree.cpp")
target_link_libraries(test-db-tree ${Boost_LIBRARIES})

