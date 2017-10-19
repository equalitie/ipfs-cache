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
externalproject_add(beast
  GIT_REPOSITORY https://github.com/inetic/beast
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_COMMAND ""
  PREFIX "beast")

set(BEAST_DIR "${CMAKE_BINARY_DIR}/beast/src/beast")

################################################################################
externalproject_add(golang
    URL https://storage.googleapis.com/golang/go1.9.1.linux-amd64.tar.gz
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    PREFIX golang)

set(GOROOT "${CMAKE_BINARY_DIR}/golang/src/golang")
set(GOPATH "${CMAKE_BINARY_DIR}/gopath")

################################################################################
externalproject_add(ipfs
    DOWNLOAD_COMMAND mkdir -p ${GOPATH}
                  && export GOPATH=${GOPATH}
                  && export GOROOT=${GOROOT}
                  && export PATH=$ENV{PATH}:${GOROOT}/bin
                  && go get -u -d github.com/ipfs/go-ipfs
    BUILD_ALWAYS 1
    DEPENDS golang
    CONFIGURE_COMMAND ""
    BUILD_COMMAND export GOPATH=${GOPATH}
               && export GOROOT=${GOROOT}
               && export PATH=$ENV{PATH}:${GOROOT}/bin
               && cd ${GOPATH}/src/github.com/ipfs/go-ipfs
               && make install
    INSTALL_COMMAND ""
    PREFIX "ipfs")

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
    DEPENDS ${CMAKE_SOURCE_DIR}/include/ipfs_cache/ipfs_error_codes.h
            ${CMAKE_SOURCE_DIR}/src/go/ipfs_bindings.go
    COMMAND mkdir -p ${BINDING_DIR}
         && cd ${BINDING_DIR}
         && export GOPATH=${GOPATH}
         && export GOROOT=${GOROOT}
         && export PATH=$ENV{PATH}:${GOROOT}/bin
         && go build --buildmode=c-archive ${CMAKE_SOURCE_DIR}/src/go/ipfs_bindings.go)

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.h"
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(ipfs-cache STATIC ${sources}
    ${BINDING_DIR}/ipfs_bindings.h
    ${BINDING_DIR}/ipfs_bindings.a)

add_dependencies(ipfs-cache json ipfs)

# Append object files from  ipfs_bindings.a into libipfs-cache.a.
add_custom_command(TARGET ipfs-cache
    POST_BUILD
    COMMAND cd ${BINDING_DIR}
         && ar -x ipfs_bindings.a
         && cd ${CMAKE_BINARY_DIR}
         && ar -qc libipfs-cache.a ${BINDING_DIR}/*.o)

################################################################################
project(client)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options coroutine system REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/example/client.cpp")

add_executable(client ${sources})

add_dependencies(client ipfs-cache)

target_link_libraries(client
    ${CMAKE_BINARY_DIR}/libipfs-cache.a
    ${Boost_LIBRARIES})

################################################################################
project(injector)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options coroutine system REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${BEAST_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/example/injector.cpp")

add_executable(injector ${sources})

add_dependencies(injector ipfs-cache beast)

target_link_libraries(injector
    ${CMAKE_BINARY_DIR}/libipfs-cache.a
    ${Boost_LIBRARIES})

################################################################################
