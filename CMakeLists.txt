cmake_minimum_required(VERSION 3.0)
project(srun VERSION 0.0.1 LANGUAGES CXX)

option(SanitizeAddress "compile with -fsanitize=address" OFF)

include(GNUInstallDirs)
set(LIB_SYSTEMD_DIR /lib/systemd)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -fPIC -std=c++11 ")

if(SanitizeAddress)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})
execute_process(
  COMMAND ${PROTOBUF_PROTOC_EXECUTABLE} --version
  OUTPUT_VARIABLE PROTOBUF_VERSION
)
string(REGEX MATCH "([0-9.]+).*" VERSION_NUMBER ${PROTOBUF_VERSION})
if(VERSION_NUMBER VERSION_LESS "3.0.0")
  include_directories(${CMAKE_CURRENT_BINARY_DIR}/proto/proto2)
  add_subdirectory(proto/proto2)
else()
include_directories(${CMAKE_CURRENT_BINARY_DIR}/proto/proto3)
  add_subdirectory(proto/proto3)
endif()

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED OFF)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS filesystem)
include_directories(${Boost_INCLUDE_DIRS})

add_subdirectory(src)
add_subdirectory(systemd)
