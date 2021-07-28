add_dpcpp_trace_library(record_handler STATIC record_handler.cpp)

add_dpcpp_trace_library(plugin_record SHARED record.cpp)
install(TARGETS plugin_record DESTINATION lib)
