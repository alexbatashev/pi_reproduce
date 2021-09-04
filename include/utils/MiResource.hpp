#pragma once

#include <memory_resource>
#include <mimalloc.h>

namespace dpcpp_trace {
class MiResource : public std::pmr::memory_resource {
private:
  void *do_allocate(std::size_t bytes, std::size_t alignment) final {
    return mi_malloc_aligned(bytes, alignment);
  }
  void do_deallocate(void *p, std::size_t bytes, std::size_t alignment) final {
    mi_free_size_aligned(p, bytes, alignment);
  }
  bool do_is_equal(const std::pmr::memory_resource &) const noexcept final {
    // TODO figure out how to do equality in abscense of RTTI.
    return false;
  }
};

namespace detail {
MiResource *getMiResource();
}

template <typename T> auto makeMiResource() {
  return std::pmr::polymorphic_allocator<T>(detail::getMiResource());
}
} // namespace dpcpp_trace
