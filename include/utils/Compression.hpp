#pragma once

#include "utils/Buffer.hpp"

#include <iostream>
#include <memory>
#include <ranges>

namespace dpcpp_trace {
namespace detail {
class CompressionImpl;
}
class Compression {
public:
  Compression();

  Buffer compress(std::ranges::contiguous_range auto in) {
    return std::move(
        compress(std::ranges::data(in),
                 std::ranges::size(in) *
                     sizeof(std::ranges::range_value_t<decltype(in)>)));
  }

  Buffer uncompress(std::ranges::contiguous_range auto in) {
    return std::move(
        uncompress(std::ranges::data(in),
                   std::ranges::size(in) *
                       sizeof(std::ranges::range_value_t<decltype(in)>)));
  }

private:
  Buffer compress(const void *ptr, size_t size);
  Buffer uncompress(const void *ptr, size_t size);

  std::shared_ptr<detail::CompressionImpl> mImpl;
};
} // namespace dpcpp_trace
