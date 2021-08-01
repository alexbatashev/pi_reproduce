add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/fmt ${PROJECT_BINARY_DIR}/fmt)

if (BUILD_DEBUGGER)
  add_subdirectory(debug_server)
endif()

add_dpcpp_trace_library(options STATIC options.cpp)

add_dpcpp_trace_executable(dpcpp_trace
  record.cpp
  replay.cpp
  print.cpp
  main.cpp
  trace.cpp
  pack.cpp
  $<$<BOOL:${BUILD_DEBUGGER}>:debug.cpp>
)

if (BUILD_DEBUGGER)
  target_link_libraries(dpcpp_trace PRIVATE debug_server)
endif()

target_link_libraries(dpcpp_trace PRIVATE options -ldl -lpthread fmt
  nlohmann_json::nlohmann_json
  trace_reader)

target_include_directories(dpcpp_trace PRIVATE
  ${PROJECT_SOURCE_DIR}/lib/trace_reader
)

install(TARGETS dpcpp_trace DESTINATION bin)
