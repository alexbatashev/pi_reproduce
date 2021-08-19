#pragma once

#include "utils/RTTI.hpp"

#include <cstdint>
#include <memory>

namespace dpcpp_trace {
class AbstractDebugger;
}

extern "C" {
struct DebuggerInfo {
  dpcpp_trace::AbstractDebugger *debugger;
  void (*deinitialize)(dpcpp_trace::AbstractDebugger *);
};
int initialize(DebuggerInfo *);
}

namespace dpcpp_trace {
class AbstractDebugger : public RTTIRoot, public RTTIChild<AbstractDebugger> {
public:
  constexpr static std::size_t ID =
      static_cast<size_t>(RTTIHierarchy::AbstractDebugger);

  virtual ~AbstractDebugger() = default;

  virtual void attach(uint64_t pid) = 0;
  virtual bool isAttached() = 0;
  virtual void detach() = 0;

  void *cast(std::size_t type) override {
    if (type == RTTIChild<AbstractDebugger>::getID()) {
      return this;
    }
    return nullptr;
  }
};

} // namespace dpcpp_trace
