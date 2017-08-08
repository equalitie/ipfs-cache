cmake_minimum_required (VERSION 2.8)
set(GLOB BOOST_VERSION 1.55)
################################################################################
project(ipfs-cache)

find_package(Boost ${BOOST_VERSION} COMPONENTS REQUIRED)
find_package(Threads)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb")

include_directories(
    "${Boost_INCLUDE_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/golang")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(ipfs-cache ${sources})
target_link_libraries(ipfs-cache event ${CMAKE_SOURCE_DIR}/golang/go_ipfs_cache.a)
