add_dpcpp_trace_library(debug_server STATIC
  server.cpp
)

set(Boost_USE_MULTITHREADED ON)
if (DPCPP_TRACE_LINK_STATICALLY)
  set(Boost_USE_STATIC_RUNTIME ON)
  set(Boost_USE_STATIC_LIBS ON)
endif()

find_package(Boost REQUIRED COMPONENTS system)
target_link_libraries(debug_server PUBLIC fmt::fmt Boost::boost Boost::system)
