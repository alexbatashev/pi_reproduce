add_library(graph_dump SHARED graph_dump.cpp)

target_link_libraries(graph_dump PRIVATE nlohmann_json::nlohmann_json)

target_link_directories(graph_dump PRIVATE ${INTEL_LLVM_PATH}/build/lib)
target_link_libraries(graph_dump PRIVATE xptifw)
target_include_directories(graph_dump PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)
install(TARGETS graph_dump DESTINATION lib)
