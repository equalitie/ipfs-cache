cmake_minimum_required (VERSION 3.5)
set(GLOB BOOST_VERSION 1.55)
include(ExternalProject)
################################################################################
#set(SANITIZE "-fsanitize=address")
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

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${Boost_INCLUDE_DIR}"
    "${JSON_DIR}"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/golang")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(ipfs-cache STATIC ${sources})

add_dependencies(ipfs-cache json)

################################################################################
project(example)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/example/*.cpp")

add_executable(example ${sources})

add_dependencies(example ipfs-cache)

target_link_libraries(example
    event
    event_pthreads
    ${CMAKE_BINARY_DIR}/libipfs-cache.a
    ${CMAKE_SOURCE_DIR}/golang/go_ipfs_cache.a
    ${Boost_LIBRARIES})

################################################################################
