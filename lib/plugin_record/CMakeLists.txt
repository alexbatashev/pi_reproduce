add_library(plugin_record SHARED record.cpp)
#TODO find a better way to link this stuff
target_link_directories(plugin_record PRIVATE ${INTEL_LLVM_PATH}/build/lib)
target_link_libraries(plugin_record PRIVATE xptifw)
target_include_directories(plugin_record PRIVATE
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)
