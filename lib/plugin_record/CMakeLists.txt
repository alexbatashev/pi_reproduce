add_library(record_handler STATIC record_handler.cpp)
target_include_directories(record_handler PUBLIC
  ${PROJECT_SOURCE_DIR}/include
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)
target_compile_options(record_handler PRIVATE -fPIC)

add_library(plugin_record SHARED record.cpp)
#TODO find a better way to link this stuff
target_link_directories(plugin_record PRIVATE ${INTEL_LLVM_PATH}/build/lib)
target_link_libraries(plugin_record PRIVATE record_handler xptifw)
install(TARGETS plugin_record DESTINATION lib)
