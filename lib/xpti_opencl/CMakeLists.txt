add_dpcpp_trace_library(ocl_xpti_layer MODULE layer.cpp)
target_link_libraries(ocl_xpti_layer PRIVATE xpti)
