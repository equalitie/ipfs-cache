cmake_minimum_required (VERSION 3.5)
################################################################################

# NOTE: from_iso_extended_string has been introduced in Boost 1.62
set(BOOST_VERSION 1.62)
include(ExternalProject)

################################################################################
project(ipfs-cache)

find_package(Threads)
find_package(Boost ${BOOST_VERSION} REQUIRED COMPONENTS
    program_options
    date_time
    coroutine
    context
    thread
    system)

# Convert system name into GOOS.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set(GOOS "linux")
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    set(GOOS "android")
else()
    message(FATAL_ERROR "unsupported system name ${CMAKE_SYSTEM_NAME}")
endif()

# Convert system processor into GOARCH (and maybe GOARM).
if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    set(GOARCH "amd64")
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "aarch64")
    set(GOARCH "arm64")
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "armv7-a")
    set(GOARCH "arm")
    set(GOARM "7")
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^arm.*")
    set(GOARCH "arm")
    set(GOARM "6")
else()
    message(FATAL_ERROR "unsupported system processor ${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Build target tag with the Android API version if relevant.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    set(TARGET "${CMAKE_SYSTEM_NAME}${CMAKE_SYSTEM_VERSION}--${CMAKE_SYSTEM_PROCESSOR}")
else()
    set(TARGET "${CMAKE_SYSTEM_NAME}--${CMAKE_SYSTEM_PROCESSOR}")
endif()

################################################################################
option(IPFS_CACHE_WITH_EXAMPLE_BINARIES "Build with example binaries" ON)

add_definitions(-DBOOST_COROUTINE_NO_DEPRECATION_WARNING
                -DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

################################################################################
option(IPFS_CACHE_WITH_EXAMPLE_BINARIES "Build with example binaries" ON)

add_definitions(-DBOOST_COROUTINE_NO_DEPRECATION_WARNING
                -DBOOST_COROUTINES_NO_DEPRECATION_WARNING)

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

# Android does not support POSIX file:
# <https://github.com/boostorg/beast/issues/800>.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBOOST_BEAST_NO_POSIX_FILE=0")
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
               && make deps
    INSTALL_COMMAND ""
    PREFIX "ipfs")

################################################################################
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

set(BINDINGS_HEADER  "${BINDING_DIR}/ipfs_bindings.h")
set(BINDINGS_LIBRARY "${BINDING_DIR}/ipfs_bindings.so")
set(BINDINGS_OUTPUT ${BINDINGS_HEADER} ${BINDINGS_LIBRARY})

add_custom_command(
    OUTPUT ${BINDINGS_OUTPUT}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/include/ipfs_cache/ipfs_error_codes.h
            ${CMAKE_CURRENT_SOURCE_DIR}/src/ipfs_bindings/ipfs_bindings.go
            ipfs
    COMMAND mkdir -p ${BINDING_DIR}
         && export GOPATH=${GOPATH}
         && export GOROOT=${GOROOT}
         && export GOOS=${GOOS}
         && export GOARCH=${GOARCH}
         && export GOARM=${GOARM}
         && export CGO_ENABLED=1
         && export CC=${CMAKE_C_COMPILER}
         && export PATH=${GOROOT}/bin:$ENV{PATH}
         && mkdir -p ${GOPATH}/src/github.com/equalitie/ipfs-cache
         && cd ${GOPATH}/src/github.com/equalitie/ipfs-cache
         && rsync -r ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src .
         && go build -ldflags '-extldflags -Wl,-soname,ipfs_bindings.so'
                     --buildmode=c-shared
                     -o ${BINDING_DIR}/ipfs_bindings.so
                     ./src/ipfs_bindings)


add_custom_target(ipfs-bindings-target DEPENDS ${BINDINGS_OUTPUT})
add_library(ipfs-bindings SHARED IMPORTED GLOBAL)
add_dependencies(ipfs-bindings ipfs-bindings-target)
set_target_properties(ipfs-bindings PROPERTIES IMPORTED_LOCATION ${BINDINGS_LIBRARY})

################################################################################
include_directories(
    "${Boost_INCLUDE_DIR}"
    "${JSON_DIR}"
    "${BINDING_DIR}"
    "./include")

file(GLOB sources
  "./src/*.h"
  "./src/*.cpp")

add_library(ipfs-cache SHARED ${sources})
set_target_properties(ipfs-cache PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(ipfs-cache ipfs-bindings)
add_dependencies(ipfs-cache json ipfs-bindings)

################################################################################
# Only set these variables if this CMakeLists.txt is included from another one.
get_directory_property(hasParent PARENT_DIRECTORY)
if(hasParent)
    set(IPFS_CACHE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include" PARENT_SCOPE)
    set(IPFS_CACHE_TARGET "ipfs-cache" PARENT_SCOPE)
    set(IPFS_CACHE_JSON_INCLUDE_DIR "${JSON_DIR}" PARENT_SCOPE)
    set(IPFS_CACHE_LIBRARIES
        ipfs-cache
        ipfs-bindings
        PARENT_SCOPE)
endif()

################################################################################
if(IPFS_CACHE_WITH_EXAMPLE_BINARIES)
################################################################################
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "./include")

set(sources "./example/client.cpp")

add_executable(cache_client ${sources})
add_dependencies(cache_client ipfs-cache)
target_link_libraries(cache_client ipfs-cache ipfs-bindings ${Boost_LIBRARIES})

################################################################################
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${BEAST_DIR}/include"
    "./include")

set(sources "./example/injector.cpp")

add_executable(cache_injector ${sources})
add_dependencies(cache_injector ipfs-cache beast)
target_link_libraries(cache_injector ipfs-cache ipfs-bindings ${Boost_LIBRARIES})

################################################################################
endif() # IPFS_CACHE_WITH_EXAMPLE_BINARIES
################################################################################
