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
include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

include(AddDPCPPTrace)

add_subdirectory(tools)

add_subdirectory(src)
add_subdirectory(lib)

enable_testing()
add_subdirectory(benchmarks)
add_subdirectory(tests)
add_subdirectory(unittests)
