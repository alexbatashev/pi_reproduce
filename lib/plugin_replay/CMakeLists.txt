add_library(plugin_replay SHARED replay.cpp)

target_include_directories(plugin_replay PRIVATE
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include
)

