if (BUILD_DEBUGGER)
  add_subdirectory(debug_server)
endif()

configure_file(config.hpp.in ${PROJECT_BINARY_DIR}/include/config.hpp)

add_dpcpp_trace_executable(dpcpp_trace
  record.cpp
  replay.cpp
  print.cpp
  main.cpp
  pack.cpp
  unpack.cpp
  $<$<BOOL:${BUILD_DEBUGGER}>:debug.cpp>
)

if (BUILD_DEBUGGER)
  target_link_libraries(dpcpp_trace PRIVATE debug_server)
endif()

target_link_libraries(dpcpp_trace PRIVATE
  utils
  -ldl
  -lpthread
  CONAN_PKG::fmt
  CONAN_PKG::nlohmann_json
  trace_proto
)

target_include_directories(dpcpp_trace PRIVATE
  ${PROJECT_SOURCE_DIR}/lib/trace_reader
)

install(TARGETS dpcpp_trace DESTINATION bin)
