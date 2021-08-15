add_dpcpp_trace_library(debug_server STATIC
  server.cpp
)

target_link_libraries(debug_server PUBLIC CONAN_PKG::fmt CONAN_PKG::boost)
