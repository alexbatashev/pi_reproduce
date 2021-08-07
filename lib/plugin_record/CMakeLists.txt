add_dpcpp_trace_library(record_handler STATIC record_handler.cpp)

add_dpcpp_trace_library(plugin_record SHARED record.cpp)
target_link_libraries(plugin_record PRIVATE record_handler xptifw)
install(TARGETS plugin_record DESTINATION lib)
