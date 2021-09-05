#pragma once

#include <cppcoro/generator.hpp>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>
#include <memory>
#include <type_traits>
#include <google/protobuf/message.h>

#include "utils/MappedFile.hpp"

namespace dpcpp_trace {
template <typename T>
concept ProtobufMessage = std::is_base_of_v<google::protobuf::Message, T>;

template <ProtobufMessage T>
cppcoro::generator<T> protobuf_stream(std::filesystem::path path) {
  using namespace google::protobuf;

  MappedFile file{path};

  uint64_t size;
  while (file.readUInt64(size)) {
    T message;
    message.ParseFromArray(file.cur(), size);
    file.skip(size);
    co_yield message;
  }

  co_return;
}
}
