add_library(trace_reader STATIC trace_reader.cpp)
target_include_directories(trace_reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(trace_reader PRIVATE -fPIC)

target_include_directories(trace_reader PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/lib/trace_reader
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/source
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)
