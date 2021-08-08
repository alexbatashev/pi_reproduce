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

private:
  void *mPtr;
  size_t mSize;
  int mFileDescriptor;
};
} // namespace dpcpp_trace
