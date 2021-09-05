#pragma once

#include <filesystem>

namespace dpcpp_trace {
class MappedFile {
public:
  explicit MappedFile(std::filesystem::path path);
  ~MappedFile();

  uint8_t *begin();
  const uint8_t *begin() const;

  uint8_t *end();
  const uint8_t *end() const;

  size_t size() const noexcept { return mSize; }
  void *data() noexcept { return mPtr; }
  const void *data() const noexcept { return mPtr; }

  bool readUInt64(uint64_t &val);
  void *cur() noexcept;
  const void *cur() const noexcept;
  void skip(size_t numBytes) noexcept { mPos += numBytes; }

private:
  void *mPtr;
  size_t mSize;
  int mFileDescriptor;
  size_t mPos = 0;
};
} // namespace dpcpp_trace
