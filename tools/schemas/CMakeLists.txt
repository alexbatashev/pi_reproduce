protobuf_generate_cpp(DPCPP_PROTO_SRC DPCPP_PROTO_HDRS
  api_call.proto
  device_binary.proto
)

add_dpcpp_trace_library(trace_proto STATIC ${DPCPP_PROTO_SRC} ${DPCPP_PROTO_HDRS})
target_include_directories(trace_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(trace_proto PUBLIC CONAN_PKG::protobuf)
