#include "utils/Compression.hpp"

#include <cassert>
#include <zstd.h>

namespace dpcpp_trace {
namespace detail {
class CompressionImpl {
public:
  CompressionImpl() {
    mCompContext = ZSTD_createCCtx();
    mDecContext = ZSTD_createDCtx();
  }

  ~CompressionImpl() {
    ZSTD_freeCCtx(mCompContext);
    ZSTD_freeDCtx(mDecContext);
  }

  ZSTD_CCtx &getCompRef() { return *mCompContext; }

  ZSTD_DCtx &getDecRef() { return *mDecContext; }

private:
  ZSTD_CCtx *mCompContext;
  ZSTD_DCtx *mDecContext;
};
} // namespace detail

Compression::Compression() {
  mImpl = std::make_shared<detail::CompressionImpl>();
}

Buffer Compression::compress(const void *ptr, size_t size) {
  const size_t maxSize = ZSTD_compressBound(size);
  Buffer buf{maxSize};

  const size_t compSize = ZSTD_compressCCtx(&mImpl->getCompRef(), buf.data(),
                                            maxSize, ptr, size, 3);

  buf.resize(compSize);

  return buf;
}

Buffer Compression::uncompress(const void *ptr, size_t size) {
  const size_t maxSize = ZSTD_getFrameContentSize(ptr, size);
  Buffer buf{maxSize};

  const size_t decSize =
      ZSTD_decompressDCtx(&mImpl->getDecRef(), buf.data(), maxSize, ptr, size);
  assert(maxSize == decSize);

  return buf;
}
} // namespace dpcpp_trace
