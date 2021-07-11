add_library(plugin_replay SHARED replay.cpp)

target_link_libraries(plugin_replay PRIVATE -lpthread trace_reader)
target_compile_options(plugin_replay PRIVATE -fno-omit-frame-pointer)
target_include_directories(plugin_replay PRIVATE
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)
install(TARGETS plugin_replay DESTINATION lib)
