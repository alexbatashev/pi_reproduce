#pragma once

#include <cstddef>
#include <cstdint>

// TODO this is a very rough implementation of RTTI. The reason this whole thing
// exists is that LLVM is built with RTTI off by default. The main drawback of
// this implementation is lack of ad-hoc support for class hierarchy. Some ideas
// on how to get it closer to vanilla C++ includes using consteval function with
// code_location argument in order to get stable type IDs.

namespace dpcpp_trace {
template <typename T, typename U> T *dyn_cast(U *src) {
  return static_cast<T *>(src->cast(T::getID()));
}

struct RTTIRoot {
  virtual void *cast(std::size_t type) = 0;
  virtual ~RTTIRoot() = default;
};

template <typename T> struct RTTIChild {
  static std::size_t getID() { return T::ID; }
};

enum class RTTIHierarchy : std::size_t {
  AbstractDebugger,
  Tracer,
  HostDebugger
};
} // namespace dpcpp_trace
