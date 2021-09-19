add_dpcpp_trace_library(graph_dump MODULE graph_dump.cpp)

target_link_libraries(graph_dump PRIVATE trace_proto xptifw)

install(TARGETS graph_dump DESTINATION lib)
