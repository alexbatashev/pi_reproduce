add_library(graph_dump SHARED graph_dump.cpp)

include(FetchContent)

FetchContent_Declare(json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.7.3)

FetchContent_GetProperties(json)
if(NOT json_POPULATED)
  FetchContent_Populate(json)
  add_subdirectory(${json_SOURCE_DIR} ${json_BINARY_DIR} EXCLUDE_FROM_ALL)
endif()

target_link_libraries(graph_dump PRIVATE nlohmann_json::nlohmann_json)

target_link_directories(graph_dump PRIVATE ${INTEL_LLVM_PATH}/build/lib)
target_link_libraries(graph_dump PRIVATE xptifw)
target_include_directories(graph_dump PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${INTEL_LLVM_PATH}/sycl/include
  ${INTEL_LLVM_PATH}/sycl/tools/xpti_helpers
  ${INTEL_LLVM_PATH}/xpti/include)
install(TARGETS graph_dump DESTINATION lib)
