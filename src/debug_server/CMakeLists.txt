add_dpcpp_trace_library(debug_server STATIC
  server.cpp
  GDBServerProtocol.cpp
)

find_package(LLVM PATHS "${INTEL_LLVM_BINARY_PATH}" NO_DEFAULT_PATH
  NO_CMAKE_FIND_ROOT_PATH CONFIG REQUIRED)
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
llvm_map_components_to_libnames(LLVM_LIBRARIES support)

target_compile_options(debug_server PRIVATE -fno-rtti)
target_link_libraries(debug_server PUBLIC
  lldbUtility
  ${LLVM_LIBRARIES}
  CONAN_PKG::fmt
  CONAN_PKG::boost
  CONAN_PKG::ctre
)
target_include_directories(debug_server PRIVATE
  ${INTEL_LLVM_SOURCE_PATH}/lldb/include
  ${INTEL_LLVM_SOURCE_PATH}/llvm/include
  ${INTEL_LLVM_BINARY_PATH}/tools/lldb/include
  ${INTEL_LLVM_BINARY_PATH}/include
  ${INTEL_LLVM_SOURCE_PATH}/lldb/source
  )
