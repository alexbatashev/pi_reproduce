cmake_minimum_required(VERSION 3.14)

set(DPCPP_TRACE_VERSION_MAJOR 0)
set(DPCPP_TRACE_VERSION_MINOR 1)
set(DPCPP_TRACE_VERSION_PATCH 0)
set(DPCPP_TRACE_VERSION "${DPCPP_TRACE_VERSION_MAJOR}.${DPCPP_TRACE_VERSION_MINOR}.${DPCPP_TRACE_VERSION_PATCH}")

option(BUILD_DEBUGGER "Build dpcpp_trace debug server, client and plugins" OFF)
option(DPCPP_TRACE_LINK_STATICALLY "Use static libraries" OFF)

project(dpcpp_trace VERSION ${DPCPP_TRACE_VERSION})

if (NOT INTEL_LLVM_SOURCE_PATH)
  message(FATAL_ERROR "INTEL_LLVM_SOURCE_PATH variable is not set!")
endif()

if (NOT INTEL_LLVM_BINARY_PATH)
  message(FATAL_ERROR "INTEL_LLVM_BINARY_PATH variable is not set!")
endif()

set(CMAKE_CXX_STANDARD 20)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FetchContent)

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.7.3)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/fmt ${PROJECT_BINARY_DIR}/fmt)

find_package(Protobuf REQUIRED)
find_package(folly REQUIRED)
find_package(ZLIB REQUIRED)

include(AddDPCPPTrace)

add_subdirectory(tools)

add_subdirectory(src)
add_subdirectory(lib)

enable_testing()
add_subdirectory(benchmarks)
add_subdirectory(tests)
add_subdirectory(unittests)
