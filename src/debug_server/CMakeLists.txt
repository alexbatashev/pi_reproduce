add_dpcpp_trace_library(debug_server STATIC
  server.cpp
  GDBServerProtocol.cpp
)

target_compile_options(debug_server PRIVATE -fno-rtti)
target_link_libraries(debug_server PUBLIC
  lldbUtility
  ${LLVM_LIBRARIES}
  CONAN_PKG::fmt
  CONAN_PKG::boost
  CONAN_PKG::ctre
)
target_include_directories(debug_server SYSTEM PRIVATE
  ${INTEL_LLVM_SOURCE_PATH}/lldb/include
  ${INTEL_LLVM_SOURCE_PATH}/llvm/include
  ${INTEL_LLVM_BINARY_PATH}/tools/lldb/include
  ${INTEL_LLVM_BINARY_PATH}/include
  ${INTEL_LLVM_SOURCE_PATH}/lldb/source
  )
