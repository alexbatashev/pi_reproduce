#pragma once

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
class AbstractDebugger {
public:
  virtual ~AbstractDebugger() = default;

  virtual void attach(uint64_t pid) = 0;
  virtual void detach() = 0;
};
} // namespace dpcpp_trace
