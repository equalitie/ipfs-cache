cmake_minimum_required (VERSION 3.5)
set(GLOB BOOST_VERSION 1.55)
include(ExternalProject)
################################################################################
# TODO(peterj): Address sanitizer seems to cause segmentation faults on exit
#               from main(?).
#set(SANITIZE "-fsanitize=address")
################################################################################
externalproject_add(json
    # TODO: We only really need to download one header file.
    URL https://github.com/nlohmann/json/archive/v2.1.1.tar.gz
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    INSTALL_COMMAND ""
    PREFIX json)

set(JSON_DIR "${CMAKE_BINARY_DIR}/json/src/json/src")

externalproject_add(go_cache
    # TODO(peterj): Can it be done so that the .h and .a files are not
    # created "in source"?
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/golang
    BUILD_ALWAYS 1
    BUILD_IN_SOURCE 1
    BUILD_COMMAND ${MAKE}
    PREFIX go_cache
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND "")

set(GO_CACHE_DIR "${CMAKE_SOURCE_DIR}/golang")

################################################################################
project(ipfs-cache)

find_package(Boost ${BOOST_VERSION} COMPONENTS REQUIRED)
find_package(Threads)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${Boost_INCLUDE_DIR}"
    "${JSON_DIR}"
    "${GO_CACHE_DIR}"
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(ipfs-cache STATIC ${sources})

add_dependencies(ipfs-cache json go_cache)

################################################################################
project(client)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/example/client.cpp")

add_executable(client ${sources})

add_dependencies(client ipfs-cache)

target_link_libraries(client
    event
    event_pthreads
    ${CMAKE_BINARY_DIR}/libipfs-cache.a
    ${GO_CACHE_DIR}/go_ipfs_cache.a
    ${Boost_LIBRARIES})

################################################################################
project(injector)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/example/injector.cpp")

add_executable(injector ${sources})

add_dependencies(injector ipfs-cache)

target_link_libraries(injector
    event
    event_pthreads
    ${CMAKE_BINARY_DIR}/libipfs-cache.a
    ${GO_CACHE_DIR}/go_ipfs_cache.a
    ${Boost_LIBRARIES})

################################################################################
