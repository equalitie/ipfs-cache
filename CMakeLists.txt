cmake_minimum_required (VERSION 3.5)
set(GLOB BOOST_VERSION 1.55)
include(ExternalProject)
################################################################################
option(IPFS_CACHE_WITH_EXAMPLE_BINARIES "Build with example binaries" ON)

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

set(JSON_DIR "${CMAKE_CURRENT_BINARY_DIR}/json/src/json/src")
set(BINDING_DIR "${CMAKE_CURRENT_BINARY_DIR}/ipfs_bindings")

################################################################################
if(NOT TARGET beast)
    externalproject_add(beast
      GIT_REPOSITORY https://github.com/inetic/beast
      UPDATE_COMMAND ""
      CONFIGURE_COMMAND ""
      INSTALL_COMMAND ""
      BUILD_COMMAND ""
      PREFIX "beast")

    set(BEAST_DIR "${CMAKE_CURRENT_BINARY_DIR}/beast/src/beast")
endif()

################################################################################
externalproject_add(golang
    URL https://storage.googleapis.com/golang/go1.9.1.linux-amd64.tar.gz
    BUILD_COMMAND ""
    UPDATE_COMMAND ""
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    PREFIX golang)

set(GOROOT "${CMAKE_CURRENT_BINARY_DIR}/golang/src/golang")
set(GOPATH "${CMAKE_CURRENT_BINARY_DIR}/gopath")

################################################################################
# IPFS releases
set(IPFS_V0_4_12 "8eb7956622ee1a340398d5e1a00ab5f7588cb23d")
set(IPFS_V0_4_13 "cc01b7f188622e7148ce041b9d09252c85041d9f")

set(IPFS_GO_COMMIT ${IPFS_V0_4_13})

externalproject_add(ipfs
    DOWNLOAD_COMMAND mkdir -p ${GOPATH}
                  && export GOPATH=${GOPATH}
                  && export GOROOT=${GOROOT}
                  && export PATH=${GOROOT}/bin:$ENV{PATH}
                  && go get -u -d github.com/ipfs/go-ipfs
                  && cd ${GOPATH}/src/github.com/ipfs/go-ipfs
                  && git checkout ${IPFS_GO_COMMIT}
    BUILD_ALWAYS 1
    DEPENDS golang
    CONFIGURE_COMMAND ""
    BUILD_COMMAND export GOPATH=${GOPATH}
               && export GOROOT=${GOROOT}
               && export PATH=${GOROOT}/bin:$ENV{PATH}
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
    "./include")

add_custom_command(
    OUTPUT ${BINDING_DIR}/ipfs_bindings.h
           ${BINDING_DIR}/ipfs_bindings.a
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/ipfs_cache/ipfs_error_codes.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ipfs_bindings.go
    COMMAND mkdir -p ${BINDING_DIR}
         && cd ${BINDING_DIR}
         && export GOPATH=${GOPATH}
         && export GOROOT=${GOROOT}
         && export PATH=${GOROOT}/bin:$ENV{PATH}
         && go build --buildmode=c-archive ${CMAKE_CURRENT_SOURCE_DIR}/src/ipfs_bindings.go)

file(GLOB sources
  "./src/*.h"
  "./src/*.cpp")

add_library(ipfs-cache STATIC ${sources}
    ${BINDING_DIR}/ipfs_bindings.h
    ${BINDING_DIR}/ipfs_bindings.a)

add_dependencies(ipfs-cache json ipfs)

# Append object files from  ipfs_bindings.a into libipfs-cache.a.
add_custom_command(TARGET ipfs-cache
    POST_BUILD
    COMMAND cd ${BINDING_DIR}
         && ar -x ipfs_bindings.a
         && cd ${CMAKE_CURRENT_BINARY_DIR}
         && ar -qc libipfs-cache.a ${BINDING_DIR}/*.o)

################################################################################
set(IPFS_CACHE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)
set(IPFS_CACHE_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/libipfs-cache.a" PARENT_SCOPE)
set(IPFS_CACHE_JSON_INCLUDE_DIR "${JSON_DIR}" PARENT_SCOPE)

################################################################################
if(IPFS_CACHE_WITH_EXAMPLE_BINARIES)
################################################################################
project(cache_client)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options coroutine system REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "./include")

set(sources
  "./example/client.cpp")

add_executable(cache_client ${sources})

add_dependencies(cache_client ipfs-cache)

target_link_libraries(cache_client
    ${CMAKE_CURRENT_BINARY_DIR}/libipfs-cache.a
    ${Boost_LIBRARIES})

################################################################################
project(cache_injector)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options coroutine system REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${BEAST_DIR}/include"
    "./include")

set(sources
  "./example/injector.cpp")

add_executable(cache_injector ${sources})

add_dependencies(cache_injector ipfs-cache beast)

target_link_libraries(cache_injector
    ${CMAKE_CURRENT_BINARY_DIR}/libipfs-cache.a
    ${Boost_LIBRARIES})

################################################################################
endif() # IPFS_CACHE_WITH_EXAMPLE_BINARIES
################################################################################
