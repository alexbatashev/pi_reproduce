add_dpcpp_trace_library(plugin_replay SHARED replay.cpp)

target_link_libraries(plugin_replay PRIVATE -lpthread trace_proto)
install(TARGETS plugin_replay DESTINATION lib)
