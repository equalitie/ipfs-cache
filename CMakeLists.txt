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
set(BINDING_DIR "${CMAKE_BINARY_DIR}/ipfs_bindings")

################################################################################
project(ipfs-cache)

find_package(Boost ${BOOST_VERSION} COMPONENTS REQUIRED)
find_package(Threads)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${Boost_INCLUDE_DIR}"
    "${JSON_DIR}"
    "${BINDING_DIR}"
    "${CMAKE_SOURCE_DIR}/include")

add_custom_command(
    OUTPUT ${BINDING_DIR}/ipfs_bindings.h
           ${BINDING_DIR}/ipfs_bindings.a
    DEPENDS ${CMAKE_SOURCE_DIR}/src/ipfs_bindings.go
    COMMAND mkdir -p ${BINDING_DIR}
         && cd ${BINDING_DIR}
         && go build --buildmode=c-archive ${CMAKE_SOURCE_DIR}/src/ipfs_bindings.go)

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(ipfs-cache STATIC ${sources}
    ${BINDING_DIR}/ipfs_bindings.h
    ${BINDING_DIR}/ipfs_bindings.a)

add_dependencies(ipfs-cache json)

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
    ${BINDING_DIR}/ipfs_bindings.a
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
    ${BINDING_DIR}/ipfs_bindings.a
    ${Boost_LIBRARIES})

################################################################################
