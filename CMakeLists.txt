cmake_minimum_required (VERSION 3.5)
set(GLOB BOOST_VERSION 1.55)

################################################################################
project(_dummy)  # just to force loading cross-compilation variables

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
set(BINDING_DIR "${CMAKE_BINARY_DIR}/ipfs_bindings--${TARGET}")

################################################################################
externalproject_add(beast
  GIT_REPOSITORY https://github.com/inetic/beast
  UPDATE_COMMAND ""
  CONFIGURE_COMMAND ""
  INSTALL_COMMAND ""
  BUILD_COMMAND ""
  PREFIX "beast")

set(BEAST_DIR "${CMAKE_BINARY_DIR}/beast/src/beast")

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

set(GOROOT "${CMAKE_BINARY_DIR}/golang/src/golang")
set(GOPATH "${CMAKE_BINARY_DIR}/gopath")

################################################################################
externalproject_add(ipfs
    DOWNLOAD_COMMAND mkdir -p ${GOPATH}
                  && export GOPATH=${GOPATH}
                  && export GOROOT=${GOROOT}
                  && export PATH=${GOROOT}/bin:$ENV{PATH}
                  && go get -u -d github.com/ipfs/go-ipfs
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
           ${CMAKE_BINARY_DIR}/libipfs-bindings--${TARGET}.so
    DEPENDS ${CMAKE_SOURCE_DIR}/include/ipfs_cache/ipfs_error_codes.h
            ${CMAKE_SOURCE_DIR}/src/ipfs_bindings/ipfs_bindings.go
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
         && rsync -r ${CMAKE_SOURCE_DIR}/include ${CMAKE_SOURCE_DIR}/src .
         && go build --buildmode=c-shared -o ${BINDING_DIR}/ipfs_bindings.so ./src/ipfs_bindings
         && cp -f ${BINDING_DIR}/ipfs_bindings.so ${CMAKE_BINARY_DIR}/libipfs-bindings--${TARGET}.so
         && chmod +x ${CMAKE_BINARY_DIR}/libipfs-bindings--${TARGET}.so)

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/src/*.h"
  "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_library(ipfs-cache--${TARGET} SHARED ${sources}
    ${BINDING_DIR}/ipfs_bindings.h
    ${CMAKE_BINARY_DIR}/libipfs-bindings--${TARGET}.so)

add_dependencies(ipfs-cache--${TARGET} json ipfs)

################################################################################
project(client)

find_package(Boost ${BOOST_VERSION} COMPONENTS program_options coroutine system REQUIRED)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -pthread -Wall -ggdb ${SANITIZE}")

include_directories(
    "${CMAKE_SOURCE_DIR}/include")

file(GLOB sources
  "${CMAKE_SOURCE_DIR}/example/client.cpp")

add_executable(client--${TARGET} ${sources})

add_dependencies(client--${TARGET} ipfs-cache--${TARGET})

target_link_libraries(client--${TARGET}
    ${CMAKE_BINARY_DIR}/libipfs-bindings--${TARGET}.so
    ${CMAKE_BINARY_DIR}/libipfs-cache--${TARGET}.so
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

add_executable(injector--${TARGET} ${sources})

add_dependencies(injector--${TARGET} ipfs-cache--${TARGET} beast)

target_link_libraries(injector--${TARGET}
    ${CMAKE_BINARY_DIR}/libipfs-bindings--${TARGET}.so
    ${CMAKE_BINARY_DIR}/libipfs-cache--${TARGET}.so
    ${Boost_LIBRARIES})

################################################################################
