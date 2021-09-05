#include "utils/MappedFile.hpp"

#include <cstdint>
#include <fcntl.h>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace dpcpp_trace {
MappedFile::MappedFile(std::filesystem::path p) {
  mSize = fs::file_size(p);
  mFileDescriptor = open(p.c_str(), O_RDONLY, 0);
  if (mFileDescriptor == -1)
    throw std::runtime_error("Failed to open file");

  mPtr = mmap(nullptr, mSize, PROT_READ, MAP_PRIVATE | MAP_POPULATE,
              mFileDescriptor, 0);

  if (mPtr == nullptr)
    throw std::runtime_error("Failed to map file");
}

MappedFile::~MappedFile() {
  munmap(mPtr, mSize);
  close(mFileDescriptor);
}

uint8_t *MappedFile::begin() { return static_cast<uint8_t *>(mPtr); }
const uint8_t *MappedFile::begin() const {
  return static_cast<uint8_t *>(mPtr);
}

uint8_t *MappedFile::end() { return static_cast<uint8_t *>(mPtr) + mSize; }
const uint8_t *MappedFile::end() const {
  return static_cast<uint8_t *>(mPtr) + mSize;
}

bool MappedFile::readUInt64(uint64_t &val) {
  if (sizeof(uint64_t) + mPos >= mSize)
    return false;

  void *basePtr = static_cast<char *>(mPtr) + mPos;
  val = *static_cast<uint64_t*>(basePtr);
  mPos += sizeof(uint64_t);

  return true;
}

void *MappedFile::cur() noexcept {
  return (static_cast<char*>(mPtr) + mPos);
}

const void *MappedFile::cur() const noexcept {
  return (static_cast<const char*>(mPtr) + mPos);
}

} // namespace dpcpp_trace
