#pragma once

#include <cstdint>
#include <vector>

namespace dpcpp_trace {
class Buffer {
public:
  explicit Buffer(size_t size) { mData.resize(size); }

  Buffer(const Buffer &) = delete;
  Buffer(Buffer &&) = default;
  Buffer &operator=(const Buffer &) = delete;
  Buffer &operator=(Buffer &&) = default;

  uint8_t *data() noexcept { return mData.data(); }
  const uint8_t *data() const noexcept { return mData.data(); }

  template <typename T> T *as() noexcept {
    return reinterpret_cast<T *>(mData.data());
  }
  template <typename T> const T *as() const noexcept {
    return reinterpret_cast<const T *>(mData.data());
  }

  size_t size() const noexcept { return mData.size(); }

  void resize(size_t size) { mData.resize(size); }

private:
  std::vector<uint8_t> mData;
};
} // namespace dpcpp_trace
