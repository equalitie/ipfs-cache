cmake_minimum_required (VERSION 3.5)
set(GLOB BOOST_VERSION 1.55)

################################################################################
project(_dummy)  # just to force loading cross-compilation variables

# Convert system name into xgo's platform.
if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
    set(TARGET_PLATFORM "linux")
elseif("${CMAKE_SYSTEM_NAME}" STREQUAL "Android")
    set(TARGET_PLATFORM "android")
else()
    message(FATAL_ERROR "unsupported platform ${CMAKE_SYSTEM_NAME}")
endif()

# Convert system processor into xgo's architecture.
if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    set(TARGET_ARCHITECTURE "amd64")
elseif("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "arm64")
    set(TARGET_ARCHITECTURE "arm64")
elseif("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "^arm.*")
    set(TARGET_ARCHITECTURE "arm-6")
else()
    message(FATAL_ERROR "unsupported architecture ${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(TARGET "${TARGET_PLATFORM}-${TARGET_ARCHITECTURE}")

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
set(BINDING_DIR "${CMAKE_BINARY_DIR}/ipfs_bindings-${TARGET}")

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
               && make deps
    INSTALL_COMMAND ""
    PREFIX "ipfs")

################################################################################
externalproject_add(xgo
    DOWNLOAD_COMMAND mkdir -p ${GOPATH}
                  && export GOPATH=${GOPATH}
                  && export GOROOT=${GOROOT}
                  && export PATH=$ENV{PATH}:${GOROOT}/bin
                  && go get -u -d github.com/karalabe/xgo
    BUILD_ALWAYS 1
    DEPENDS golang
    CONFIGURE_COMMAND ""
    BUILD_COMMAND export GOPATH=${GOPATH}
               && export GOROOT=${GOROOT}
               && export PATH=$ENV{PATH}:${GOROOT}/bin
               && go get github.com/karalabe/xgo
    INSTALL_COMMAND ""
    PREFIX "xgo")

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
            ${CMAKE_SOURCE_DIR}/src/ipfs_bindings/ipfs_bindings.go
    COMMAND mkdir -p ${BINDING_DIR}
         && export GOPATH=${GOPATH}
         && export GOROOT=${GOROOT}
         && export PATH=$ENV{PATH}:${GOPATH}/bin
         && mkdir -p ${GOPATH}/src/github.com/equalitie
         && cd ${GOPATH}/src/github.com/equalitie
         && ln -sfT ${CMAKE_SOURCE_DIR} ipfs-cache
         && cd ipfs-cache
         && xgo --targets=${TARGET_PLATFORM}/${TARGET_ARCHITECTURE} -buildmode=c-archive -dest ${BINDING_DIR} ./src/ipfs_bindings
         && cp -f ${BINDING_DIR}/ipfs_bindings-*.h ${BINDING_DIR}/ipfs_bindings.h
         && cp -f ${BINDING_DIR}/ipfs_bindings-*.a ${BINDING_DIR}/ipfs_bindings.a)

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.h"
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(ipfs-cache-${TARGET} STATIC ${sources}
    ${BINDING_DIR}/ipfs_bindings.h
    ${BINDING_DIR}/ipfs_bindings.a)

add_dependencies(ipfs-cache-${TARGET} json ipfs xgo)

# Append object files from  ipfs_bindings.a into libipfs-cache.a.
add_custom_command(TARGET ipfs-cache-${TARGET}
    POST_BUILD
    COMMAND cd ${BINDING_DIR}
         && ar -x ipfs_bindings.a
         && cd ${CMAKE_BINARY_DIR}
         && ar -qc libipfs-cache-${TARGET}.a ${BINDING_DIR}/*.o)

################################################################################
project(client)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options coroutine system REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/example/client.cpp")

add_executable(client-${TARGET} ${sources})

add_dependencies(client-${TARGET} ipfs-cache-${TARGET})

target_link_libraries(client-${TARGET}
    ${CMAKE_BINARY_DIR}/libipfs-cache-${TARGET}.a
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

add_executable(injector-${TARGET} ${sources})

add_dependencies(injector-${TARGET} ipfs-cache-${TARGET} beast)

target_link_libraries(injector-${TARGET}
    ${CMAKE_BINARY_DIR}/libipfs-cache-${TARGET}.a
    ${Boost_LIBRARIES})

################################################################################
