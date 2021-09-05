add_dpcpp_trace_library(trace_collector SHARED
  collector.cpp
  RecorderCommon.cpp
  RecorderPI.cpp
  RecorderOCL.cpp
)
target_link_libraries(trace_collector PRIVATE
  xptifw
  trace_proto
  utils
)
install(TARGETS trace_collector DESTINATION lib)

