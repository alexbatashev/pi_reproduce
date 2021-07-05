add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/fmt ${PROJECT_BINARY_DIR}/fmt)

set(CMAKE_BUILD_RPATH ${CMAKE_BINARY_DIR}/lib/plugin_record)
add_executable(dpcpp_trace record.cpp replay.cpp print.cpp options.cpp main.cpp)

target_link_libraries(dpcpp_trace PRIVATE -ldl -lpthread fmt)

target_include_directories(dpcpp_trace PRIVATE
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/source
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)

install(TARGETS dpcpp_trace DESTINATION bin)
