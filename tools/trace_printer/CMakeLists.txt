add_dpcpp_trace_executable(trace_printer main.cpp)

target_link_libraries(trace_printer PRIVATE
  utils
  trace_proto
  CONAN_PKG::andreasbuhr-cppcoro
  )
