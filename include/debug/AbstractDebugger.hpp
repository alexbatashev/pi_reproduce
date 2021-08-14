#pragma once

#include <cstdint>

class AbstractDebugger;

extern "C" {
struct DebuggerInfo {
  AbstractDebugger *debugger;
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
