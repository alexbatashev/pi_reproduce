#pragma once

#include <ranges>

namespace dpcpp_trace {
template <typename T>
class MemoryView : public std::ranges::view_interface<MemoryView<T>> {
public:
  MemoryView() = default;
  MemoryView(T *ptr, size_t count) : mBegin(ptr), mEnd(ptr + count) {}

  auto begin() const { return mBegin; }
  auto end() const { return mEnd; }

private:
  T *mBegin;
  T *mEnd;
};
} // namespace dpcpp_trace
