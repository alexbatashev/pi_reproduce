add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/fmt ${PROJECT_BINARY_DIR}/fmt)

add_dpcpp_trace_library(options STATIC options.cpp)

add_dpcpp_trace_executable(dpcpp_trace record.cpp replay.cpp print.cpp main.cpp)

target_link_libraries(dpcpp_trace PRIVATE options -ldl -lpthread fmt
  nlohmann_json::nlohmann_json
  trace_reader)

target_include_directories(dpcpp_trace PRIVATE
  ${PROJECT_SOURCE_DIR}/lib/trace_reader
)

install(TARGETS dpcpp_trace DESTINATION bin)
