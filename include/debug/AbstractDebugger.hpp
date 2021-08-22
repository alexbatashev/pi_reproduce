#pragma once

#include "utils/RTTI.hpp"

#include <cstdint>
#include <memory>
#include <vector>

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
struct StopReason {
  enum class Type {
    invalid,
    none,
    trace,
    breakpoint,
    watchpoint,
    signal,
    exception,
    exec,
    fork,
    vfork,
    vfork_done,
    exit
  };

  Type type = Type::invalid;
  int code = 0;
};

class AbstractDebugger : public RTTIRoot, public RTTIChild<AbstractDebugger> {
public:
  constexpr static std::size_t ID =
      static_cast<size_t>(RTTIHierarchy::AbstractDebugger);

  virtual ~AbstractDebugger() = default;

  virtual std::vector<uint8_t> getRegistersData(size_t threadIdx) = 0;

  virtual std::string getGDBTargetXML() = 0;
  virtual std::string getExecutablePath() = 0;

  virtual void attach(uint64_t pid) = 0;
  virtual bool isAttached() = 0;
  virtual void detach() = 0;

  virtual size_t getNumThreads() = 0;
  virtual uint64_t getThreadIDAtIndex(size_t threadIdx) = 0;

  virtual StopReason getStopReason(size_t threadId) = 0;

  virtual void CreateSWBreakpoint(uint64_t address) = 0;
  virtual void resume(int signal = 0, uint64_t tid = 0) = 0;

  virtual std::vector<uint8_t> readMemory(uint64_t addr, size_t len) = 0;

  virtual std::vector<uint8_t> getAuxvData() = 0;

  void *cast(std::size_t type) override {
    if (type == RTTIChild<AbstractDebugger>::getID()) {
      return this;
    }
    return nullptr;
  }
};

} // namespace dpcpp_trace
