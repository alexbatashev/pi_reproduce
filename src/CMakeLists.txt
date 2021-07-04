set(CMAKE_BUILD_RPATH ${CMAKE_BUILD_DIR} / lib / plugin_record)
add_executable(prp record.cpp replay.cpp print.cpp options.cpp main.cpp)

target_link_libraries(prp PRIVATE -ldl -lpthread)

target_include_directories(prp PRIVATE
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/source
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)
