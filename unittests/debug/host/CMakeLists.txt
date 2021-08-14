add_dpcpp_trace_executable(HostDebuggerTests
  main.cpp
  launch.cpp
  )
target_include_directories(HostDebuggerTests PRIVATE
  ${INTEL_LLVM_SOURCE_PATH}/lldb/include
  ${INTEL_LLVM_SOURCE_PATH}/llvm/include
  ${INTEL_LLVM_BINARY_PATH}/tools/lldb/include
  ${INTEL_LLVM_BINARY_PATH}/include
  ${INTEL_LLVM_SOURCE_PATH}/lldb/source
  )
target_link_libraries(HostDebuggerTests PRIVATE utils Catch2::Catch2 host_debugger)
target_include_directories(HostDebuggerTests PRIVATE
  ${PROJECT_SOURCE_DIR}/lib/debug/host)
catch_discover_tests(HostDebuggerTests)

