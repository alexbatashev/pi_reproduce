add_dpcpp_trace_library(graph_dump SHARED graph_dump.cpp)

target_link_libraries(graph_dump PRIVATE CONAN_PKG::nlohmann_json xptifw)

install(TARGETS graph_dump DESTINATION lib)
