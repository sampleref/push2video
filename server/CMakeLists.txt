cmake_minimum_required(VERSION 3.8)
project(PushToTalkService)
set(CMAKE_CXX_STANDARD 11)
set(CURL_LIBRARY "-lcurl")

option(WITH_TEST "Set this to ON if you want to compile with gtest" ON)

# Download and unpack googletest at configure time
if (WITH_TEST)
    configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
    if (result)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif ()
    execute_process(COMMAND ${CMAKE_COMMAND} --build .
            RESULT_VARIABLE result
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download)
    if (result)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif ()

    # Add googletest directly to our build. This defines
    # the gtest and gtest_main targets.
    add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src
            ${CMAKE_BINARY_DIR}/googletest-build)
endif ()

# Cmake find modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
find_package(CURL REQUIRED)

find_package(Protobuf REQUIRED)
find_package(GRPC REQUIRED)

# Use second file(GLOB PROTOS ..) while building for local binary and comment(#) first one
file(GLOB PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/protos/*.proto")
#file(GLOB PROTOS "${CMAKE_CURRENT_SOURCE_DIR}/../common/protos/*.proto")

set(PROTO_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/proto-src)
file(MAKE_DIRECTORY ${PROTO_SRC_DIR})

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_SRC_DIR} ${PROTOS})
grpc_generate_cpp(GRPC_SRCS GRPC_HDRS ${PROTO_SRC_DIR} ${PROTOS})

pkg_check_modules(GSTLIBS REQUIRED
        gobject-2.0
        glib-2.0
        gstreamer-webrtc-1.0
        gstreamer-sdp-1.0
        gstreamer-pbutils-1.0
        libsoup-2.4
        json-glib-1.0
        gstreamer-rtsp-server-1.0)

include_directories(
        ${CURL_INCLUDE_DIR}
        ${PROTO_SRC_DIR}
        /usr/lib/x86_64-linux-gnu/glib-2.0/include
        /usr/include/glib-2.0
        /usr/local/include/gstreamer-1.0
        /usr/local/include/libsoup-2.4
        /usr/local/include/json-glib-1.0)

if (WITH_TEST)
    include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
endif ()

file(GLOB SOURCE_FILES application/*.cpp application/*.hpp application/grpc/*.cpp application/grpc/*.hpp)

link_directories(${GSTLIBS_LIBRARY_DIRS})

add_executable(push2talk_server ${SOURCE_FILES} ${PROTO_SRCS} ${GRPC_SRCS})

target_link_libraries(push2talk_server ${GSTLIBS_LIBRARIES} gRPC::grpc++_reflection protobuf::libprotobuf ${CMAKE_THREAD_LIBS_INIT} ${CURL_LIBRARIES})