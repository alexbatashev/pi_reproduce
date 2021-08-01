add_dpcpp_trace_library(debug_server STATIC
  server.cpp
)

find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(debug_server PUBLIC fmt::fmt Boost::boost Boost::system)
