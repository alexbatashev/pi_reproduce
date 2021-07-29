add_dpcpp_trace_library(trace_reader STATIC trace_reader.cpp)
target_include_directories(trace_reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(trace_reader PRIVATE
  ${PROJECT_SOURCE_DIR}/lib/trace_reader
)
