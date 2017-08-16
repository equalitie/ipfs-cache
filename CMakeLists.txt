cmake_minimum_required (VERSION 3.5)
set(GLOB BOOST_VERSION 1.55)
include(ExternalProject)
################################################################################
externalproject_add(json
    # TODO: We only really need to download one header file.
    URL https://github.com/nlohmann/json/archive/v2.1.1.tar.gz
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    PREFIX "json")

set(JSON_DIR "${CMAKE_BINARY_DIR}/json/src/json/src")

################################################################################
project(ipfs-cache)

find_package(Boost ${BOOST_VERSION} COMPONENTS REQUIRED)
find_package(Threads)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb -fsanitize=address")

include_directories(
    "${Boost_INCLUDE_DIR}"
    "${JSON_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/golang")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(ipfs-cache ${sources})

add_dependencies(ipfs-cache json)

target_link_libraries(ipfs-cache
    event
    ${CMAKE_SOURCE_DIR}/golang/go_ipfs_cache.a)
